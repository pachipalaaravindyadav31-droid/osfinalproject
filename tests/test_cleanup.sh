#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

for i in $(seq 1 10); do
    ./build/pipeflow config/pipeline.conf \
        > /dev/null 2> /dev/null

    if ps -eo pid,ppid,stat,args |
        grep -E '[b]uild/(pipeflow|source|filter|transform|sink)' |
        grep -q .; then
        echo "FAIL: PipeFlow process remains after run $i"
        ps -eo pid,ppid,stat,args |
            grep -E '[b]uild/(pipeflow|source|filter|transform|sink)'
        exit 1
    fi
done

grep -qx "ERROR DATABASE CONNECTION FAILED" data/output.txt
grep -qx "ERROR NETWORK TIMEOUT" data/output.txt

echo "PASS: Ten start/stop cycles completed."
echo "PASS: No matching PipeFlow processes remain."
