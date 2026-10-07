import json
import contextlib
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_catalog import message, ts
import catalog

try:
    import qlog_localize as tool
except ModuleNotFoundError:
    tool = None


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(tool, 'The single-command release tool is not implemented')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'i18n').mkdir()
        (self.root / 'ui').mkdir()
        (self.root / 'ui/panel.cpp').write_text('tr("Old");\n', encoding='utf-8')
        (self.root / 'QLog.pro').write_text('SOURCES = ui/panel.cpp\n', encoding='utf-8')
        self.seed(message('Old'))
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        self.git('add', '.')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'release')
        self.git('tag', 'release')

    def git(self, *args):
        return subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True)

    def seed(self, body):
        for lang in catalog.LANGUAGES:
            (self.root / 'i18n' / f'qlog_{lang}.ts').write_bytes(ts(body, lang))

    def provider(self, prompt):
        return {'translations': [{'id': item['id'], 'forms': ['Nové %1'],
                                  'needs_review': False, 'reason': ''} for item in prompt['messages']],
                'source_requests': [], 'terms': []}

    def test_parallel_runs_are_rejected_before_writes_and_lock_releases_on_exit(self):
        script = '''
import contextlib
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import qlog_localize as tool
def pause(root, executable):
    print('READY', file=sys.__stdout__, flush=True)
    sys.stdin.read(1)
tool.update_catalogs = pause
with contextlib.redirect_stdout(sys.stderr):
    tool.run(Path(sys.argv[2]), 'release', None, languages=['cs'])
'''
        memory_path = self.root / 'devtools/translator/memory.json'
        memory_path.parent.mkdir(parents=True)
        memory_path.write_text('{"version":1,"languages":{}}', encoding='utf-8')
        report_path = self.root / 'build/qlog-localize-report.json'
        for killed in (False, True):
            with self.subTest(killed=killed):
                process = subprocess.Popen([sys.executable, '-u', '-c', script,
                                            str(Path(tool.__file__).parent), str(self.root)],
                                           stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE, text=True)
                try:
                    self.assertEqual(process.stdout.readline().strip(), 'READY')
                    paths = [*self.root.glob('i18n/*.ts'), memory_path, report_path]
                    before = {path: path.read_bytes() for path in paths}
                    alternate_report = self.root / 'other-report.json'
                    with patch('qlog_localize.update_catalogs', side_effect=ValueError('lupdate must not run')):
                        for report in (None, alternate_report):
                            with self.assertRaisesRegex(ValueError, 'already running'):
                                tool.run(self.root, 'release', self.provider, languages=['de'], report_path=report)
                    self.assertEqual({path: path.read_bytes() for path in paths}, before)
                    self.assertFalse(alternate_report.exists())
                    if killed:
                        process.kill()
                        process.communicate(timeout=10)
                    else:
                        _, errors = process.communicate(input='\n', timeout=10)
                        self.assertEqual(process.returncode, 0, errors)
                finally:
                    if process.poll() is None:
                        process.kill()
                    process.communicate(timeout=10)
                with patch('qlog_localize.update_catalogs'):
                    report = tool.run(self.root, 'release', self.provider, languages=['de'])
                self.assertEqual(report['status'], 'complete')

    def test_selected_language_only_calls_model_and_writes_its_catalog(self):
        self.updated()
        before = {lang: (self.root / f'i18n/qlog_{lang}.ts').read_bytes() for lang in catalog.LANGUAGES}
        seen = []
        def provider(prompt):
            seen.append(prompt['language'])
            return self.provider(prompt)
        output = io.StringIO()
        with patch('qlog_localize.update_catalogs'), contextlib.redirect_stdout(output):
            report = tool.run(self.root, 'release', provider, languages=['cs'])
        self.assertEqual(seen, ['Czech', 'Czech'])
        self.assertEqual(set(report['languages']), {'cs'})
        self.assertEqual(report['model_requests'], 2)
        self.assertIn('translate', output.getvalue())
        self.assertIn('review', output.getvalue())
        self.assertIn('Wrote i18n/qlog_cs.ts', output.getvalue())
        for lang in catalog.LANGUAGES.keys() - {'cs'}:
            self.assertEqual((self.root / f'i18n/qlog_{lang}.ts').read_bytes(), before[lang])

    def test_invalid_language_is_rejected_before_lupdate(self):
        with patch('qlog_localize.update_catalogs') as update:
            with self.assertRaises(ValueError):
                tool.run(self.root, 'release', self.provider, languages=['unknown'])
        update.assert_not_called()

    def test_chinese_translates_new_messages_and_reuses_terminology_on_rerun(self):
        def updated(*args):
            (self.root / 'i18n/qlog_zh_CN.ts').write_bytes(ts(
                message('Old') + message('Callsign %1') + message('%n QSO', numerus=True), 'zh_CN'))
        seen = []
        def provider(prompt):
            seen.append(prompt)
            return {'translations': [{'id': row['id'], 'forms': [
                '%n 个 QSO' if row['numerus'] else '呼号 %1'], 'needs_review': False, 'reason': ''}
                for row in prompt['messages']], 'source_requests': [], 'terms': [{
                    'source': 'Callsign', 'meaning': 'Amateur station identifier', 'target': '呼号',
                    'message_ids': [row['id'] for row in prompt['messages'] if row['source'] == 'Callsign %1']}]}
        with patch('qlog_localize.update_catalogs', side_effect=updated):
            report = tool.run(self.root, 'HEAD', provider, languages=['zh_CN'])
        self.assertEqual(report['translated'], 2)
        self.assertEqual([row['plural_forms'] for row in seen], [1, 1])
        self.assertTrue(all('Simplified Chinese' in row['language'] for row in seen))
        path = self.root / 'i18n/qlog_zh_CN.ts'
        rows = catalog.read_ts(path.read_bytes(), 'zh_CN')
        self.assertEqual([row.forms for row in rows], [('',), ('呼号 %1',), ('%n 个 QSO',)])
        saved = json.loads((self.root / 'devtools/translator/memory.json').read_text())
        self.assertEqual(len(saved['languages']['zh_CN']['reviewed']), 2)
        path.write_bytes(path.read_bytes().replace(b'</context>', message('Callsign today').encode() + b'</context>'))
        def next_provider(prompt):
            self.assertIn({'source': 'Callsign', 'meaning': 'Amateur station identifier',
                           'target': '呼号', 'mandatory': False}, prompt['terminology'])
            return {'translations': [{'id': row['id'], 'forms': ['今日呼号'], 'needs_review': False,
                                      'reason': ''} for row in prompt['messages']], 'source_requests': [], 'terms': []}
        with patch('qlog_localize.update_catalogs'):
            report = tool.run(self.root, 'HEAD', next_provider, languages=['zh_CN'])
        self.assertEqual(report['translated'], 1)

    def updated(self, *args, **kwargs):
        self.seed(message('Old') + message('New %1') + message('Draft', 'Manual draft'))
        return type('Result', (), {'returncode': 0})()

    def test_lupdate_runs_first_with_agreed_options_and_translates_only_new_messages(self):
        invocations = []
        real_run = subprocess.run
        def runner(argv, **kwargs):
            if argv[0] == 'lupdate-test':
                if argv[1:] == ['-help']:
                    return type('Result', (), {'returncode': 0, 'stdout': '-disable-heuristic {sametext|similartext|number}'})()
                invocations.append(argv)
                return self.updated()
            return real_run(argv, **kwargs)
        def provider(prompt):
            self.assertTrue(invocations)
            self.assertEqual([item['source'] for item in prompt['messages']], ['New %1'])
            return self.provider(prompt)
        with patch('qlog_localize.subprocess.run', side_effect=runner):
            report = tool.run(self.root, 'release', provider, lupdate='lupdate-test')
        self.assertEqual(report['translated'], len(catalog.LANGUAGES))
        self.assertEqual(set(invocations[0][2:]), {'-no-obsolete', '-locations', 'absolute',
                         '-disable-heuristic', 'similartext', 'sametext', 'number'})
        for lang in catalog.LANGUAGES:
            rows = catalog.read_ts((self.root / f'i18n/qlog_{lang}.ts').read_bytes(), lang)
            self.assertEqual(rows[0].forms, ('',))
            self.assertEqual(rows[1].forms, ('Nové %1',))
            self.assertEqual(rows[1].state, 'unfinished')
            self.assertEqual(rows[2].forms, ('Manual draft',))

    def test_qt6_without_number_heuristic_uses_supported_options(self):
        invocations = []
        def runner(argv, **kwargs):
            invocations.append(argv)
            return type('Result', (), {'returncode': 0, 'stdout': '-disable-heuristic {sametext|similartext}'})()
        with patch('qlog_localize.subprocess.run', side_effect=runner):
            tool.update_catalogs(self.root, 'lupdate-test')
        self.assertNotIn('number', invocations[-1])
        self.assertIn('sametext', invocations[-1])
        self.assertIn('similartext', invocations[-1])

    def test_later_batch_reuses_a_newly_reviewed_phrase_without_model_calls(self):
        def updated(*args):
            for lang in catalog.LANGUAGES:
                data = ts(message('New status', extra='<extracomment>QSO status</extracomment>'), lang, 'First')
                second = ts(message('New status', extra='<extracomment>QSO status</extracomment>'), lang, 'Second')
                context = second[second.index(b'<context>'):second.index(b'</context>') + len(b'</context>')]
                (self.root / f'i18n/qlog_{lang}.ts').write_bytes(data.replace(b'</TS>', context + b'</TS>'))
        calls = []
        def provider(prompt):
            calls.append(prompt['language'])
            return {'translations': [{'id': row['id'], 'forms': ['New translation'], 'needs_review': False, 'reason': ''}
                                     for row in prompt['messages']], 'source_requests': [], 'terms': []}
        with patch('qlog_localize.update_catalogs', side_effect=updated), patch(
                'qlog_localize.make_batches', side_effect=lambda messages, *args: ([row] for row in messages)):
            report = tool.run(self.root, 'release', provider)
        self.assertEqual(len(calls), 2 * len(catalog.LANGUAGES))
        self.assertEqual(report['reused'], len(catalog.LANGUAGES))
        for lang in catalog.LANGUAGES:
            rows = catalog.read_ts((self.root / f'i18n/qlog_{lang}.ts').read_bytes(), lang)
            self.assertEqual([row.forms for row in rows], [('New translation',), ('New translation',)])

    def test_saved_terminology_is_available_on_the_next_run(self):
        def updated(*args):
            self.seed(message('Worked'))
        def provider(prompt):
            return {'translations': [{'id': row['id'], 'forms': ['Pracováno'], 'needs_review': False, 'reason': ''}
                                     for row in prompt['messages']], 'source_requests': [],
                    'terms': [{'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno',
                               'message_ids': [row['id'] for row in prompt['messages']]}]}
        with patch('qlog_localize.update_catalogs', side_effect=updated):
            tool.run(self.root, 'release', provider)
        memory_path = self.root / 'devtools/translator/memory.json'
        self.assertTrue(memory_path.is_file())
        for lang in catalog.LANGUAGES:
            raw = (self.root / f'i18n/qlog_{lang}.ts').read_bytes()
            extra = message('Worked today').encode()
            (self.root / f'i18n/qlog_{lang}.ts').write_bytes(raw.replace(b'</context>', extra + b'</context>'))
        seen = []
        def next_provider(prompt):
            seen.append(prompt['terminology'])
            return {'translations': [{'id': row['id'], 'forms': ['Pracováno dnes'], 'needs_review': False, 'reason': ''}
                                     for row in prompt['messages']], 'source_requests': [], 'terms': []}
        with patch('qlog_localize.update_catalogs'):
            tool.run(self.root, 'release', next_provider)
        self.assertEqual(len(seen), 2 * len(catalog.LANGUAGES))
        self.assertTrue(all(any(term['target'] == 'Pracováno' for term in terms) for terms in seen))

    def test_qt_rejects_a_well_formed_xml_file_with_invalid_ts_structure(self):
        executable = shutil.which('lrelease')
        if not executable:
            self.skipTest('Qt lrelease unavailable')
        raw = b'<TS language="cs"><unexpected/></TS>'
        with self.assertRaisesRegex(ValueError, 'lrelease'):
            tool.check_catalog(raw, executable)
        tool.check_catalog(ts(message('Files (*)', 'Soubory (*)')), executable)
        self.assertEqual(list(self.root.rglob('*.qm')), [])

    def test_qt_validation_failure_preserves_the_destination(self):
        self.updated()
        original = (self.root / 'i18n/qlog_cs.ts').read_bytes()
        def validate(raw, executable):
            language = raw.split(b'language="')[1].split(b'"', 1)[0].decode()
            rows = catalog.read_ts(raw, language)
            if any(row.forms == ('Nové %1',) for row in rows):
                raise ValueError('lrelease rejected candidate')
        with patch('qlog_localize.update_catalogs'), patch('qlog_localize.check_catalog', side_effect=validate):
            with self.assertRaisesRegex(ValueError, 'lrelease'):
                tool.run(self.root, 'release', self.provider)
        self.assertEqual((self.root / 'i18n/qlog_cs.ts').read_bytes(), original)
        self.assertEqual(json.loads((self.root / 'build/qlog-localize-report.json').read_text())['translated'], 0)

    def test_memory_failure_reports_catalog_writes_that_already_completed(self):
        self.updated()
        real_write = tool.atomic_write
        def write(path, content, **kwargs):
            if path.name == 'memory.json':
                raise OSError('Cannot save memory')
            return real_write(path, content, **kwargs)
        with patch('qlog_localize.update_catalogs'), patch('qlog_localize.atomic_write', side_effect=write):
            with self.assertRaisesRegex(OSError, 'memory'):
                tool.run(self.root, 'release', self.provider)
        rows = catalog.read_ts((self.root / 'i18n/qlog_cs.ts').read_bytes(), 'cs')
        self.assertEqual(rows[1].forms, ('Nové %1',))
        report = json.loads((self.root / 'build/qlog-localize-report.json').read_text())
        self.assertEqual(report['translated'], 1)
        self.assertEqual(report['status'], 'failed')

    def test_report_cannot_overwrite_learned_memory(self):
        memory = self.root / 'devtools/translator/memory.json'
        memory.parent.mkdir(parents=True)
        original = b'{"version": 1, "languages": {}}'
        memory.write_bytes(original)
        with self.assertRaisesRegex(ValueError, 'report'):
            tool.run(self.root, 'release', lambda prompt: self.fail('Unexpected model call'),
                     report_path=memory, lupdate='must-not-run')
        self.assertEqual(memory.read_bytes(), original)

    def test_relative_lrelease_path_is_resolved_before_using_a_temporary_directory(self):
        executable = shutil.which('lrelease')
        if not executable or os.name == 'nt':
            self.skipTest('Qt lrelease with symlink support unavailable')
        (self.root / 'lrelease').symlink_to(executable)
        script = ('import sys; from pathlib import Path; '
                  f'sys.path.insert(0, {str(Path(tool.__file__).parent)!r}); '
                  'from qlog_localize import check_catalog; '
                  'check_catalog(Path("i18n/qlog_cs.ts").read_bytes(), "./lrelease")')
        result = subprocess.run([sys.executable, '-c', script], cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_lupdate_failure_never_calls_model_or_writes_translations(self):
        original = (self.root / 'i18n/qlog_cs.ts').read_bytes()
        real_run = subprocess.run
        def runner(argv, **kwargs):
            if argv[0] == 'lupdate-test':
                return type('Result', (), {'returncode': 7})()
            return real_run(argv, **kwargs)
        with patch('qlog_localize.subprocess.run', side_effect=runner):
            with self.assertRaises(ValueError):
                tool.run(self.root, 'release', lambda prompt: self.fail('Model called after lupdate failure'), lupdate='lupdate-test')
        self.assertEqual((self.root / 'i18n/qlog_cs.ts').read_bytes(), original)

    def test_lupdate_failure_replaces_a_stale_success_report(self):
        report_path = self.root / 'build/qlog-localize-report.json'
        report_path.parent.mkdir()
        report_path.write_text('{"status":"complete","translated":123}', encoding='utf-8')
        with patch('qlog_localize.update_catalogs', side_effect=ValueError('lupdate failed')):
            with self.assertRaises(ValueError):
                tool.run(self.root, 'release', self.provider)
        report = json.loads(report_path.read_text(encoding='utf-8'))
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['translated'], 0)
        self.assertIn('lupdate failed', report['error'])

    def test_invalid_baseline_is_not_treated_as_an_empty_catalog(self):
        with self.assertRaises(ValueError):
            tool.run(self.root, 'does-not-exist', self.provider, lupdate='does-not-exist')

    def test_interruption_keeps_completed_batches_and_rerun_skips_them(self):
        self.updated()
        calls = []
        def failing(prompt):
            calls.append(prompt['language'])
            if prompt['language'] == 'German':
                raise ValueError('Backend interrupted')
            return self.provider(prompt)
        with patch('qlog_localize.update_catalogs'):
            with self.assertRaises(ValueError):
                tool.run(self.root, 'release', failing)
            self.assertEqual(catalog.read_ts((self.root / 'i18n/qlog_cs.ts').read_bytes(), 'cs')[1].forms, ('Nové %1',))
            report = tool.run(self.root, 'release', self.provider)
        self.assertEqual(report['translated'], len(catalog.LANGUAGES) - 1)
        stored = json.loads((self.root / 'build/qlog-localize-report.json').read_text(encoding='utf-8'))
        self.assertEqual(stored['translated'], len(catalog.LANGUAGES) - 1)

    def test_concurrent_catalog_edit_is_preserved(self):
        self.updated()
        external = ts(message('Old') + message('New %1', 'Manual %1'), 'cs')
        def provider(prompt):
            if prompt['stage'] == 'review':
                (self.root / 'i18n/qlog_cs.ts').write_bytes(external)
            return self.provider(prompt)
        with patch('qlog_localize.update_catalogs'), self.assertRaises(ValueError):
            tool.run(self.root, 'release', provider)
        self.assertEqual((self.root / 'i18n/qlog_cs.ts').read_bytes(), external)

    def test_concurrent_source_edit_blocks_stale_translation(self):
        self.updated()
        def provider(prompt):
            (self.root / 'ui/panel.cpp').write_text('tr("Changed while translating");\n', encoding='utf-8')
            return self.provider(prompt)
        with patch('qlog_localize.update_catalogs'), self.assertRaises(ValueError):
            tool.run(self.root, 'release', provider)
        self.assertEqual(catalog.read_ts((self.root / 'i18n/qlog_cs.ts').read_bytes(), 'cs')[1].forms, ('',))

    def test_catalog_edit_during_source_verification_is_not_overwritten(self):
        self.updated()
        external = ts(message('Old') + message('New %1', 'Manual %1'), 'cs')
        signature = tool.source_signature
        calls = 0
        def edit_during_signature(root, paths):
            nonlocal calls
            calls += 1
            if calls == 2:
                (self.root / 'i18n/qlog_cs.ts').write_bytes(external)
            return signature(root, paths)
        with patch('qlog_localize.update_catalogs'), patch('qlog_localize.source_signature', side_effect=edit_during_signature):
            with self.assertRaises(ValueError):
                tool.run(self.root, 'release', self.provider)
        self.assertEqual((self.root / 'i18n/qlog_cs.ts').read_bytes(), external)

    def test_uncertain_translation_is_reported_and_not_written(self):
        self.updated()
        def provider(prompt):
            value = self.provider(prompt)
            for row in value['translations']:
                row.update(needs_review=True, reason='Meaning unresolved', forms=[])
            return value
        with patch('qlog_localize.update_catalogs'):
            report = tool.run(self.root, 'release', provider)
        self.assertEqual(report['translated'], 0)
        self.assertEqual(len(report['needs_review']), len(catalog.LANGUAGES))
        self.assertEqual(report['status'], 'needs_review')

    @unittest.skipIf(os.name == 'nt', 'POSIX permissions')
    def test_atomic_write_preserves_existing_permissions(self):
        target = self.root / 'test.ts'
        target.write_bytes(b'old')
        target.chmod(0o644)
        tool.atomic_write(target, b'new')
        self.assertEqual(target.read_bytes(), b'new')
        self.assertEqual(target.stat().st_mode & 0o777, 0o644)

    def test_real_lupdate_updates_worktree_before_translation(self):
        executable = shutil.which('lupdate')
        if not executable:
            self.skipTest('Qt lupdate unavailable')
        (self.root / 'QLog.pro').write_text(
            'SOURCES = ui/panel.cpp\nTRANSLATIONS = ' + ' '.join(f'i18n/qlog_{lang}.ts' for lang in catalog.LANGUAGES) + '\n', encoding='utf-8')
        (self.root / 'ui/panel.cpp').write_text(
            '#include <QObject>\nclass Panel : public QObject { Q_OBJECT\n'
            'void test() { tr("Old"); tr("New %1"); } };\n', encoding='utf-8')
        report = tool.run(self.root, 'release', self.provider, lupdate=executable)
        self.assertEqual(report['translated'], len(catalog.LANGUAGES))
        self.assertFalse((self.root / '.qmake.stash').exists())

    @unittest.skipIf(os.name == 'nt', 'POSIX executable fixture')
    def test_cli_pipeline_uses_real_qt_and_synthetic_codex_without_extra_stages(self):
        executable = shutil.which('lupdate')
        if not executable:
            self.skipTest('Qt lupdate unavailable')
        (self.root / 'QLog.pro').write_text(
            'SOURCES = ui/panel.cpp\nTRANSLATIONS = ' + ' '.join(f'i18n/qlog_{lang}.ts' for lang in catalog.LANGUAGES) + '\n', encoding='utf-8')
        (self.root / 'ui/panel.cpp').write_text(
            '#include <QObject>\nclass Panel : public QObject { Q_OBJECT\n'
            'void test() { tr("Old"); tr("New %1"); } };\n', encoding='utf-8')
        directory = self.root / 'devtools/translator'
        directory.mkdir(parents=True)
        for name in ['catalog.py', 'localize.py', 'qlog_localize.py']:
            shutil.copy2(Path(tool.__file__).parent / name, directory / name)
        binaries = self.root / 'bin'
        binaries.mkdir()
        codex = binaries / 'codex'
        codex.write_text('#!' + sys.executable + '\nimport json,sys\nfrom pathlib import Path\n'
                         'prompt=json.load(sys.stdin)\n'
                         'target="新 %1" if "Simplified Chinese" in prompt["language"] else "Nové %1"\n'
                         'rows=[{"id":row["id"],"forms":[target],"needs_review":False,"reason":""} for row in prompt["messages"]]\n'
                         'Path(sys.argv[sys.argv.index("-o")+1]).write_text(json.dumps({"translations":rows,"source_requests":[],"terms":[]}),encoding="utf-8")\n', encoding='utf-8')
        codex.chmod(0o755)
        environment = {**os.environ, 'PATH': str(binaries) + os.pathsep + os.environ['PATH'],
                       'PYTHONDONTWRITEBYTECODE': '1'}
        for language in ('cs', 'zh_CN'):
            with self.subTest(language=language):
                result = subprocess.run([sys.executable, str(directory / 'qlog_localize.py'),
                                         '--from', 'release', '--lupdate', executable, '--language', language],
                                        env=environment, cwd=self.root / 'ui', capture_output=True, encoding='utf-8')
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = json.loads((self.root / 'build/qlog-localize-report.json').read_text(encoding='utf-8'))
                self.assertEqual(report['translated'], 1)
                self.assertEqual(report['status'], 'complete')
                self.assertEqual(set(report['languages']), {language})
                chinese = catalog.read_ts((self.root / 'i18n/qlog_zh_CN.ts').read_bytes(), 'zh_CN')
                self.assertEqual(next(row.forms for row in chinese if row.source == 'New %1'),
                                 ('新 %1',) if language == 'zh_CN' else ('',))


if __name__ == '__main__':
    unittest.main()
