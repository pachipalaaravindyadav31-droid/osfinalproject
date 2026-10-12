#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p data
cat > config/test_fanout.conf <<'CONFIG'
source data/input.txt
transform uppercase
sink_a data/fanout_a.txt
sink_b data/fanout_b.txt

connect source transform
connect transform sink_a
connect transform sink_b
CONFIG

./build/pipeflow config/test_fanout.conf

diff -u data/fanout_a.txt data/fanout_b.txt
test "$(wc -l < data/fanout_a.txt)" -eq 6
test "$(wc -l < data/fanout_b.txt)" -eq 6

echo "PASS: Fan-out sends all records to both sinks."
