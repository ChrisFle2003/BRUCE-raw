#!/usr/bin/env python3
"""Check relative Markdown file links offline; never modify documents.

Run from the project root: python3 scripts/check_local_doc_links.py
Uses the CI documentation inputs: README.md, CONTRIBUTING.md, SECURITY.md,
HISTORY.md, docs/, and benchmarks/. External URLs and fragment anchors are
outside this file-existence check.
"""
import argparse
from pathlib import Path
import re
import string
from urllib.parse import unquote, urlsplit


def mask_fenced_blocks(text):
    """Keep offsets/newlines intact while excluding fenced examples."""
    fence = None
    masked = []
    for line in text.splitlines(keepends=True):
        marker = re.match(r'^\s*(`{3,}|~{3,})', line)
        excluded = fence is not None or marker is not None
        if marker:
            token = marker.group(1)
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
        masked.append(''.join('\n' if char == '\n' else ' ' for char in line)
                      if excluded else line)
    return ''.join(masked)


def read_destination(text, start):
    """Read an angle-delimited or balanced bare Markdown destination."""
    while start < len(text) and text[start].isspace():
        start += 1
    angled = start < len(text) and text[start] == '<'
    cursor = start + 1 if angled else start
    content_start = cursor
    depth = 0
    while cursor < len(text):
        char = text[cursor]
        if char == '\\' and cursor + 1 < len(text):
            cursor += 2
            continue
        if angled:
            if char == '>':
                break
            if char in '\n<>':
                return None
        else:
            if char.isspace() or (char == ')' and depth == 0):
                break
            if char == '(':
                depth += 1
            elif char == ')':
                depth -= 1
        cursor += 1
    if cursor == content_start or depth or (angled and cursor == len(text)):
        return None
    target = text[content_start:cursor]
    return re.sub(r'\\([' + re.escape(string.punctuation) + r'])', r'\1', target)


def destinations(text):
    """Yield source line and destination, including continued definitions."""
    text = mask_fenced_blocks(text)
    markers = list(re.finditer(r'\]\(', text))
    markers += list(re.finditer(r'(?m)^ {0,3}\[(?:\\.|[^\]\\\n])+\]:', text))
    for marker in sorted(markers, key=lambda match: match.start()):
        # An escaped closing bracket does not introduce an inline link.
        cursor = marker.start() - 1
        while cursor >= 0 and text[cursor] == '\\':
            cursor -= 1
        if (marker.start() - cursor - 1) % 2:
            continue
        target = read_destination(text, marker.end())
        if target is not None:
            yield text.count('\n', 0, marker.start()) + 1, target


def check(root):
    documents = links = broken = 0
    inputs = [root / name for name in ('README.md', 'CONTRIBUTING.md', 'SECURITY.md', 'HISTORY.md')]
    inputs += list((root / 'docs').rglob('*.md')) + list((root / 'benchmarks').rglob('*.md'))
    for document in sorted(path for path in inputs if path.is_file()):
        relative = document.relative_to(root)
        documents += 1
        for number, target in destinations(document.read_text(encoding='utf-8')):
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path or parsed.path.startswith('/'):
                continue
            links += 1
            if not (document.parent / unquote(parsed.path)).exists():
                broken += 1
                print(f'{relative.as_posix()}:{number}: {target}')
    print(f'documents={documents} links={links} broken={broken}')
    return 1 if broken else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    return check(args.root.resolve())


if __name__ == '__main__':
    raise SystemExit(main())
