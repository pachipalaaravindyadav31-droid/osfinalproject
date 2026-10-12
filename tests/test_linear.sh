#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

./build/pipeflow config/pipeline.conf

grep -qx "ERROR DATABASE CONNECTION FAILED" data/output.txt
grep -qx "ERROR NETWORK TIMEOUT" data/output.txt

echo "PASS: Linear pipeline output is correct."
