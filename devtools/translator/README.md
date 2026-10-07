# QLog release localization

Translate only new or changed source texts since a previous QLog release into
Czech, German, French, Spanish, Italian and Simplified Chinese (`zh_CN`). The tool uses the current working
tree, including uncommitted source changes, and writes directly to `i18n/`.

Requires Python 3.10+, Git, your Qt kit's `lupdate` and `lrelease`, and an installed, signed-in
Codex CLI with `exec --output-schema` support. No Python packages are required.
Your configured Codex model is used unless you supply `--model`.
Each request replaces the general coding instructions with localization rules,
disables coding memories and plugin/agent features, and limits the skills catalog.
These overrides affect only that request; model/provider settings are preserved.

From the QLog root:

```bash
python3 devtools/translator/qlog_localize.py --from v0.53.0
```

To translate only Czech:

```bash
python3 devtools/translator/qlog_localize.py --from v0.53.0 --language cs
```

Repeat `--language` to select several languages. Without it, all six are selected.
`lupdate` still updates all project catalogs before the selected languages are
analyzed and translated.

Choose the last release whose existing translation backlog should be excluded.
The repository is always the QLog checkout containing this script; there is no
`--repo`, target release, export stage, or separate apply command.

## What happens

1. Validate the reference and options, then run the equivalent of:

   ```bash
   lupdate QLog.pro -no-obsolete -locations absolute \
     -disable-heuristic similartext -disable-heuristic sametext \
     -disable-heuristic number
   ```

   This updates the project's current catalogs before analysis. It uses every
   catalog listed in `QLog.pro`, including Chinese. The localization stage reads
   and translates the selected languages from `cs`, `de`, `fr`, `es`, `it`, `zh_CN`. An unsuccessful `lupdate`
   stops the run before any model requests.
   Supported heuristics are read from `lupdate -help`; `number` is omitted for
   Qt kits that do not provide it, including recent Qt6 versions.
2. Compare the selected updated catalogs against their catalogs in `--from`. Source,
   disambiguation and plurality determine release eligibility. Texts present in
   the baseline, including unfinished texts and context-only moves, are excluded.
   Changed source text is eligible. Nonempty drafts, finished translations, byte
   elements and length variants are preserved.
3. Retrieve a small set of relevant approved translations and draft references.
   Exact reuse requires matching context, disambiguation or explicit meaning
   hints and an unambiguous target. It is checked again before each batch, so
   later batches can reuse freshly reviewed phrases without model calls.
   Approved human translations take precedence over reviewed suggestions;
   conflicting approved variants remain visible.
4. Translate messages from multiple dialogs together in bounded batches, keeping
   each message's own context. Short request-local IDs and a constrained response
   schema reduce token use and prevent references to nonexistent message IDs.
   Codex can request source
   excerpts and literal searches, including surrounding functions and callers,
   from current QLog source files. Those requests are served by the script; the
   model runs read-only outside the repository. Source access and prompt size
   are bounded. Clear translations can be returned while context is requested
   for specific message IDs; only those remaining messages are sent again.
5. Independently review proposals for meaning, natural native ham-radio language
   and consistency. Review groups are sized using the actual proposals, so longer
   target text can split the review without repeating the translation request.
   Share only compact technical terminology decisions relevant
   to each batch, rather than a detailed dictionary for every new word.
6. Validate formatting, compile each candidate TS with `lrelease` into a temporary
   QM file, then write each completed batch atomically. The updated catalogs are
   also checked before any model requests. No QM files are left in the checkout.
   Suggestions remain `unfinished`. Inspect the Git diff and accept translations
   in Qt Linguist.

Technical checks preserve literal tags/attributes and angle-bracket parameters,
entities, file filters including `(*)` and `;;`, stars, square-bracket arguments,
backtick literals, symbols, tabs, boundary spaces, indentation, line structure and
markup text regions. Qt placeholders can change order within a text region but
cannot move across markup or line boundaries. Checks also cover plural counts,
underscore identifiers, dotted technical names, SQL example lines, known product
names and ampersands. Ordinary prose punctuation may follow native usage.
There is one formatting repair request per stage; unresolved rows are reported
without being written, while valid rows can continue.
Source lookups allow at most two rounds per translation or review group, with up
to eight requests per round. A repeated lookup that adds no evidence also ends
the lookup phase. The next request must return translations or mark only the
remaining unclear messages for review; it cannot request more context. Unavailable
or invalid lookups do not discard other translations. Rows without a proposal
remain unresolved and do not trigger a separate review request.
Invalid optional terminology evidence is discarded and recorded as a warning;
it does not abort or retry otherwise valid translations. Translation IDs and
formatting still require strict validation.
Conflicting terminology decisions leave only their supporting translations for
manual review; unrelated valid translations can still be written.

`lrelease` verifies that Qt can load and compile the TS structure. It does not
prove linguistic quality or recognize every embedded command/name. Local
formatting checks remain necessary on Qt versions without `-fail-on-invalid`.
The tool does not use `-nounfinished` or `-fail-on-unfinished`, since suggestions
and excluded backlog are intentionally unfinished.

Detected concurrent catalog or source edits stop translation. Catalog bytes are
checked immediately before replacement; this is an optimistic check, not a lock
on external editors. Finished batches remain written if a later request fails. Rerunning
the same command skips their nonempty translations. Git is the revision and
rollback mechanism; the tool creates no backup catalog or proposal directory.

