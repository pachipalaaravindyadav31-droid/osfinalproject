#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

seq -f 'record-%06g' 1 20000 > data/large_input.txt

cat > config/test_backpressure.conf <<'EOF'
source data/large_input.txt
transform uppercase
sink data/output.txt

connect source transform
connect transform sink
EOF

echo "Running pipeline with a deliberately slow sink..."

PIPEFLOW_SINK_DELAY_MS=5 \
    ./build/pipeflow config/test_backpressure.conf \
    2> data/backpressure.log

echo "Stage metrics:"
grep '\[Metrics\]' data/backpressure.log

echo "Check source write_time_ms and total execution time."
echo "PASS: Slow-sink experiment completed."
