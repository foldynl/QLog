"""Qt TS messages, release selection and byte-preserving translation patches."""

from collections import Counter
from dataclasses import dataclass
import hashlib
import html
import json
import re
import unicodedata
import xml.etree.ElementTree as ET
from xml.parsers import expat


LANGUAGES = {'cs': ('Czech', 3), 'de': ('German', 2), 'fr': ('French', 2),
             'es': ('Spanish', 2), 'it': ('Italian', 2),
             'zh_CN': ('Simplified Chinese (Mainland China)', 1)}
PROTECTED = ('QLog', 'WSJT-X', 'Hamlib', 'Omnirig', 'OmniRig', 'FLRig', 'FLrig',
             'FLDigi', 'fldigi', 'LoTW', 'TQSL', 'eQSL', 'QRZ', 'QRZ.com', 'HamQTH',
             'Clublog', 'ClubLog', 'Wavelog', 'Cloudlog', 'WinKey', 'WinKeyer',
             'ADIF', 'DXCC', 'QSO', 'QSL', 'CW', 'SSB', 'RTTY', 'FT8', 'FT4',
             'CAT', 'TCI', 'POTA', 'SOTA', 'IOTA', 'RBN', 'CQ', 'UTC', 'USB', 'LSB')


@dataclass(frozen=True)
class Message:
    id: str
    context: str
    source: str
    comment: str
    numerus: bool
    hint: str
    translator_comment: str
    forms: tuple
    state: str
    supported: bool
    locations: tuple
    span: tuple

    @property
    def phrase(self):
        return self.source, self.comment, self.numerus

    @property
    def empty(self):
        return (self.supported and self.state == 'unfinished'
                and not any(self.forms))

    @property
    def approved(self):
        return self.supported and self.state == '' and bool(self.forms) and all(form.strip() for form in self.forms)


def text_of(element):
    if element is None:
        return ''
    result = element.text or ''
    for child in element:
        if child.tag == 'byte':
            value = child.attrib['value']
            result += chr(int(value[1:], 16) if value.startswith('x') else int(value))
        else:
            result += text_of(child)
        result += child.tail or ''
    return result


def translation_spans(raw):
    """Expat offsets refer to raw bytes, including entities and multibyte UTF-8."""
    parser = expat.ParserCreate()
    stack, spans = [], []
    current = None

    def tag_end(offset):
        quote = None
        for position in range(offset, len(raw)):
            character = raw[position]
            if quote:
                if character == quote:
                    quote = None
            elif character in (34, 39):
                quote = character
            elif character == 62:
                return position + 1
        raise ValueError('Unterminated XML tag')

    def start(name, attrs):
        nonlocal current
        if name == 'message' and stack == ['TS', 'context']:
            current = [None, None]
        elif name == 'translation' and current is not None and stack[-1] == 'message':
            current[0] = parser.CurrentByteIndex
            opening_end = tag_end(current[0])
            if raw[opening_end - 2:opening_end] == b'/>':
                current[1] = opening_end
        stack.append(name)

    def end(name):
        nonlocal current
        offset = parser.CurrentByteIndex
        if name == 'translation' and current is not None and stack[-2] == 'message':
            if current[1] is None:
                current[1] = tag_end(offset)
        elif name == 'message' and current is not None:
            spans.append(tuple(current) if current[0] is not None else (offset, offset))
            current = None
        stack.pop()

    parser.StartElementHandler = start
    parser.EndElementHandler = end
    parser.Parse(raw, True)
    return spans


def read_ts(raw, language):
    root = ET.fromstring(raw)
    ts_language = root.get('language', '')
    if '_' not in language:
        ts_language = ts_language.split('_')[0]
    if language not in LANGUAGES or root.tag != 'TS' or ts_language != language:
        raise ValueError('Unexpected TS language: ' + language)
    spans = iter(translation_spans(raw))
    messages, seen = [], set()
    for context in root.findall('context'):
        name = text_of(context.find('name'))
        for node in context.findall('message'):
            span = next(spans)
            source = text_of(node.find('source'))
            comment = text_of(node.find('comment'))
            numerus = node.get('numerus') == 'yes'
            identity = [name, source, comment, numerus, node.get('id', '')]
            ident = hashlib.sha256(json.dumps(identity, ensure_ascii=False).encode('utf-8')).hexdigest()
            if ident in seen:
                raise ValueError('Duplicate TS message identity: ' + name + ': ' + source)
            seen.add(ident)
            translation = node.find('translation')
            state = translation.get('type', '') if translation is not None else 'unfinished'
            forms = (tuple(text_of(form) for form in translation.findall('numerusform'))
                     if numerus and translation is not None else (text_of(translation),))
            supported = (bool(source) and node.find('.//byte') is None
                         and node.find('.//lengthvariant') is None
                         and (translation is None or translation.get('variants') != 'yes'))
            messages.append(Message(ident, name, source, comment, numerus,
                                    text_of(node.find('extracomment')),
                                    text_of(node.find('translatorcomment')), forms, state,
                                    supported, tuple(dict(loc.attrib) for loc in node.findall('location')), span))
    return messages


