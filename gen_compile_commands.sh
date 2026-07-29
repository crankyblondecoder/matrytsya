#!/bin/bash
set -e

# Dependencies are generated before bear is involved, so the wrapped build below issues nothing but
# real compiler invocations. See the "dep" target in build/makefile_std_dep for why that matters.
make -C ./build clean
make -C ./build dep
bear -- make -C ./build

# Backstop for when the database is produced some other way, for instance "bear -- make" on its own or a
# header edit that regenerates a dep file mid build. A dependency scan ("g++ -MM ...") that reaches the
# database wins bear's first-seen duplicate filter and hides that file's real compile command, leaving
# clangd with no -std=c++20. -MD and -MMD are deliberately not filtered; those accompany a real compile.
python3 - <<'PY'
import json

path = 'compile_commands.json'

with open(path) as f:
    db = json.load(f)

kept = [e for e in db
        if not {'-M', '-MM'} & set(e.get('arguments') or (e.get('command') or '').split())]

if len(kept) != len(db):
    print(f'stripped {len(db) - len(kept)} dependency-scan entries from {path}')

with open(path, 'w') as f:
    json.dump(kept, f, indent=2)
PY