## Codex communication and intermediate files

Python starts a separate `codex exec` process for each request. Codex handles
authentication and communication with the configured model provider. The script
sends the request as JSON through standard input and reads the final JSON from
an output file. The basic command is:

```bash
codex exec --sandbox read-only --skip-git-repo-check \
  --output-schema schema.json -o response.json \
  -c 'model_instructions_file="instructions.txt"' -
```

The request contains the batch's source texts, context and selected consistency
references. Responses contain `translations`, `source_requests` and `terms`.
A normal batch needs one translation request and at least one review request,
using the same model in separate sessions. Direct reuse needs no model request.
If Codex requests source context, it identifies the affected `message_ids`.
Python reads the requested excerpts or performs a literal search, then starts
another request for only the remaining IDs with the added evidence. A positive
line number takes precedence over `query`; without it, `query` is a literal search.
Formatting repairs can also add a request. Every request starts a fresh session and resends
the required context; the script does not use `codex exec resume`.

| Location | Contents and lifetime |
|---|---|
| Python memory | Input JSON, proposed translations, source excerpts and validation errors; held during the run. |
| Temporary `qlog-localize-*` directory | `instructions.txt`, `schema.json` and `response.json`; deleted after each request. |
| Temporary `qlog-ts-check-*` directory | `candidate.ts` and `candidate.qm` for Qt validation; deleted after the check. |
| `i18n/*.ts` | Updated catalogs and successfully written translation suggestions. |
| `devtools/translator/memory.json` | Persistent terminology and reviewed-translation evidence. |
| `build/qlog-localize-report.json` | Counts, unresolved items, warnings and errors; replaced for each invocation. |
| Codex session storage, normally `~/.codex/sessions/` | Codex's own session records; the script does not use `--ephemeral` or remove these records. |

The script keeps no separate permanent archive of prompts and responses. Its
report records the number of Codex requests, but not token usage.

Timestamped progress output shows the language, stage, pending message count,
prompt size in bytes, request duration, source lookups, formatting repairs and
successful catalog writes. Capture it when investigating a run:

```bash
python3 devtools/translator/qlog_localize.py --from v0.53.0 --language cs \
  2>&1 | tee build/qlog-localize-cs.log
```

## Options and report

- `--lupdate /path/to/lupdate`: choose the executable from your Qt kit.
- `--lrelease /path/to/lrelease`: choose the matching Qt compiler for TS checks.
- `--language cs`: select one language (`cs`, `de`, `fr`, `es`, `it`, `zh_CN`); repeat for
  multiple languages. Default: all six. Use `--language zh_CN` for Simplified Chinese.
- `--model MODEL`: override your configured Codex model.
- `--timeout SECONDS`: limit each Codex request; default 600.
- `--prompt-budget BYTES`: maximum input JSON size in UTF-8 bytes; default 64000,
  minimum 8000. This is a byte limit, not an exact model token count.
- `--glossary FILE`: optional mandatory labels, for example
  `{"cs": {"Worked": "Pracováno"}, "de": {"Mode": "Betriebsart"}}`.
  Sentence translations may inflect terminology; standalone labels are checked.
- `--report FILE`: override `build/qlog-localize-report.json`.

The report records selected languages, model request counts, excluded backlog,
preserved new drafts, written/reused counts, unresolved proposals, lookup and
terminology warnings, and failures. Unresolved items are
not written; resolve them manually or improve source comments before rerunning.
The report describes this invocation, not cumulative translation history.
Its final status is `needs_review` if any messages remain unresolved; `complete`
means there are no unresolved messages in this invocation.

`devtools/translator/memory.json` is created automatically after successful writes.
It stores compact technical terminology with evidence and hashes of reviewed
suggestions, not copies of the catalogs or a dictionary for every word. Each
terminology decision identifies the messages supporting its particular meaning;
another use of the same word cannot keep rejected terminology alive. Keep it
in Git to share decisions across machines and releases. Only records backed by
unchanged current translations and meaning hints are reused. Manual edits or
removed messages invalidate the affected evidence; approved catalog translations
remain authoritative. Deleting the memory file discards learned decisions without
changing any translations. Prompts still receive at most eight optional relevant
terms and eight relevant examples; mandatory glossary labels are always included.

Completed current-release drafts are available as consistency references on
reruns. Unchanged suggestions recorded as reviewed can be reused directly with
matching meaning evidence; other drafts remain references. The tool does not
automatically replace existing drafts, fix old translations or choose the most
frequent term in a conflict. Large individual messages that cannot fit the prompt
budget stop the run with an error; increase the budget or translate them manually.

## Focused verification

Run from `build/`:

```bash
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=../devtools/translator \
  python3 -m unittest discover -s ../devtools/translator/tests -v
```

Tests use temporary repositories and synthetic model responses. A small real
`lupdate` integration runs when Qt is available. They do not demonstrate live
Codex linguistic quality or interactive Linguist behavior.

The previous standalone tool is preserved in `devtools/old/`.

References: [Qt lupdate](https://doc.qt.io/qt-6/linguist-lupdate.html),
[Codex non-interactive mode](https://learn.chatgpt.com/docs/non-interactive-mode).
