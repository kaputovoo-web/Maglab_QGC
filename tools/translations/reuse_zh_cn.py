#!/usr/bin/env python3
"""Reuse exact, structurally compatible translations from local Qt TS references.

References must be reviewed, trusted catalogs (for example official QGC releases).
The default is a dry run. Existing finished translations are never overwritten.
"""

import argparse
import collections
import copy
import re
import xml.etree.ElementTree as ET
from pathlib import Path
from xml.sax.saxutils import escape


def placeholders(text: str) -> collections.Counter[str]:
    return collections.Counter(re.findall(r"%(?:L?\d+|n)", text))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("references", type=Path, nargs="+")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    known = {}
    for reference in args.references:
        for context in ET.parse(reference).findall("context"):
            for message in context.findall("message"):
                translation = message.find("translation")
                source = message.findtext("source", "")
                if translation is None or translation.get("type") in {"unfinished", "obsolete", "vanished"}:
                    continue
                if list(translation) or message.get("numerus") == "yes":
                    continue
                text = translation.text or ""
                if not re.search(r"[\u3400-\u9fff]", text) or placeholders(source) != placeholders(text):
                    continue
                # Reject malformed or changed rich-text markup, even in upstream references.
                if re.findall(r"</?[A-Za-z][^>]*>", source) != re.findall(r"</?[A-Za-z][^>]*>", text):
                    continue
                key = (context.findtext("name"), source, message.findtext("comment"))
                known.setdefault(key, text)

    path = args.catalog
    original = path.read_text(encoding="utf-8")
    expected = copy.deepcopy(ET.fromstring(original))
    replacements = []
    for context in expected.findall("context"):
        context_name = context.findtext("name")
        context_match = re.search(r"<context>\s*<name>" + re.escape(escape(context_name or "")) + r"</name>.*?</context>", original, re.S)
        if context_match is None:
            raise ValueError(f"Cannot locate context: {context_name}")
        block = context_match.group()
        updated = block
        for message in context.findall("message"):
            translation = message.find("translation")
            source = message.findtext("source", "")
            key = (context_name, source, message.findtext("comment"))
            if translation is None or translation.get("type") != "unfinished" or key not in known:
                continue
            text = known[key]
            if "enumStrings" in message.findtext("extracomment", "") and source.count(",") != text.count(","):
                continue
            # Match the complete message by its decoded lookup key, not a global string replacement.
            matches = []
            for match in re.finditer(r"<message\b[^>]*>.*?</message>", updated, re.S):
                candidate = ET.fromstring(match.group())
                if (candidate.findtext("source"), candidate.findtext("comment")) == key[1:]:
                    matches.append(match)
            if len(matches) != 1:
                raise ValueError(f"Non-unique message: {key}")
            match = matches[0]
            new_message, count = re.subn(
                r'<translation\b[^>]*>.*?</translation>',
                lambda _: "<translation>" + escape(text) + "</translation>",
                match.group(),
                flags=re.S,
            )
            if count != 1:
                raise ValueError(f"Invalid translation element: {key}")
            updated = updated[:match.start()] + new_message + updated[match.end():]
            translation.attrib.clear()
            translation.text = text
        if updated != block:
            replacements.append((context_match.start(), context_match.end(), updated))

    result = original
    for start, end, updated in reversed(replacements):
        result = result[:start] + updated + result[end:]
    if ET.tostring(ET.fromstring(result)) != ET.tostring(expected):
        raise ValueError("Catalog changed outside the intended translations")
    old_pending = original.count('type="unfinished"')
    new_pending = result.count('type="unfinished"')
    print(f"{path}: {old_pending - new_pending} exact translations reusable; {new_pending} unfinished remain")
    if args.apply:
        path.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    main()
