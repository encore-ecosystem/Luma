#!/bin/sh

set -eu

package_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output="$package_root/target/backend_contract"
mkdir -p "$(dirname -- "$output")"

/usr/bin/clang -fobjc-exceptions -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-non-prototype \
    "$package_root/tests/backend_contract.m" \
    -framework Foundation -framework Metal -framework QuartzCore \
    -o "$output"

exec "$output"