def select_new(current, baseline):
    # Context and location changes must not reopen the previous release's backlog.
    previous = {message.phrase for message in baseline}
    return [message for message in current if message.empty and message.phrase not in previous]


FORMATTING = re.compile(
    r'\b[A-Za-z_]\w*(?:\.[A-Za-z_*][\w*]*){2,}(?:=[^\s]+)?'
    r'|<(?:[^<>\"\']|\"[^\"]*\"|\'[^\']*\')*>|[<>]|&(?:#\d+|#x[\da-fA-F]+|[\w]+);'
    r'|\([^()\n]*[?*][^()\n]*\)|\[[^\]\n]*\]|`[^`\n]*`|[\t\r]|;;|[*`#]+'
    r'|(?<!\w)_+|_+(?!\w)')


def layout_signature(text):
    """Keep literal separators and text regions; allow native word order inside regions."""
    result = []
    symbols = ''.join(sorted({char for char in text if unicodedata.category(char).startswith('S') or char == '•'}))
    pattern = re.compile(FORMATTING.pattern + ('|[' + re.escape(symbols) + ']' if symbols else ''))
    for line in text.split('\n'):
        if re.match(r'\s*(?:SELECT|INSERT|UPDATE|DELETE|PRAGMA|WITH)\b', line):
            result.append(('literal', line))
            continue
        parts, start = [], 0
        for match in list(pattern.finditer(line)) + [None]:
            end = match.start() if match else len(line)
            region = line[start:end]
            if region.strip():
                parts.append(('text', region[:len(region) - len(region.lstrip())],
                              region[len(region.rstrip()):], Counter(re.findall(r'%L?(?:[0-9]{1,2}|n)', region))))
            elif region:
                parts.append(('space', region))
            if match:
                parts.append(('literal', match.group()))
                start = match.end()
        result.append(parts)
    return result


def technical_signature(text):
    # Tags may span lines; retain their complete attributes before line partitioning.
    markup = tuple(match.group() for match in FORMATTING.finditer(text) if match.group().startswith('<'))
    ampersands = re.sub(r'&(?:#[0-9]+|#x[0-9a-fA-F]+|[A-Za-z][A-Za-z0-9]+);', '', text)
    tokens = re.findall(r'<[A-Z][A-Z0-9_]*>|\b[A-Za-z][A-Za-z0-9]*(?:_[A-Za-z0-9]+)+\b|\*\.[A-Za-z0-9]+', text)
    return (Counter(re.findall(r'%L?(?:[0-9]{1,2}|n)', text)), markup, layout_signature(text),
            ampersands.count('&&'), ampersands.replace('&&', '').count('&'),
            text.count('\n'), Counter(tokens))


def validate_forms(source, numerus, forms, language):
    expected = LANGUAGES[language][1] if numerus else 1
    if not isinstance(forms, (list, tuple)) or len(forms) != expected:
        raise ValueError(f'Expected {expected} translation forms')
    signature = technical_signature(source)
    names = [name for name in PROTECTED if re.search(r'(?<!\w)' + re.escape(name) + r'(?!\w)', source)]
    # Chinese prose can adjoin ASCII technical names without spaces.
    word_characters = '[A-Za-z0-9_]' if language == 'zh_CN' else r'\w'
    for target in forms:
        if not isinstance(target, str) or not target.strip():
            raise ValueError('Empty or invalid translation form')
        if re.search(r'[\x00-\x08\x0b\x0c\x0e-\x1f\ud800-\udfff\ufffe\uffff]', target):
            raise ValueError('Translation contains characters forbidden in XML')
        if technical_signature(target) != signature:
            raise ValueError('Formatting, literals, placeholders, fields or mnemonics changed')
        for name in names:
            pattern = '(?<!' + word_characters + ')' + re.escape(name) + '(?!' + word_characters + ')'
            if len(re.findall(pattern, target)) != len(re.findall(pattern, source)):
                raise ValueError('Protected name changed: ' + name)


def patch_ts(raw, replacements, language):
    if not replacements:
        return raw
    rows = {message.id: message for message in read_ts(raw, language)}
    if not set(replacements) <= rows.keys():
        raise ValueError('Unknown translation IDs')
    edits = []
    for ident, forms in replacements.items():
        message = rows[ident]
        if not message.empty:
            raise ValueError('Refusing to overwrite an existing or unsupported translation')
        validate_forms(message.source, message.numerus, forms, language)
        body = (''.join('<numerusform>' + html.escape(form, quote=False) + '</numerusform>' for form in forms)
                if message.numerus else html.escape(forms[0], quote=False))
        replacement = ('<translation type="unfinished">' + body + '</translation>').encode('utf-8')
        edits.append((*message.span, replacement))
    for start, end, replacement in sorted(edits, reverse=True):
        raw = raw[:start] + replacement + raw[end:]
    after = {message.id: message for message in read_ts(raw, language)}
    if after.keys() != rows.keys():
        raise ValueError('Patched TS message identities changed')
    for ident, message in after.items():
        expected = tuple(replacements.get(ident, rows[ident].forms))
        if message.forms != expected:
            raise ValueError('Patched TS failed translation round-trip validation')
    return raw
