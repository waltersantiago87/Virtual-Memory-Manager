#!/bin/bash

set -eu

make

temp_dir=$(mktemp -d)
trap 'rm -rf "$temp_dir"' EXIT

echo "=== Virtual Memory Manager - Tests ==="

./vm address.txt lru fifo > "$temp_dir/reference.txt"
cmp correct.txt "$temp_dir/reference.txt"

echo "Reference output: OK"

for page in fifo lru; do
    for tlb in fifo lru; do
        ./vm address.txt "$page" "$tlb" \
            > "$temp_dir/$page-$tlb.txt"

        python3 - "$temp_dir/$page-$tlb.txt" <<'PY'
import sys
import re
from pathlib import Path

backing = Path("BACKING_STORE.bin").read_bytes()
addresses = list(map(int, Path("address.txt").read_text().split()))
lines = Path(sys.argv[1]).read_text().splitlines()

assert len(lines) == len(addresses), "Invalid line count"

for address, line in zip(addresses, lines):
    match = re.fullmatch(
        r"Virtual address: (\d+) TLB: (\d+) Physical address: (\d+) Value: (-?\d+)",
        line
    )

    assert match, "Invalid output format"

    virtual, tlb, physical, value = map(int, match.groups())

    expected = int.from_bytes(
        backing[address:address+1], "little", signed=True
    )

    assert virtual == address
    assert 0 <= tlb < 16
    assert 0 <= physical < 32768
    assert physical % 256 == address % 256
    assert value == expected
PY

        echo "Page=$page TLB=$tlb: OK"
    done
done

echo "All tests passed."
