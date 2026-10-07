#!/usr/bin/env python3
"""Translate current QLog release additions since --from, after running lupdate."""

import argparse
from contextlib import contextmanager
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from catalog import LANGUAGES, patch_ts, read_ts, select_new
from localize import Codex, DEFAULT_BUDGET, Memory, Sources, json_loads, make_batches, progress, translate_batch


def atomic_write(path, content, expected=None):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix='.' + path.name + '-', dir=path.parent)
    try:
        if path.exists():
            os.chmod(temporary, stat.S_IMODE(path.stat().st_mode))
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        if expected is not None and path.read_bytes() != expected:
            raise ValueError('Catalog changed while translating: ' + str(path))
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def write_report(path, report):
    atomic_write(path, (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))


def git(root, *arguments):
    result = subprocess.run(['git', '-C', str(root), *arguments], capture_output=True)
    if result.returncode:
        raise ValueError('Git failed: ' + result.stderr.decode('utf-8', errors='replace').strip())
    return result.stdout


def source_paths(root):
    paths = git(root, 'ls-files', '-z', '--cached', '--others', '--exclude-standard')
    suffixes = {'.cpp', '.h', '.c', '.hpp', '.ui', '.tri', '.qml', '.js', '.pro', '.pri', '.qrc'}
    excluded = ('build/', 'tests/', 'devtools/', '3rdparty/')
    return sorted({os.fsdecode(path) for path in paths.split(b'\0') if path
                   and not os.fsdecode(path).startswith(excluded)
                   and Path(os.fsdecode(path)).suffix in suffixes
                   and (root / os.fsdecode(path)).is_file()})


def source_signature(root, paths):
    result = {}
    for path in paths:
        target = (root / path).resolve()
        if not target.is_relative_to(root):
            raise ValueError('Project source escapes QLog: ' + path)
        result[path] = hashlib.sha256(target.read_bytes()).hexdigest()
    return result


def update_catalogs(root, executable):
    executable = os.path.abspath(executable) if os.path.dirname(executable) else str(executable)
    help_result = subprocess.run([str(executable), '-help'], cwd=root, capture_output=True, text=True)
    if help_result.returncode:
        raise ValueError(f'lupdate failed with exit status {help_result.returncode}; translation was not started')
    supported = re.search(r'-disable-heuristic\s+\{([^}]+)\}', help_result.stdout)
    if not supported:
        raise ValueError('Cannot determine supported lupdate heuristics from -help')
    argv = [str(executable), str(root / 'QLog.pro'), '-no-obsolete', '-locations', 'absolute']
    for heuristic in ('similartext', 'sametext', 'number'):
        if heuristic in supported.group(1).split('|'):
            argv.extend(['-disable-heuristic', heuristic])
    print('Updating current QLog catalogs with lupdate', flush=True)
    result = subprocess.run(argv, cwd=root)
    if result.returncode:
        raise ValueError(f'lupdate failed with exit status {result.returncode}; translation was not started')


def check_catalog(raw, executable):
    """Compile a candidate TS with Qt, without creating QM files in the checkout."""
    executable = os.path.abspath(executable) if os.path.dirname(executable) else str(executable)
    with tempfile.TemporaryDirectory(prefix='qlog-ts-check-') as temporary:
        path = Path(temporary)
        source, compiled = path / 'candidate.ts', path / 'candidate.qm'
        source.write_bytes(raw)
        result = subprocess.run([str(executable), '-silent', str(source), '-qm', str(compiled)],
                                cwd=path, capture_output=True, text=True)
        if result.returncode or not compiled.is_file():
            raise ValueError('lrelease rejected TS: ' + (result.stderr + result.stdout)[-2000:])


@contextmanager
def _repository_lock(root):
    path = root / 'build/qlog-localize.lock'
    path.parent.mkdir(parents=True, exist_ok=True)
    # Keep the file in place so all processes lock the same file, even after a run ends.
    with path.open('a+b') as stream:
        try:
            if os.name == 'nt':
                import msvcrt
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            if error.errno in (errno.EACCES, errno.EAGAIN, errno.EDEADLK):
                raise ValueError('QLog localization is already running in this checkout; wait for it to finish.') from error
            raise
        yield


def run(root, baseline, provider, lupdate='lupdate', budget=DEFAULT_BUDGET, glossary=None, report_path=None, lrelease='lrelease', languages=None):
    root = Path(root).resolve()
    if not (root / 'QLog.pro').is_file() or Path(os.fsdecode(git(root, 'rev-parse', '--show-toplevel')).strip()).resolve() != root:
        raise ValueError('Run the tool installed in the QLog repository')
    with _repository_lock(root):
        return _run_locked(root, baseline, provider, lupdate, budget, glossary, report_path, lrelease, languages)


