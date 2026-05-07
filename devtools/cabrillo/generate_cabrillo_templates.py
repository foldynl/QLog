#!/usr/bin/env python3
"""
Generate Cabrillo template SQL for QLog from formats.dat.

Source: https://codeberg.org/kq4mhe/adif2cabrillo/raw/branch/main/resources/formats.dat

Usage:
  python3 generate_cabrillo_templates.py          -- diff since last run
  python3 generate_cabrillo_templates.py --full    -- all templates

Downloads formats.dat automatically. SQL goes to stdout.
State is kept in formats.dat.state next to this script.

Field mapping:
  All field-name mappings live in FIELD_MAP below.

  Format:  "name": (sent_db, rcvd_db, formatter [, sent_label, rcvd_label])
           "name": None                          -- skip this field

  Examples:
    "freq":  ("freq", "freq", "freq_khz"),             -- common field
    "call":  ("station_callsign", "callsign", "upper",  -- with explicit labels
              "My Call", "Call Rcvd"),
    "exch":  ("stx_string", "srx_string", ""),           -- exchange field
    "t":     None,                                       -- ignored

  Fields not listed produce "-- REVIEW" in the SQL output.
  To fix a REVIEW, just add the field name to FIELD_MAP.
"""

import os
import re
import sys
import urllib.request

SOURCE_URL = ("https://codeberg.org/kq4mhe/adif2cabrillo"
              "/raw/branch/main/resources/formats.dat")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DAT_PATH = os.path.join(SCRIPT_DIR, "formats.dat")
STATE_PATH = os.path.join(SCRIPT_DIR, "formats.dat.state")

# ---------------------------------------------------------------------------
# Built-in contest IDs — only these are emitted as builtin templates.
# Top 10 most popular worldwide contests by participation.
# ---------------------------------------------------------------------------
BUILTIN_CONTEST_IDS = {
    "CQ-WW-CW",
    "CQ-WW-SSB",
    "CQ-WPX-CW",
    "CQ-WPX-SSB",
    "CQ-WW-RTTY",
    "ARRL-DX-CW",
    "ARRL-DX-SSB",
    "IARU-HF",
    "ARRL-VHF-JAN",
    "CQ-VHF",
}

# ---------------------------------------------------------------------------
# Field mapping: "name" -> (sent_db, rcvd_db, formatter [, sent_lbl, rcvd_lbl])
#                "name" -> None   means skip
# ---------------------------------------------------------------------------
FIELD_MAP = {
    # Common (non-directional)
    "freq":      ("freq",       "freq",       "freq_khz"),
    "mo":        ("mode",       "mode",       "mode_cabrillo"),
    "date":      ("start_time", "start_time", "date_yyyy_mm_dd"),
    "time":      ("start_time", "start_time", "time_hhmm"),

    # Callsigns
    "call":      ("station_callsign", "callsign", "upper", "My Call", "Call Rcvd"),
    "callsign":  ("station_callsign", "callsign", "upper", "My Call", "Call Rcvd"),

    # RST
    "rst":       ("rst_sent", "rst_rcvd", ""),

    # Gridsquare
    "grid":      ("my_gridsquare", "gridsquare", "upper"),

    # Serial number fields (map to stx / srx)
    "nr":        ("stx", "srx", ""),
    "number":    ("stx", "srx", ""),
    "num":       ("stx", "srx", ""),
    "ser":       ("stx", "srx", ""),
    "stx":       ("stx", "srx", ""),
    "srx":       ("stx", "srx", ""),

    # Exchange fields (map to stx_string / srx_string)
    "exch":      ("stx_string", "srx_string", ""),
    "exchange":  ("stx_string", "srx_string", ""),
    "exc":       ("stx_string", "srx_string", ""),
    "ex1":       ("stx_string", "srx_string", ""),
    "ex2":       ("stx_string", "srx_string", ""),
    "ex3":       ("stx_string", "srx_string", ""),
    "sec":       ("stx_string", "srx_string", ""),
    "p":         ("stx_string", "srx_string", ""),
    "ck":        ("stx_string", "srx_string", ""),
    "zn":        ("stx_string", "srx_string", ""),
    "age":       ("stx_string", "srx_string", ""),
    "name":      ("stx_string", "srx_string", ""),
    "qth":       ("stx_string", "srx_string", ""),

    # Transmitter ID (fixed value, not from DB)
    "t":         ("", "", "transmitter_id", "t", "t"),
}

