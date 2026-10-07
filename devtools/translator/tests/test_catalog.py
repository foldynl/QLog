import unittest

try:
    import catalog
except ModuleNotFoundError:
    catalog = None


def ts(body, language='cs', context='Panel'):
    return (f'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n'
            f'<TS language="{language}"><context><name>{context}</name>'
            f'{body}</context></TS>\n').encode('utf-8')


def message(source, target='', state='unfinished', extra='', numerus=False):
    plural = ' numerus="yes"' if numerus else ''
    kind = f' type="{state}"' if state else ''
    return (f'<message{plural}>{extra}<source>{source}</source>'
            f'<translation{kind}>{target}</translation></message>')


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(catalog, 'The new catalog module is not implemented')

    def test_old_unfinished_and_moved_context_are_not_release_additions(self):
        baseline = catalog.read_ts(ts(message('Old'), context='Before'), 'cs')
        current = catalog.read_ts(ts(message('Old') + message('New'), context='After'), 'cs')
        self.assertEqual([m.source for m in catalog.select_new(current, baseline)], ['New'])

    def test_changed_source_is_new_but_nonempty_drafts_and_finished_are_preserved(self):
        baseline = catalog.read_ts(ts(message('Before')), 'cs')
        current = catalog.read_ts(ts(message('After') + message('Draft', 'Draft translation') +
                                     message('Done', 'Translation', '')), 'cs')
        self.assertEqual([m.source for m in catalog.select_new(current, baseline)], ['After'])

    def test_disambiguation_and_plurality_distinguish_meanings(self):
        baseline = catalog.read_ts(ts(message('Key', extra='<comment>API</comment>')), 'cs')
        current = catalog.read_ts(ts(message('Key', extra='<comment>Morse</comment>')), 'cs')
        self.assertEqual(len(catalog.select_new(current, baseline)), 1)

    def test_byte_elements_and_length_variants_are_never_overwritten(self):
        raw = ts(message('Escape', '<byte value="x1b"/>', '') +
                 '<message><source>Variant</source><translation variants="yes">'
                 '<lengthvariant/></translation></message>')
        rows = catalog.read_ts(raw, 'cs')
        self.assertEqual(rows[0].forms, ('\x1b',))
        self.assertEqual(catalog.select_new(rows, []), [])
        with self.assertRaises(ValueError):
            catalog.patch_ts(raw, {rows[0].id: ['Changed']}, 'cs')

    def test_patch_preserves_comments_cdata_and_all_unselected_bytes(self):
        raw = ts(message('Old', 'Hotovo', '') +
                 '<!-- <message><translation/></message> -->' +
                 '<message><source><![CDATA[New %1]]></source><translation type="unfinished" /></message>')
        row = catalog.read_ts(raw, 'cs')[1]
        result = catalog.patch_ts(raw, {row.id: ['Nové %1']}, 'cs')
        expected = raw.replace(b'<translation type="unfinished" />',
                               '<translation type="unfinished">Nové %1</translation>'.encode())
        self.assertEqual(result, expected)
        self.assertEqual(catalog.patch_ts(raw, {}, 'cs'), raw)

    def test_patch_inserts_missing_translation_and_plural_forms(self):
        raw = ts('<message><source>New</source></message>' + message('%n QSO', numerus=True))
        first, plural = catalog.read_ts(raw, 'cs')
        result = catalog.patch_ts(raw, {first.id: ['Nové'], plural.id: ['%n QSO'] * 3}, 'cs')
        rows = catalog.read_ts(result, 'cs')
        self.assertEqual(rows[0].forms, ('Nové',))
        self.assertEqual(rows[1].forms, ('%n QSO', '%n QSO', '%n QSO'))

    def test_finished_and_nonempty_unfinished_cannot_be_patched(self):
        for state in ['', 'unfinished']:
            raw = ts(message('New', 'Manual', state))
            row = catalog.read_ts(raw, 'cs')[0]
            with self.assertRaises(ValueError):
                catalog.patch_ts(raw, {row.id: ['Changed']}, 'cs')

    def test_intentionally_empty_finished_translation_is_preserved(self):
        raw = ts(message('Hidden label', '', ''))
        row = catalog.read_ts(raw, 'cs')[0]
        self.assertEqual(catalog.select_new([row], []), [])
        with self.assertRaises(ValueError):
            catalog.patch_ts(raw, {row.id: ['Visible label']}, 'cs')

    def test_nonempty_whitespace_draft_is_preserved(self):
        raw = ts(message('Spacing', '   '))
        row = catalog.read_ts(raw, 'cs')[0]
        self.assertEqual(catalog.select_new([row], []), [])
        with self.assertRaises(ValueError):
            catalog.patch_ts(raw, {row.id: ['Changed']}, 'cs')

    def test_exact_macro_case_products_fields_and_file_patterns_are_protected(self):
        for source, target in [('<DXCALL>', '<dxcall>'), ('FLDigi', 'FL-Digi'),
                               ('TQSL', 'T-QSL'), ('APP_QLOG_ID', 'APP_QLOG_IDENT'),
                               ('Files (*.adi)', 'Dateien (*.adif)')]:
            with self.subTest(source=source), self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'de')

    def test_lowercase_identifiers_and_qlogs_zero_placeholder_are_preserved(self):
        for source, target in [('auto_power_on', 'Automatisch einschalten'),
                               ('start_time station_callsign', 'Startzeit Rufzeichen'),
                               ('Profile %0', 'Profil')]:
            with self.subTest(source=source), self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'de')

    def test_placeholders_markup_mnemonics_and_newlines_are_protected(self):
        for source, target in [('%1 %1', '%1'), ('%L1', '%1'), ('&Save', 'Uložit'),
                               ('A\nB', 'A B'), ('<a href="x">X</a>', '<a href="y">Y</a>')]:
            with self.subTest(source=source), self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'cs')
        catalog.validate_forms('&Save %1\nQSO', False, ['&Uložit %1\nQSO'], 'cs')

    def test_language_specific_plural_counts(self):
        for lang, count in [('cs', 3), ('de', 2), ('fr', 2), ('es', 2), ('it', 2), ('zh_CN', 1)]:
            catalog.validate_forms('%n QSO', True, ['%n QSO'] * count, lang)
            with self.assertRaises(ValueError):
                catalog.validate_forms('%n QSO', True, ['%n QSO'] * (count - 1), lang)

    def test_chinese_requires_the_mainland_locale_and_preserves_utf8(self):
        raw = ts(message('Callsign', '呼号', '') + message('%n QSO', numerus=True), 'zh_CN')
        approved, plural = catalog.read_ts(raw, 'zh_CN')
        self.assertTrue(approved.approved)
        result = catalog.patch_ts(raw, {plural.id: ['%n 个 QSO']}, 'zh_CN')
        self.assertEqual(catalog.read_ts(result, 'zh_CN')[1].forms, ('%n 个 QSO',))
        with self.assertRaises(ValueError):
            catalog.read_ts(ts(message('Callsign'), 'zh_TW'), 'zh_CN')
        with self.assertRaises(ValueError):
            catalog.read_ts(ts(message('Callsign'), 'zh'), 'zh_CN')
        self.assertEqual(catalog.read_ts(ts(message('Callsign'), 'cs_CZ'), 'cs')[0].source, 'Callsign')

    def test_chinese_can_adjoin_protected_names_without_spaces(self):
        source = 'Confirmed QSO via LoTW'
        catalog.validate_forms(source, False, ['通过LoTW确认QSO'], 'zh_CN')
        for target in ['通过LoTW确认', '通过LoTW确认QSOs', '通过LoTw确认QSO']:
            with self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'zh_CN')

    def test_formatting_boundaries_and_literal_specials_cannot_change(self):
        cases = [('All Files (*)', 'Všechny soubory ()'),
                 ('Files (*.adi);;All Files (*)', 'Soubory (*.adi);Všechny soubory (*)'),
                 ('Frequency < 30 MHz >', 'Frekvence 30 MHz'),
                 ('Band\tMode', 'Pásmo Druh provozu'),
                 (' Name ', 'Název'),
                 ('First\n  Second', 'První\nDruhý'),
                 ('First\nSecond', '\nPrvní Druhý'),
                 ('<b>Important</b> warning', '<b>Důležité varování</b>'),
                 ('A&nbsp;B', 'A B'),
                 ('Ready ●', 'Připraveno'),
                 ('Send <callsign>', 'Odeslat <Callsign>'),
                 ('e.g. qlog.ui.*.runtime=true', 'např. qlog.ui.runtime=false'),
                 ('e.g. qlog.ui.*.runtime=true', 'např. qlog.ui.*.runtime=false'),
                 ('Example:\n  SELECT id, callsign FROM contacts WHERE ...', 'Příklad:\n  SELECT id FROM contacts ...')]
        for source, target in cases:
            with self.subTest(source=source), self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'cs')

    def test_native_word_order_inside_formatting_is_allowed(self):
        for source, target in [('<b>Important</b> warning', '<b>Důležité</b> varování'),
                               (' Name ', ' Název '), ('Band\tMode', 'Pásmo\tDruh provozu'),
                               ('Files (*.adi);;All Files (*)', 'Soubory (*.adi);;Všechny soubory (*)'),
                               ('Profile %1 of %2', 'Z %2 profil %1'),
                               ('Ready ●', 'Připraveno ●')]:
            with self.subTest(source=source):
                catalog.validate_forms(source, False, [target], 'cs')

    def test_translations_cannot_introduce_characters_invalid_in_xml(self):
        for character in ('\x00', '\x0b', '\ud800', '\ufffe', '\uffff'):
            with self.subTest(character=repr(character)), self.assertRaises(ValueError):
                catalog.validate_forms('New', False, ['Nové' + character], 'cs')

    def test_multiline_markup_attributes_remain_literal(self):
        for source, target in [('<a\nhref="x">Help</a>', '<a\nhref="y">Nápověda</a>'),
                               ('<span\nstyle="color:red">Warning</span>', '<span\nstyle="color:blue">Varování</span>'),
                               ('<a title="First\nSecond">Help</a>', '<a title="First\nChanged">Nápověda</a>')]:
            with self.subTest(source=source), self.assertRaises(ValueError):
                catalog.validate_forms(source, False, [target], 'cs')
        catalog.validate_forms('<a\nhref="x">Help</a>', False, ['<a\nhref="x">Nápověda</a>'], 'cs')

    def test_duplicate_id_and_wrong_language_are_rejected(self):
        with self.assertRaises(ValueError):
            catalog.read_ts(ts(message('Same') * 2), 'cs')
        with self.assertRaises(ValueError):
            catalog.read_ts(ts(message('New'), 'de'), 'cs')


if __name__ == '__main__':
    unittest.main()
