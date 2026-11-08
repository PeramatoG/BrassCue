#!/usr/bin/env python3
"""Small tracked-source and layer guard; no automatic policy growth."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
policy = json.loads((root / 'tests/source_file_size_policy.json').read_text())
paths = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
errors = []
for name in filter(None, paths):
    path = root / name
    if path.suffix not in policy['suffixes']:
        continue
    text = path.read_text(encoding='utf-8')
    limit = policy['exceptions'].get(name, policy['max_lines'])
    if len(text.splitlines()) > limit:
        errors.append(f'{name}: exceeds {limit} physical lines')
    if name.startswith('bridge/src/') and ('#include <wx/' in text or '#include <Arduino' in text):
        errors.append(f'{name}: Core cannot depend on GUI or Arduino')
    if name.startswith(('build/', 'out/', '.vs/')):
        errors.append(f'{name}: tracked build output')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print('Tracked-source size, hygiene and Core dependency checks passed')