DEFAULT_FIELDS = ["freq", "mo", "date", "time",
                  "call", "rst", "exch", "call", "rst", "exch", "t"]


# ---------------------------------------------------------------------------
# Mapping logic
# ---------------------------------------------------------------------------
def map_field(name, is_sent, is_common):
    """Map field name to (db_field, formatter, label, needs_review) or None to skip."""
    low = name.lower()
    entry = FIELD_MAP.get(low)

    if entry is None and low in FIELD_MAP:
        return None  # explicitly skipped

    if entry is None:
        # Unknown field -> default to exchange
        db = "stx_string" if is_sent else "srx_string"
        sfx = "Sent" if is_sent else "Rcvd"
        return (db, "", f"{name.title()} {sfx}", False)

    sent_db, rcvd_db, formatter = entry[0], entry[1], entry[2]
    db = sent_db if is_sent else rcvd_db

    if len(entry) >= 5:
        label = entry[3] if is_sent else entry[4]
    elif is_common:
        label = name.title()
    else:
        sfx = "Sent" if is_sent else "Rcvd"
        label = f"{name.title()} {sfx}"

    return (db, formatter, label, False)


def guess_mode(name):
    u = name.upper()
    if u.endswith("-CW"):                          return "CW"
    if u.endswith("-SSB") or u.endswith("-PH"):     return "SSB"
    if "RTTY" in u or u.endswith("-RY"):            return "RTTY"
    if "PSK" in u or "DIGI" in u or "FT8" in u:    return "DIGI"
    if u.endswith("-FM"):                           return "FM"
    return "MIXED"


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------
def parse_names(line):
    names = []
    for part in line.strip().split(","):
        m = re.match(r"^\s*([\w-]+)\s*\(([\w-]+)\)\s*$", part)
        if m:
            names += [m.group(1), m.group(2)]
        else:
            clean = re.sub(r"\s*\(.*?\)", "", part).strip()
            if clean:
                names.append(clean)
    return names


def is_name(line):
    s = line.strip()
    if not s or s.startswith(("QSO:", "****", "[x")):
        return False
    if s[0].isdigit() and s.replace(" ", "").isdigit():
        return False
    return "info sent" not in s.lower()


def is_format(line):
    s = line.strip()
    return (s.startswith(("QSO:", "****"))
            and ("***" in s or "yyyy" in s or "nnnn" in s))


def is_header(line):
    s = line.strip().lower()
    return (s.startswith("qso:") and not is_format(line)
            and ("freq" in s or "call" in s))


def tokens(line):
    t = line.split()
    return t[1:] if t and t[0] in ("QSO:", "****") else t


def esc(s):
    return s.replace("'", "''")


def make_sig(fld, fmt_tok):
    return " ".join(f"{f}:{len(t)}" for f, t in zip(fld, fmt_tok)
                    if FIELD_MAP.get(f.lower()) is not None or f.lower() not in FIELD_MAP)


def load_state(path):
    state = {}
    if not os.path.isfile(path):
        return state
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if line:
                parts = line.split("\t", 1)
                state[parts[0]] = parts[1] if len(parts) > 1 else ""
    return state


def save_state(path, state):
    with open(path, "w") as f:
        for cid in sorted(state):
            f.write(f"{cid}\t{state[cid]}\n")