def _run_locked(root, baseline, provider, lupdate, budget, glossary, report_path, lrelease, languages):
    languages = list(dict.fromkeys(languages)) if languages is not None else list(LANGUAGES)
    if not languages or any(language not in LANGUAGES for language in languages):
        raise ValueError('Choose at least one supported language: ' + ', '.join(LANGUAGES))
    if budget < 8000:
        raise ValueError('--prompt-budget must be at least 8000 UTF-8 bytes')
    # Resolve the ref before mutating anything; a missing ref must never mean all texts.
    commit = git(root, 'rev-parse', '--verify', '--end-of-options', baseline + '^{commit}').decode('ascii').strip()
    overrides = json_loads(Path(glossary).read_text(encoding='utf-8')) if glossary else {}
    if not isinstance(overrides, dict) or any(language not in LANGUAGES for language in overrides):
        raise ValueError('Glossary must map supported language codes to source/target dictionaries')
    for values in overrides.values():
        if not isinstance(values, dict) or any(not isinstance(key, str) or not key.strip()
                                              or not isinstance(value, str) or not value.strip()
                                              for key, value in values.items()):
            raise ValueError('Glossary labels and translations must be nonempty strings')
    report_path = Path(report_path).resolve() if report_path else root / 'build/qlog-localize-report.json'
    memory_path = (root / 'devtools/translator/memory.json').resolve()
    if report_path.suffix != '.json' or report_path.is_relative_to(root / 'i18n') or report_path == memory_path:
        raise ValueError('--report must name a JSON file outside i18n and translation memory')
    report = {'from': baseline, 'base_commit': commit, 'status': 'running',
              'translated': 0, 'reused': 0, 'languages': {}, 'needs_review': [], 'warnings': [],
              'requested_languages': languages, 'model_requests': 0}
    write_report(report_path, report)

    def model_request(prompt):
        report['model_requests'] += 1
        write_report(report_path, report)
        return provider(prompt)

    progress(f'Baseline {baseline} ({commit[:12]}); target languages: {", ".join(languages)}')
    try:
        memory_raw = memory_path.read_bytes() if memory_path.exists() else None
        saved = json_loads(memory_raw.decode('utf-8')) if memory_raw is not None else {'version': 1, 'languages': {}}
        if (not isinstance(saved, dict) or set(saved) != {'version', 'languages'} or type(saved['version']) is not int
                or saved['version'] != 1 or not isinstance(saved['languages'], dict)
                or any(language not in LANGUAGES for language in saved['languages'])):
            raise ValueError('Invalid devtools/translator/memory.json')
        update_catalogs(root, lupdate)
        catalogs = {}
        for language in languages:
            relative = f'i18n/qlog_{language}.ts'
            previous = read_ts(git(root, 'show', commit + ':' + relative), language)
            raw = (root / relative).read_bytes()
            current = read_ts(raw, language)
            progress(f'{language}: checking updated TS with lrelease')
            check_catalog(raw, lrelease)
            catalogs[language] = (raw, current, previous)
        paths = source_paths(root)
        signature = source_signature(root, paths)
        sources = Sources(root, paths)
        progress(f'Source context is available from {len(paths)} current QLog files')
        for language, (raw, current, previous) in catalogs.items():
            selected = select_new(current, previous)
            old_phrases = {message.phrase for message in previous}
            summary = {'selected': len(selected), 'translated': 0, 'reused': 0,
                       'excluded_backlog': sum(message.empty and message.phrase in old_phrases for message in current),
                       'preserved_drafts': sum(message.state == 'unfinished' and any(message.forms)
                                              and message.phrase not in old_phrases for message in current)}
            report['languages'][language] = summary
            print(f'{language}: {len(selected)} release additions; {summary["excluded_backlog"]} old empty messages excluded', flush=True)
            # Baseline memory retains approved terms removed by lupdate -no-obsolete.
            memory = Memory(current, overrides.get(language, {}), saved['languages'].get(language))
            current_ids = {message.id for message in current}
            for message in previous:
                if message.id not in current_ids and message.approved:
                    memory.add(message, message.forms)
            known = {message.id: message for message in selected}
            destination = root / f'i18n/qlog_{language}.ts'

            def commit_batch(rows, terms=(), direct=False):
                nonlocal raw, memory_raw
                replacements = {ident: row['forms'] for ident, row in rows.items() if not row['needs_review']}
                for ident, row in rows.items():
                    if row['needs_review']:
                        message = known[ident]
                        report['needs_review'].append({'language': language, 'source': message.source,
                                                       'context': message.context, **row})
                if replacements:
                    result = patch_ts(raw, replacements, language)
                    progress(f'{language}: validating {len(replacements)} translations with lrelease before writing')
                    check_catalog(result, lrelease)
                    if source_paths(root) != paths or source_signature(root, paths) != signature:
                        raise ValueError('QLog sources changed while translating; rerun against the current state')
                    for ident, forms in replacements.items():
                        memory.add(known[ident], forms, reviewed=True)
                    memory.add_terms(terms, [known[ident] for ident in replacements])
                    atomic_write(destination, result, expected=raw)
                    progress(f'Wrote i18n/qlog_{language}.ts: {len(replacements)} translations '
                             f'({"reused" if direct else "reviewed"})')
                    raw = result
                    summary['translated'] += len(replacements)
                    report['translated'] += len(replacements)
                    if direct:
                        summary['reused'] += len(replacements)
                        report['reused'] += len(replacements)
                    saved['languages'][language] = memory.snapshot()
                    memory_content = (json.dumps(saved, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
                    atomic_write(memory_path, memory_content, expected=memory_raw)
                    memory_raw = memory_content
                write_report(report_path, report)
                progress(f'{language}: saved progress; {summary["translated"]} written, '
                         f'{sum(row["needs_review"] for row in rows.values())} unresolved in this batch; '
                         f'{report["model_requests"]} model requests so far')

            for batch in make_batches(selected, memory, language, budget):
                direct, pending = {}, []
                for message in batch:
                    reused = memory.reuse(message, language)
                    if reused:
                        direct[message.id] = {'id': message.id, 'forms': reused,
                                              'needs_review': False, 'reason': 'Reused a phrase with matching meaning evidence'}
                    else:
                        pending.append(message)
                if direct:
                    commit_batch(direct, direct=True)
                if not pending:
                    continue
                print(f'Localizing {language}: {len(pending)} messages in {len({message.context for message in pending})} contexts', flush=True)
                rows, terms = translate_batch(pending, memory, language, sources, model_request, budget, warnings=report['warnings'])
                commit_batch(rows, terms)
        report['status'] = 'needs_review' if report['needs_review'] else 'complete'
    except BaseException as error:
        report['status'] = 'interrupted' if isinstance(error, KeyboardInterrupt) else 'failed'
        report['error'] = str(error)
        write_report(report_path, report)
        raise
    write_report(report_path, report)
    print(f'Wrote {report["translated"]} translations ({report["reused"]} reused); '
          f'{len(report["needs_review"])} require review; {report["model_requests"]} model requests. Report: {report_path}', flush=True)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--from', dest='baseline', required=True, help='Previous release tag or commit; old unfinished texts are excluded')
    parser.add_argument('--language', action='append', choices=LANGUAGES, help='Translate only this language; repeat to select several. Default: all supported languages')
    parser.add_argument('--lupdate', default='lupdate', help='lupdate executable from your Qt kit')
    parser.add_argument('--lrelease', default='lrelease', help='lrelease executable from your Qt kit; validates TS before writing')
    parser.add_argument('--model', help='Optional Codex model override; otherwise use your configured model')
    parser.add_argument('--timeout', type=int, default=600, help='Timeout per Codex request in seconds')
    parser.add_argument('--prompt-budget', type=int, default=DEFAULT_BUDGET, help='Maximum input JSON size in UTF-8 bytes')
    parser.add_argument('--glossary', type=Path, help='Optional JSON: language -> source label -> mandatory target label')
    parser.add_argument('--report', type=Path, help='Report JSON, default build/qlog-localize-report.json')
    args = parser.parse_args(argv)
    if args.timeout < 1:
        parser.error('--timeout must be positive')
    try:
        run(Path(__file__).resolve().parents[2], args.baseline, Codex(args.model, args.timeout),
            args.lupdate, args.prompt_budget, args.glossary, args.report, args.lrelease, languages=args.language)
    except KeyboardInterrupt:
        print('Interrupted. Completed translations remain in TS; rerun the same command to continue.', file=sys.stderr)
        return 130
    except (OSError, ValueError, ET.ParseError, subprocess.SubprocessError) as error:
        print('Error: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
