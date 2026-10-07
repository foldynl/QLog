from pathlib import Path
from contextlib import redirect_stdout
import io
import json
import tempfile
import unittest
from unittest.mock import patch

from test_catalog import message, ts
import catalog

try:
    import localize
except ModuleNotFoundError:
    localize = None


def response(batch, forms=None, requests=None, terms=None):
    return {'translations': [{'id': row.id, 'forms': forms or ['Translation'],
                              'needs_review': False, 'reason': ''} for row in batch],
            'source_requests': requests or [],
            'terms': [{**term, 'message_ids': term.get('message_ids', [row.id for row in batch])} for term in terms or []]}


class LocalizationTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(localize, 'The new localization module is not implemented')

    def test_direct_reuse_requires_matching_meaning_and_respects_glossary(self):
        known = catalog.read_ts(ts(message('Key', 'Klíč', '', extra='<extracomment>Morse key</extracomment>')), 'cs')
        new = catalog.read_ts(ts(message('Key', extra='<extracomment>Morse key</extracomment>'), context='Other'), 'cs')[0]
        memory = localize.Memory(known)
        self.assertEqual(memory.reuse(new, 'cs'), ['Klíč'])
        api = catalog.read_ts(ts(message('Key', extra='<extracomment>API key</extracomment>'), context='Other'), 'cs')[0]
        self.assertIsNone(memory.reuse(api, 'cs'))
        self.assertIsNone(localize.Memory(known, {'Key': 'Telegrafní klíč'}).reuse(new, 'cs'))

    def test_conflicting_approved_targets_are_visible_and_not_reused(self):
        first = catalog.read_ts(ts(message('Mode', 'Modus', '', extra='<comment>radio</comment>'), 'de'), 'de')
        second = catalog.read_ts(ts(message('Mode', 'Betriebsart', '', extra='<comment>radio</comment>'), 'de', 'Other'), 'de')
        new = catalog.read_ts(ts(message('Mode', extra='<comment>radio</comment>'), 'de', 'New'), 'de')[0]
        memory = localize.Memory(first + second)
        self.assertIsNone(memory.reuse(new, 'de'))
        prompt = localize.make_prompt([new], memory, 'de', 12000)
        self.assertIn('Modus', str(prompt))
        self.assertIn('Betriebsart', str(prompt))

    def test_large_memory_does_not_fill_prompt_and_long_messages_split_batches(self):
        known = catalog.read_ts(ts(''.join(message(f'Existing {index}', 'Hotovo', '') for index in range(300))), 'cs')
        new = catalog.read_ts(ts(''.join(message('New ' + str(index) + ' x' * 400) for index in range(8))), 'cs')
        memory = localize.Memory(known)
        batches = list(localize.make_batches(new, memory, 'cs', 12000))
        self.assertGreater(len(batches), 1)
        self.assertEqual(sum(map(len, batches)), 8)
        for batch in batches:
            prompt = localize.make_prompt(batch, memory, 'cs', 12000)
            self.assertLessEqual(localize.json_size(prompt), 12000)
            self.assertLessEqual(len(prompt['examples']), 8)

    def test_short_messages_from_many_dialogs_share_one_batch(self):
        rows = []
        for index in range(30):
            rows += catalog.read_ts(ts(message(f'New label {index}'), context=f'Dialog{index}'), 'cs')
        batches = list(localize.make_batches(rows, localize.Memory([]), 'cs', 24000))
        self.assertEqual(batches, [rows])
        prompt = localize.make_prompt(rows, localize.Memory([]), 'cs', 24000)
        self.assertEqual(len({row['context'] for row in prompt['messages']}), 30)

    def test_large_batches_review_longer_native_translations_without_translating_again(self):
        rows = catalog.read_ts(ts(''.join(message(
            f'Choose how the application should reconnect after the connection has been interrupted in setup {index}')
            for index in range(250))), 'cs')
        translated, reviewed = [], []
        def provider(prompt):
            (translated if prompt['stage'] == 'translate' else reviewed).extend(row['id'] for row in prompt['messages'])
            return {'translations': [{'id': row['id'], 'forms': [
                'Při nastavování připojení vyberte, jak se má aplikace znovu připojit po přerušení spojení '
                'a jakým způsobem se mají obnovit všechny předchozí možnosti připojení zařízení'],
                'needs_review': False, 'reason': ''} for row in prompt['messages']], 'source_requests': [], 'terms': []}
        memory = localize.Memory([])
        result = {}
        with tempfile.TemporaryDirectory() as root:
            for batch in localize.make_batches(rows, memory, 'cs', localize.DEFAULT_BUDGET):
                accepted, terms = localize.translate_batch(batch, memory, 'cs',
                    localize.Sources(root, []), provider, localize.DEFAULT_BUDGET)
                result.update(accepted)
        self.assertEqual(len(translated), len(rows))
        self.assertEqual(len(reviewed), len(rows))
        self.assertTrue(all(not row['needs_review'] for row in result.values()))

    def test_conflicting_new_terms_isolate_affected_rows_and_do_not_abort_other_translations(self):
        rows = catalog.read_ts(ts(message('Worked today') + message('Worked yesterday') + message('Confirmed')), 'cs')
        decisions = [{'source': 'Worked', 'meaning': 'QSO status', 'target': target, 'message_ids': [row.id]}
                     for row, target in zip(rows, ['Pracováno', 'Spojeno'])]
        def provider(prompt):
            return response(rows, ['Pracováno'], terms=decisions)
        warnings = []
        with tempfile.TemporaryDirectory() as root:
            result, terms = localize.translate_batch(rows, localize.Memory([]), 'cs',
                localize.Sources(root, []), provider, localize.DEFAULT_BUDGET, warnings=warnings)
        self.assertTrue(result[rows[0].id]['needs_review'])
        self.assertTrue(result[rows[1].id]['needs_review'])
        self.assertFalse(result[rows[2].id]['needs_review'])
        self.assertEqual(terms, [])
        self.assertTrue(warnings)

    def test_source_requests_read_current_uncommitted_code_then_review_translation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ui').mkdir()
            (root / 'ui/panel.cpp').write_text('// Current Morse key\ntr("Key");\n', encoding='utf-8')
            row = catalog.read_ts(ts(message('Key', extra='<location filename="../ui/panel.cpp" line="2"/>')), 'cs')[0]
            prompts = []
            def provider(prompt):
                prompts.append(prompt)
                if len(prompts) == 1:
                    return {'translations': [], 'terms': [], 'source_requests': [
                        {'path': 'ui/panel.cpp', 'line': 2, 'query': '', 'message_ids': [row.id]}]}
                return response([row], ['Telegrafní klíč'])
            rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                   localize.Sources(root, ['ui/panel.cpp']), provider, 12000)
            self.assertEqual(rows[row.id]['forms'], ['Telegrafní klíč'])
            self.assertIn('Current Morse key', str(prompts[1]['source_evidence']))
            self.assertEqual(prompts[-1]['stage'], 'review')

    def test_source_access_is_bounded_and_cannot_escape_repository(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'panel.cpp').write_text('KEY\n' * 10000, encoding='utf-8')
            sources = localize.Sources(root, ['panel.cpp'])
            result = sources.request({'path': 'panel.cpp', 'line': 5000, 'query': ''})
            self.assertLessEqual(localize.json_size(result), 4000)
            self.assertIn('KEY', str(sources.request({'path': '', 'line': 0, 'query': 'KEY'})))
            with self.assertRaises(ValueError):
                sources.request({'path': '../outside.cpp', 'line': 1, 'query': ''})

    def test_source_excerpt_always_contains_the_requested_line(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'panel.cpp').write_text(('// ' + 'x' * 110 + '\n') * 30 + 'tr("Key");\n', encoding='utf-8')
            result = localize.Sources(root, ['panel.cpp']).request({'path': 'panel.cpp', 'line': 31, 'query': ''})
            self.assertIn('31: tr("Key");', str(result))
            self.assertLessEqual(localize.json_size(result), 3500)

    def test_unresolved_context_requests_leave_messages_for_review(self):
        row = catalog.read_ts(ts(message('Key')), 'cs')[0]
        calls = []
        def provider(prompt):
            calls.append(prompt)
            if not prompt['source_requests_allowed']:
                value = response([row])
                value['translations'][0].update(forms=[], needs_review=True, reason='Source file unavailable')
                return value
            return {'translations': [], 'terms': [], 'source_requests': [
                {'path': 'missing.cpp', 'line': 1, 'query': '', 'message_ids': [row.id]}]}
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                   localize.Sources(Path(root), []), provider, 12000)
        self.assertTrue(rows[row.id]['needs_review'])
        self.assertEqual(rows[row.id]['forms'], [])
        self.assertLessEqual(len(calls), 4)

    def test_positive_source_line_is_used_even_when_query_is_a_description(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'panel.cpp'
            path.write_text('// Morse key\ntr("Key");\n', encoding='utf-8')
            result = localize.Sources(root, ['panel.cpp']).request({
                'path': 'panel.cpp', 'line': 2, 'query': 'Explain how this Morse key is used'})
        self.assertIn('tr("Key")', str(result))

    def test_partial_context_requests_keep_clear_translations_and_shrink_followups(self):
        rows = catalog.read_ts(ts(message('Worked') + message('Key') + message('Unknown use')), 'cs')
        prompts = []
        def provider(prompt):
            prompts.append(prompt)
            pending = [row for row in rows if row.id in {item['id'] for item in prompt['messages']}]
            if prompt['stage'] == 'review':
                value = response(pending, ['Pracováno'])
                for item in value['translations']:
                    if item['id'] == rows[2].id:
                        item.update(forms=[], needs_review=True, reason='Source file unavailable')
                return value
            if len(prompts) == 1:
                return {**response([rows[0]], ['Pracováno']), 'source_requests': [{
                    'path': 'panel.cpp', 'line': 2, 'query': '', 'message_ids': [rows[1].id, rows[2].id]}]}
            if len(prompts) == 2:
                return {**response([rows[1]], ['Telegrafní klíč']), 'source_requests': [{
                    'path': 'missing.cpp', 'line': 1, 'query': '', 'message_ids': [rows[2].id]}]}
            self.assertFalse(prompt['source_requests_allowed'])
            value = response([rows[2]])
            value['translations'][0].update(forms=[], needs_review=True, reason='Source file unavailable')
            return value
        with tempfile.TemporaryDirectory() as root:
            (Path(root) / 'panel.cpp').write_text('// Morse key\ntr("Key");\n', encoding='utf-8')
            result, terms = localize.translate_batch(rows, localize.Memory([]), 'cs',
                localize.Sources(root, ['panel.cpp']), provider, 24000)
        self.assertEqual([len(p['messages']) for p in prompts[:3]], [3, 2, 1])
        self.assertFalse(result[rows[0].id]['needs_review'])
        self.assertFalse(result[rows[1].id]['needs_review'])
        self.assertTrue(result[rows[2].id]['needs_review'])

    def test_repeated_source_request_gets_final_response_instead_of_losing_batch(self):
        row = catalog.read_ts(ts(message('Key')), 'cs')[0]
        prompts = []
        def provider(prompt):
            prompts.append(prompt)
            if prompt['stage'] == 'review' or not prompt['source_requests_allowed']:
                return response([row], ['Telegrafní klíč'])
            return {'translations': [], 'terms': [], 'source_requests': [{
                'path': 'panel.cpp', 'line': 2, 'query': '', 'message_ids': [row.id]}]}
        with tempfile.TemporaryDirectory() as root:
            (Path(root) / 'panel.cpp').write_text('// Morse key\ntr("Key");\n', encoding='utf-8')
            result, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                localize.Sources(root, ['panel.cpp']), provider, 12000)
        self.assertEqual(len(prompts), 4)
        self.assertFalse(prompts[2]['source_requests_allowed'])
        self.assertEqual(result[row.id]['forms'], ['Telegrafní klíč'])

    def test_partial_response_must_account_for_every_id_without_overlaps(self):
        rows = catalog.read_ts(ts(message('Worked') + message('Key')), 'cs')
        valid = {**response([rows[0]], ['Pracováno']), 'source_requests': [{
            'path': 'panel.cpp', 'line': 2, 'query': '', 'message_ids': [rows[1].id]}]}
        result, terms, requests = localize.validate_response(valid, rows, 'cs')
        self.assertEqual(set(result), {rows[0].id})
        for ids in ([rows[0].id], ['unknown'], []):
            invalid = {**valid, 'source_requests': [{**valid['source_requests'][0], 'message_ids': ids}]}
            with self.subTest(ids=ids), self.assertRaises(ValueError):
                localize.validate_response(invalid, rows, 'cs')

    def test_invalid_source_lookup_does_not_discard_other_translations(self):
        rows = catalog.read_ts(ts(message('Worked') + message('Key')), 'cs')
        for path in ('', '../outside.cpp'):
            def provider(prompt):
                if prompt['stage'] == 'review':
                    return response([rows[0]], ['Pracováno'])
                if not prompt['source_requests_allowed']:
                    result = response([rows[1]])
                    result['translations'][0].update(forms=[], needs_review=True, reason='Source context unavailable')
                    return result
                result = response([rows[0]], ['Pracováno']) if len(prompt['messages']) == 2 else response([])
                result['source_requests'] = [{'path': path, 'line': 0, 'query': '', 'message_ids': [rows[1].id]}]
                return result
            warnings = []
            with self.subTest(path=path), tempfile.TemporaryDirectory() as root:
                result, terms = localize.translate_batch(rows, localize.Memory([]), 'cs',
                    localize.Sources(root, []), provider, 12000, warnings=warnings)
                self.assertFalse(result[rows[0].id]['needs_review'])
                self.assertTrue(result[rows[1].id]['needs_review'])
                self.assertTrue(warnings)

    def test_linguistic_reviewer_can_correct_or_block_a_proposal(self):
        row = catalog.read_ts(ts(message('Worked')), 'cs')[0]
        def provider(prompt):
            if prompt['stage'] == 'translate':
                return response([row], ['Odpracováno'])
            return response([row], ['Pracováno'])
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                   localize.Sources(Path(root), []), provider, 12000)
        self.assertEqual(rows[row.id]['forms'], ['Pracováno'])

    def test_response_ids_and_technical_mutations_are_rejected(self):
        row = catalog.read_ts(ts(message('%1 %1')), 'cs')[0]
        for value in [response([], []), response([row], ['%1']),
                      {**response([row], ['%1 %1']), 'unknown': 1}]:
            with self.assertRaises(ValueError):
                localize.validate_response(value, [row], 'cs')

    def test_terms_are_shared_by_meaning_and_conflicts_are_blocked(self):
        row = catalog.read_ts(ts(message('New Mode'), 'de'), 'de')[0]
        memory = localize.Memory([])
        memory.add_terms([{'source': 'Mode', 'meaning': 'radio emission mode', 'target': 'Betriebsart'}])
        self.assertIn('Betriebsart', str(localize.make_prompt([row], memory, 'de', 12000)))
        def provider(prompt):
            return response([row], ['Neuer Modus'], terms=[
                {'source': 'Mode', 'meaning': 'radio emission mode', 'target': 'Modus'}])
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch([row], memory, 'de',
                                                   localize.Sources(Path(root), []), provider, 12000)
        self.assertTrue(rows[row.id]['needs_review'])

    def test_mandatory_glossary_is_not_silently_truncated(self):
        labels = ['Band', 'Mode', 'Rig', 'Keyer', 'Locator', 'Award', 'Worked', 'Confirmed', 'Operator']
        row = catalog.read_ts(ts(message(' '.join(labels))), 'cs')[0]
        glossary = {label: 'Term ' + label for label in labels}
        prompt = localize.make_prompt([row], localize.Memory([], glossary), 'cs', 12000)
        self.assertEqual({term['source'] for term in prompt['terminology']}, set(labels))

    def test_reviewer_sees_proposed_terminology_before_it_is_shared(self):
        row = catalog.read_ts(ts(message('Worked')), 'cs')[0]
        term = {'source': 'Worked', 'meaning': 'QSO award status', 'target': 'Pracováno'}
        def provider(prompt):
            if prompt['stage'] == 'review':
                self.assertEqual(prompt['proposed_terminology'], [{**term, 'message_ids': [row.id]}])
            return response([row], ['Pracováno'], terms=[term])
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                   localize.Sources(Path(root), []), provider, 12000)
        self.assertEqual(terms, [{**term, 'message_ids': [row.id]}])

    def test_duplicate_examples_do_not_hide_a_conflicting_target(self):
        known = []
        for index in range(12):
            known += catalog.read_ts(ts(message('Mode', 'Modus', ''), 'de', f'Panel{index}'), 'de')
        known += catalog.read_ts(ts(message('Mode', 'Betriebsart', ''), 'de', 'Other'), 'de')
        row = catalog.read_ts(ts(message('Mode'), 'de', 'New'), 'de')[0]
        examples = localize.Memory(known).examples([row])
        self.assertEqual({entry['forms'][0] for entry in examples}, {'Modus', 'Betriebsart'})

    def test_large_message_is_not_rejected_just_because_previous_batch_is_nonempty(self):
        rows = catalog.read_ts(ts(message('Short') + message('Long ' + 'x' * 9000)), 'cs')
        batches = list(localize.make_batches(rows, localize.Memory([]), 'cs', 12000))
        self.assertEqual([len(batch) for batch in batches], [1, 1])

    def test_codex_uses_utf8_read_only_and_the_configured_model_by_default(self):
        def run(argv, **kwargs):
            self.assertEqual(kwargs['encoding'], 'utf-8')
            self.assertIn('read-only', argv)
            self.assertNotIn('--model', argv)
            self.assertNotEqual(Path(kwargs['cwd']), Path.cwd())
            self.assertIn('memories.use_memories=false', argv)
            self.assertIn('skills.max_context_tokens=1', argv)
            instruction = next(value for value in argv if value.startswith('model_instructions_file='))
            self.assertEqual(Path(json.loads(instruction.split('=', 1)[1])).read_text(), localize.INSTRUCTIONS)
            self.assertNotIn('instructions', json.loads(kwargs['input']))
            Path(argv[argv.index('-o') + 1]).write_text(
                '{"translations":[],"source_requests":[],"terms":[]}', encoding='utf-8')
            return type('Result', (), {'returncode': 0, 'stderr': ''})()
        with patch('localize.subprocess.run', side_effect=run):
            value = localize.Codex()({'text': 'Pásmo ⚠'})
        self.assertEqual(value['translations'], [])

    def test_bad_formatting_is_repaired_before_linguistic_review(self):
        row = catalog.read_ts(ts(message('All Files (*)')), 'cs')[0]
        prompts = []
        def provider(prompt):
            prompts.append(prompt)
            return response([row], ['Všechny soubory ()' if len(prompts) == 1 else 'Všechny soubory (*)'])
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                   localize.Sources(root, []), provider, 12000)
        self.assertEqual(rows[row.id]['forms'], ['Všechny soubory (*)'])
        self.assertEqual(len(prompts), 3)
        self.assertIn(row.id, str(prompts[1]['validation_errors']))
        self.assertEqual(prompts[-1]['stage'], 'review')

    def test_repeated_format_errors_leave_only_bad_rows_unwritten(self):
        batch = catalog.read_ts(ts(message('All Files (*)') + message('Worked')), 'cs')
        prompts = []
        def provider(prompt):
            prompts.append(prompt)
            pending = {row['id'] for row in prompt['messages']}
            return {'translations': [{'id': row.id, 'forms': ['Všechny soubory ()' if index == 0 else 'Pracováno'],
                                     'needs_review': False, 'reason': ''} for index, row in enumerate(batch) if row.id in pending],
                    'source_requests': [], 'terms': []}
        with tempfile.TemporaryDirectory() as root:
            rows, terms = localize.translate_batch(batch, localize.Memory([]), 'cs',
                                                   localize.Sources(root, []), provider, 12000)
        self.assertTrue(rows[batch[0].id]['needs_review'])
        self.assertFalse(rows[batch[1].id]['needs_review'])
        self.assertEqual(rows[batch[1].id]['forms'], ['Pracováno'])
        self.assertLessEqual(len(prompts), 4)

    def test_invalid_xml_characters_can_be_repaired_without_breaking_prompt_encoding(self):
        row = catalog.read_ts(ts(message('New')), 'cs')[0]
        for character in ('\x00', '\ud800'):
            prompts = []
            def provider(prompt):
                localize.json_size(prompt)
                prompts.append(prompt)
                return response([row], ['Nové' + character if len(prompts) == 1 else 'Nové'])
            with self.subTest(character=repr(character)), tempfile.TemporaryDirectory() as root:
                rows, terms = localize.translate_batch([row], localize.Memory([]), 'cs',
                                                       localize.Sources(root, []), provider, 12000)
                self.assertEqual(rows[row.id]['forms'], ['Nové'])
                self.assertEqual(len(prompts), 3)

    def test_reviewed_memory_survives_reload_and_manual_edits_invalidate_it(self):
        first = catalog.read_ts(ts(message('Worked', extra='<extracomment>QSO status</extracomment>')), 'cs')[0]
        second = catalog.read_ts(ts(message('Worked', extra='<extracomment>QSO status</extracomment>'), context='Other'), 'cs')[0]
        memory = localize.Memory([])
        memory.add(first, ['Pracováno'], reviewed=True)
        memory.add_terms([{'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno', 'message_ids': [first.id]}], [first])
        saved = memory.snapshot()
        unchanged = catalog.read_ts(ts(message('Worked', 'Pracováno', extra='<extracomment>QSO status</extracomment>')), 'cs')
        restored = localize.Memory(unchanged, saved=saved)
        self.assertEqual(restored.reuse(second, 'cs'), ['Pracováno'])
        self.assertIn('Pracováno', str(restored.relevant_terms([second])))
        edited = catalog.read_ts(ts(message('Worked', 'Odpracováno', extra='<extracomment>QSO status</extracomment>')), 'cs')
        restored = localize.Memory(edited, saved=saved)
        self.assertIsNone(restored.reuse(second, 'cs'))
        self.assertEqual(restored.relevant_terms([second]), [])
        approved = catalog.read_ts(ts(message('Worked', 'Spojeno', '', extra='<extracomment>QSO status</extracomment>')), 'cs')
        self.assertEqual(localize.Memory(approved, saved=saved).reuse(second, 'cs'), ['Spojeno'])

    def test_changed_meaning_invalidates_saved_review(self):
        first = catalog.read_ts(ts(message('Key', extra='<extracomment>Morse key</extracomment>')), 'cs')[0]
        memory = localize.Memory([])
        memory.add(first, ['Klíč'], reviewed=True)
        edited = catalog.read_ts(ts(message('Key', 'Klíč', extra='<extracomment>API key</extracomment>')), 'cs')
        new = catalog.read_ts(ts(message('Key', extra='<extracomment>API key</extracomment>'), context='Other'), 'cs')[0]
        self.assertIsNone(localize.Memory(edited, saved=memory.snapshot()).reuse(new, 'cs'))

    def test_human_approved_translation_has_priority_over_a_reviewed_suggestion(self):
        hint = '<extracomment>QSO status</extracomment>'
        approved = catalog.read_ts(ts(message('Worked', 'Pracováno', '', extra=hint)), 'cs')[0]
        suggestion = catalog.read_ts(ts(message('Worked', extra=hint), context='Other'), 'cs')[0]
        new = catalog.read_ts(ts(message('Worked', extra=hint), context='New'), 'cs')[0]
        memory = localize.Memory([approved])
        memory.add(suggestion, ['Odpracováno'], reviewed=True)
        self.assertEqual(memory.reuse(new, 'cs'), ['Pracováno'])
        memory.add(approved, approved.forms, reviewed=True)
        restored = localize.Memory([approved], saved=memory.snapshot())
        self.assertEqual(restored.examples([new])[0]['origin'], 'approved')

    def test_terms_keep_evidence_only_for_their_specific_meaning(self):
        api = catalog.read_ts(ts(message('Key', extra='<comment>API</comment>')), 'cs')[0]
        morse = catalog.read_ts(ts(message('Key', extra='<comment>Morse</comment>')), 'cs')[0]
        memory = localize.Memory([])
        memory.add(api, ['Token'], reviewed=True)
        memory.add(morse, ['Klíč'], reviewed=True)
        memory.add_terms([{'source': 'Key', 'meaning': 'API authentication', 'target': 'Token', 'message_ids': [api.id]},
                          {'source': 'Key', 'meaning': 'Morse key', 'target': 'Klíč', 'message_ids': [morse.id]}], [api, morse])
        edited = catalog.read_ts(ts(message('Key', 'Přístupový klíč', extra='<comment>API</comment>') +
                                   message('Key', 'Klíč', extra='<comment>Morse</comment>')), 'cs')
        restored = localize.Memory(edited, saved=memory.snapshot())
        self.assertEqual([term['target'] for term in restored.relevant_terms([api])], ['Klíč'])

    def test_terms_cannot_claim_unknown_or_unrelated_message_evidence(self):
        row = catalog.read_ts(ts(message('Worked')), 'cs')[0]
        other = catalog.read_ts(ts(message('Mode')), 'cs')[0]
        for ids in ([other.id], [row.id, other.id], [row.id, row.id], []):
            term = {'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno', 'message_ids': ids}
            with self.subTest(ids=ids):
                warnings = []
                rows, terms, requests = localize.validate_response(
                    response([row], ['Pracováno'], terms=[term]), [row], 'cs', warnings=warnings)
                self.assertEqual(rows[row.id]['forms'], ['Pracováno'])
                self.assertEqual(terms, [])
                self.assertEqual(len(warnings), 1)
        term = {'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno', 'message_ids': [other.id]}
        result, terms, requests = localize.validate_response(response([row, other], terms=[term]), [row, other], 'cs')
        self.assertEqual(len(result), 2)
        self.assertEqual(terms, [])

    def test_irrelevant_term_does_not_retry_or_abort_valid_translations(self):
        rows = catalog.read_ts(ts(message('CW Keyer Output Error') + message('Cannot switch CW keyer output')), 'cs')
        term = {'source': 'CW keyer', 'meaning': 'Morse keying device', 'target': 'CW klíč',
                'message_ids': [row.id for row in rows] + ['unknown']}
        calls, warnings = [], []
        def provider(prompt):
            calls.append(prompt['stage'])
            return response(rows, ['CW klíč'], terms=[term])
        with tempfile.TemporaryDirectory() as root:
            result, terms = localize.translate_batch(rows, localize.Memory([]), 'cs',
                localize.Sources(root, []), provider, 24000, warnings=warnings)
        self.assertEqual(calls, ['translate', 'review'])
        self.assertTrue(all(not row['needs_review'] for row in result.values()))
        self.assertEqual(terms, [])
        self.assertTrue(warnings)

    def test_codex_compacts_ids_and_restores_translation_and_term_evidence(self):
        row = catalog.read_ts(ts(message('Worked')), 'cs')[0]
        prompt = localize.make_prompt([row], localize.Memory([]), 'cs', 24000,
            proposals=response([row], ['Pracováno'])['translations'],
            proposed_terms=[{'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno', 'message_ids': [row.id]}],
            errors=[{'id': row.id, 'reason': 'Repair'}])
        prompt['source_requests_allowed'] = False
        def run(argv, **kwargs):
            sent = json.loads(kwargs['input'])
            ident = sent['messages'][0]['id']
            self.assertLess(len(ident), 8)
            self.assertEqual(sent['proposals'][0]['id'], ident)
            self.assertEqual(sent['validation_errors'][0]['id'], ident)
            self.assertEqual(sent['proposed_terminology'][0]['message_ids'], [ident])
            schema = json.loads(Path(argv[argv.index('--output-schema') + 1]).read_text())
            self.assertEqual(schema['properties']['translations']['items']['properties']['id']['enum'], [ident])
            self.assertEqual(schema['properties']['source_requests']['maxItems'], 0)
            self.assertEqual(schema['properties']['translations']['minItems'], 1)
            value = response([row], ['Pracováno'], terms=[{'source': 'Worked', 'meaning': 'QSO status', 'target': 'Pracováno'}])
            value['translations'][0]['id'] = ident
            value['terms'][0]['message_ids'] = [ident]
            Path(argv[argv.index('-o') + 1]).write_text(json.dumps(value), encoding='utf-8')
            return type('Result', (), {'returncode': 0, 'stderr': ''})()
        with patch('localize.subprocess.run', side_effect=run):
            value = localize.Codex()(prompt)
        self.assertEqual(value['translations'][0]['id'], row.id)
        self.assertEqual(value['terms'][0]['message_ids'], [row.id])
        self.assertEqual(prompt['messages'][0]['id'], row.id)

    def test_codex_does_not_log_model_payloads(self):
        prompt = {'instructions': 'Use ham-radio terminology.', 'language': 'Czech', 'stage': 'review',
                  'messages': [{'id': 'original-id', 'source': 'Značka <CALL>'}],
                  'source_evidence': [{'path': 'panel.cpp', 'excerpts': [{'text': 'radio context'}]}]}
        raw_reply = '{"translations":[{"id":"m1","forms":["Značka"],"needs_review":false,"reason":""}],"source_requests":[],"terms":[]}'
        output = io.StringIO()
        def run(argv, **kwargs):
            Path(argv[argv.index('-o') + 1]).write_text(raw_reply, encoding='utf-8')
            return type('Result', (), {'returncode': 0, 'stderr': ''})()
        with redirect_stdout(output), patch('localize.subprocess.run', side_effect=run):
            result = localize.Codex()(prompt)
        self.assertEqual(output.getvalue(), '')
        self.assertEqual(result['translations'][0]['id'], 'original-id')
        self.assertEqual(prompt['messages'][0]['id'], 'original-id')

    def test_codex_does_not_log_failed_model_responses(self):
        for returncode in (0, 1):
            with self.subTest(returncode=returncode):
                output = io.StringIO()
                def run(argv, **kwargs):
                    Path(argv[argv.index('-o') + 1]).write_text('invalid model reply: <CALL>', encoding='utf-8')
                    return type('Result', (), {'returncode': returncode, 'stderr': 'failure'})()
                with redirect_stdout(output), patch('localize.subprocess.run', side_effect=run):
                    with self.assertRaises(ValueError):
                        localize.Codex()({'messages': []})
                self.assertEqual(output.getvalue(), '')

    def test_codex_restores_source_request_message_ids(self):
        row = catalog.read_ts(ts(message('Key')), 'cs')[0]
        def run(argv, **kwargs):
            sent = json.loads(kwargs['input'])
            ident = sent['messages'][0]['id']
            schema = json.loads(Path(argv[argv.index('--output-schema') + 1]).read_text())
            self.assertEqual(schema['properties']['source_requests']['items']['properties']['message_ids']['items']['enum'], [ident])
            value = {'translations': [], 'terms': [], 'source_requests': [{
                'path': 'panel.cpp', 'line': 2, 'query': '', 'message_ids': [ident]}]}
            Path(argv[argv.index('-o') + 1]).write_text(json.dumps(value), encoding='utf-8')
            return type('Result', (), {'returncode': 0, 'stderr': ''})()
        with patch('localize.subprocess.run', side_effect=run):
            value = localize.Codex()(localize.make_prompt([row], localize.Memory([]), 'cs', 12000))
        self.assertEqual(value['source_requests'][0]['message_ids'], [row.id])


if __name__ == '__main__':
    unittest.main()