def download():
    print(f"Downloading {SOURCE_URL} ...", file=sys.stderr)
    urllib.request.urlretrieve(SOURCE_URL, DAT_PATH)
    print(f"Saved to {DAT_PATH}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    full = "--full" in sys.argv

    download()

    prev = {} if full else load_state(STATE_PATH)

    with open(DAT_PATH, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    new_state = {}
    new_count = changed_count = skip_count = 0
    # Collect: (contest_name, display_name, mode, [(pos, db, width, fmt, lbl), ...])
    emit_list = []
    i = 0

    while i < len(lines):
        line = lines[i].rstrip()
        if not is_name(line):
            i += 1
            continue

        names = parse_names(line)
        if not names or names[0].startswith("Standard"):
            i += 1
            continue

        hdr = fmt = None
        j = i + 1
        while j < min(i + 10, len(lines)):
            l = lines[j].rstrip()
            if is_name(l):
                break
            if not hdr and is_header(l):  hdr = l
            if not fmt and is_format(l):  fmt = l
            j += 1

        if not fmt:
            i = j
            continue

        fmt_tok = tokens(fmt)
        fld = tokens(hdr) if hdr else list(DEFAULT_FIELDS[:len(fmt_tok)])
        while len(fld) < len(fmt_tok):
            fld.append("unknown")
        n = min(len(fld), len(fmt_tok))
        fld = fld[:n]
        fmt_tok = fmt_tok[:n]

        sig = make_sig(fld, fmt_tok)

        # Only process ADIF contest IDs
        names = [n for n in names if n in BUILTIN_CONTEST_IDS]
        if not names:
            i = j
            continue

        for name in names:
            new_state[name] = sig

        # Classify: new / changed / unchanged
        emit_names = []
        for name in names:
            if name not in prev:
                emit_names.append(name)
                new_count += 1
            elif prev[name] != sig:
                emit_names.append(name)
                changed_count += 1
            else:
                skip_count += 1

        if not emit_names:
            i = j
            continue

        # Find sent/rcvd boundary at second call/callsign
        calls = [k for k in range(n) if fld[k].lower() in ("call", "callsign")]
        sent0 = calls[0] if calls else n
        rcvd0 = calls[1] if len(calls) >= 2 else n

        # Build columns
        cols = []
        pos = 0
        for k in range(n):
            is_common = k < sent0
            is_sent = k < rcvd0
            mapped = map_field(fld[k], is_sent, is_common)
            if mapped is None:
                continue
            pos += 1
            db, fmtr, lbl, _ = mapped
            cols.append((pos, db, len(fmt_tok[k]), fmtr, lbl))

        for name in emit_names:
            emit_list.append((name, name.replace("-", " "), guess_mode(name), cols))

        i = j

    # -- Generate SQL -------------------------------------------------------
    out = ["-- Generated by devtools/cabrillo/generate_cabrillo_templates.py",
           "-- Source: " + SOURCE_URL, ""]

    if not emit_list:
        out.append("-- Nothing to emit.")
    else:
        for cid, display, mode, cols in emit_list:
            e = esc(cid)
            ed = esc(display)
            out.append(
                f"DELETE FROM cabrillo_templates "
                f"WHERE name = '{ed}';")
            out.append(
                f"INSERT INTO cabrillo_templates "
                f"(name, contest_name_for_header, "
                f"default_category_mode) VALUES "
                f"('{ed}', '{e}', '{esc(mode)}');")
            for p, db, w, fmtr, lbl in cols:
                out.append(
                    f"INSERT INTO cabrillo_template_columns "
                    f"(template_id, position, db_field, width, formatter, label) "
                    f"SELECT id, {p}, '{esc(db)}', {w}, '{esc(fmtr)}', "
                    f"'{esc(lbl)}' FROM cabrillo_templates "
                    f"WHERE name = '{ed}';")
            out.append("")

    save_state(STATE_PATH, new_state)
    print("\n".join(out))

    parts = []
    if full:
        parts.append(f"{new_count} templates [full]")
    else:
        parts.append(f"{new_count} new, {changed_count} changed, "
                     f"{skip_count} unchanged")
    print(", ".join(parts), file=sys.stderr)


if __name__ == "__main__":
    main()
