#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Checks the commits since <base> (default: origin/nx2) against docs/policies/Legal.md:
# - added source files carry an SPDX license header,
# - no added file looks like keys, firmware or a game dump,
# - no commit names an AI as co-author.
# The SPDX and co-author checks cover this fork's own commits: the non-merge commits on the
# first-parent line. Upstream history that comes in through a merge follows upstream's rules.
# The forbidden-file check covers everything the range adds.
# Usage: tools/license-check.sh [base-ref]

base_ref="${1:-origin/nx2}"
base=$(git merge-base "$base_ref" HEAD)
status=0

if [ "$base" = "$(git rev-parse HEAD)" ]; then
    echo "No commits since $base_ref."
    exit 0
fi

added=$(git diff --diff-filter=A --name-only "$base" HEAD)

forbidden=$(printf '%s\n' "$added" | grep -iE '\.(keys|nsp|nsz|xci|xcz|nca|ncz|nso|nro|kip|tik)$|(^|/)(exefs|romfs|nand|sysdata)/' || true)
if [ -n "$forbidden" ]; then
    echo "error: files that look like keys, firmware or game dumps were added:"
    printf '  %s\n' $forbidden
    status=1
fi

own_commits=$(git rev-list --first-parent --no-merges "$base..HEAD")
own_added=$(for c in $own_commits; do git diff-tree --no-commit-id --name-only --diff-filter=A -r "$c"; done | sort -u)

missing=""
for f in $own_added; do
    case "$f" in
        *.c|*.cc|*.cpp|*.h|*.hpp|*.inc|*.kt|*.java|*.py|*.sh|*.ps1|*.cmake|CMakeLists.txt|*/CMakeLists.txt|\
        *.glsl|*.comp|*.frag|*.vert|*.geom|*.tesc|*.tese|*.fx|*.yml|*.yaml) ;;
        *) continue ;;
    esac
    [ -f "$f" ] || continue
    head -n 15 "$f" | grep -q 'SPDX-License-Identifier:' || missing="$missing $f"
done
if [ -n "$missing" ]; then
    echo "error: added files without an SPDX-License-Identifier header:"
    printf '  %s\n' $missing
    status=1
fi

ai_coauthor=$(git log --first-parent --no-merges --format='%h %s%n%b' "$base..HEAD" |
    grep -iE '^(co-authored-by|signed-off-by):.*(claude|anthropic|copilot|openai|chatgpt|gemini)' || true)
if [ -n "$ai_coauthor" ]; then
    echo "error: an AI is named as co-author or signer; use 'Assisted-by:' instead:"
    printf '%s\n' "$ai_coauthor" | sed 's/^/  /'
    status=1
fi

[ "$status" -eq 0 ] && echo "License check passed for $(git rev-list --count "$base..HEAD") commit(s) since $base_ref."
exit "$status"
