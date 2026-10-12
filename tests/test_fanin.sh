#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p data
cat > config/test_fanin.conf <<'CONFIG'
source data/input.txt
cat data/input.txt
sink data/fanin.txt

connect source sink
connect cat sink
CONFIG

./build/pipeflow config/test_fanin.conf

test "$(wc -l < data/fanin.txt)" -eq 12

while IFS= read -r line; do
    count=$(grep -Fxc -- "$line" data/fanin.txt || true)
    if [ "$count" -ne 2 ]; then
        echo "FAIL: Expected each input record twice: $line"
        exit 1
    fi
done < data/input.txt

echo "PASS: Fan-in preserves all records from both input streams."
