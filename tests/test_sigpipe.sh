#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

mkdir -p config data

seq 1 200000 | sed 's/^/record-/' > data/large_input.txt

cat > config/test_sigpipe.conf <<'EOF'
source data/large_input.txt
head -n 1
sink data/output.txt

connect source head
connect head sink
EOF

./build/pipeflow config/test_sigpipe.conf 2> data/sigpipe.log

test "$(wc -l < data/output.txt)" -eq 1

echo "PASS: Early-exiting consumer pipeline completed."
echo "Check data/sigpipe.log for graceful EPIPE handling."
