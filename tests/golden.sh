#!/usr/bin/env bash
# Golden-screenshot regression test: renders a fixed list of deterministic
# scenarios headless, hashes each screenshot and compares with tests/golden.txt.
#
#   tests/golden.sh [--update] <path/to/open-street-chaves> <path/to/StreetChaves>
#
# Screenshots go to $GOLDEN_OUT (default build/golden-out/, gitignored) so a
# human can diff a failing scenario by eye. --update rewrites tests/golden.txt.
#
# CI CANNOT RUN THIS: the scenarios render the original, copyrighted game data,
# which is not (and must never be) in the repository. Run it locally before and
# after a refactor; golden.txt holds only scenario names and sha256 hashes.
#
# Determinism: `--frames N` makes the game use a fixed RNG seed and exactly one
# tick per frame (see src/main.c), so the output does not depend on wall-clock
# time, frame rate or the video/audio driver. Hashes were generated with the
# SDL3 software renderer; a different SDL version could in principle change
# pixels, in which case regenerate with --update and review.
set -u

update=0
if [ "${1:-}" = "--update" ]; then update=1; shift; fi
if [ $# -ne 2 ]; then
    echo "usage: $0 [--update] <binary> <game data folder>" >&2
    exit 2
fi
bin=$(realpath "$1"); data=$(realpath "$2")
[ -x "$bin" ] || { echo "not executable: $bin" >&2; exit 2; }
[ -d "$data" ] || { echo "not a folder: $data" >&2; exit 2; }

root=$(cd "$(dirname "$0")/.." && pwd)
golden="$root/tests/golden.txt"
out="${GOLDEN_OUT:-$root/build/golden-out}"
rm -rf "$out"; mkdir -p "$out"

export SDL_AUDIODRIVER=dummy
if SDL_VIDEODRIVER=offscreen "$bin" --data "$data" --frames 1 >/dev/null 2>&1; then export SDL_VIDEODRIVER=offscreen
else export SDL_VIDEODRIVER=dummy; fi

# name|frames|arguments   (frames == game ticks; --press is tick:key, held 3 ticks)
scenarios=$(cat <<'EOF'
demo-attract-fight|200|--demo
demo-late|600|--demo
demo-boxes|240|--demo --boxes
demo-es|240|--demo --es
fight-1v2-stage1-start|20|--fight --p1 1 --p2 2 --stage 1
fight-1v2-stage1-walk|90|--fight --p1 1 --p2 2 --stage 1 --press 5:Right --press 40:Right --press 60:Z
fight-3v4-stage3|140|--fight --p1 3 --p2 4 --stage 3 --demo --boxes
fight-5v6-stage5|150|--fight --p1 5 --p2 6 --stage 5 --2p --press 10:Z --press 30:X --press 50:C --press 70:V
fight-7v8-stage7|150|--fight --p1 7 --p2 8 --stage 7 --2p --press 10:C --press 40:Z --press 60:A --press 61:F
fight-9v10-stage9|150|--fight --p1 9 --p2 10 --stage 9 --autofire --demo
fight-11v12-stage11|150|--fight --p1 11 --p2 12 --stage 11 --autofire --demo
fight-13v14-stage12|150|--fight --p1 13 --p2 14 --stage 12 --autofire --demo
fight-15v1-stage2|150|--fight --p1 15 --p2 1 --stage 2 --autofire --demo
fight-2v3-stage4-fireballs|300|--fight --p1 2 --p2 3 --stage 4 --autofire --demo
fight-4v5-stage6-crouch|100|--fight --p1 4 --p2 5 --stage 6 --2p --press 5:Down --press 20:Down --press 25:V
fight-6v7-stage8-jump|80|--fight --p1 6 --p2 7 --stage 8 --2p --press 10:Up --press 11:Z
fight-8v9-stage10-es|200|--fight --p1 8 --p2 9 --stage 10 --es --autofire --demo
fight-battle8-dialogue|120|--fight --p1 1 --p2 2 --battle 8 --stage 1
fight-boxes|180|--fight --p1 1 --p2 2 --stage 1 --boxes --autofire --demo
fight-ko-late|1200|--fight --p1 3 --p2 1 --stage 2 --autofire --demo
screen-attract-0|10|
screen-attract-300|10|--tick 300
screen-attract-900|10|--tick 900
screen-attract-600|10|--tick 600
screen-menu-f1|20|--screen 2
screen-menu-f1-down|40|--screen 2 --press 10:Down --press 20:Down
screen-menu-f2|20|--screen 3
screen-menu-f2-es|40|--screen 3 --es --press 10:Down --press 20:Right
screen-records|30|--screen 8
screen-continue|60|--screen 9
screen-name|30|--screen 12
screen-credits-0|30|--screen 11
screen-credits-800|30|--screen 11 --tick 800
screen-credits-es|30|--screen 11 --tick 400 --es
ending-1|30|--ending 1 --tick 100
ending-3|30|--ending 3 --tick 400
ending-5|30|--ending 5 --tick 800
ending-8-es|30|--ending 8 --tick 300 --es
ending-12|30|--ending 12 --tick 1200
ending-15|30|--ending 15 --tick 600
EOF
)

results=$(mktemp); trap 'rm -f "$results"' EXIT
fail_run=0
while IFS='|' read -r name frames args; do
    [ -n "$name" ] || continue
    shot="$out/$name.bmp"
    # shellcheck disable=SC2086
    if ! "$bin" --data "$data" --frames "$frames" --screenshot "$shot" $args >"$out/$name.log" 2>&1 || [ ! -s "$shot" ]; then
        echo "RUN FAILED: $name (see $out/$name.log)" >&2
        fail_run=1; continue
    fi
    echo "$name $(sha256sum "$shot" | cut -d' ' -f1)" >>"$results"
done <<<"$scenarios"

if [ $update -eq 1 ]; then
    if [ $fail_run -ne 0 ]; then echo "not updating: some scenarios failed to run" >&2; exit 1; fi
    cp "$results" "$golden"
    echo "wrote $golden ($(wc -l <"$golden") scenarios)"
    exit 0
fi

[ -f "$golden" ] || { echo "missing $golden (run with --update first)" >&2; exit 2; }
bad=0
while read -r name hash; do
    want=$(awk -v n="$name" '$1 == n { print $2 }' "$golden")
    if [ -z "$want" ]; then echo "NEW (not in golden.txt): $name"; bad=1
    elif [ "$want" != "$hash" ]; then echo "MISMATCH: $name"; bad=1; fi
done <"$results"
while read -r name _; do
    grep -q "^$name " "$results" || { echo "MISSING (in golden.txt, not run): $name"; bad=1; }
done <"$golden"
if [ $bad -ne 0 ] || [ $fail_run -ne 0 ]; then
    echo "FAIL: compare screenshots in $out" >&2
    exit 1
fi
echo "OK: $(wc -l <"$results") scenarios match tests/golden.txt"
