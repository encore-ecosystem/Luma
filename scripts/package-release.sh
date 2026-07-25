#!/usr/bin/env sh
set -eu

version="${1:-}"
if [ -z "$version" ]; then
    echo "usage: scripts/package-release.sh <version>" >&2
    exit 2
fi

manifest_version="$(sed -n 's/^version = "\([^"]*\)"/\1/p' encore.toml | head -n 1)"
if [ "$version" != "$manifest_version" ]; then
    echo "version mismatch: encore.toml contains $manifest_version" >&2
    exit 1
fi

archive="luma-$version.tar.gz"
tar \
    --sort=name \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    --mtime='UTC 1970-01-01' \
    -czf "$archive" \
    ./encore.toml ./encore.lock ./README.md ./CHANGELOG.md ./LICENSE \
    ./build.enq ./runtime.c ./src ./workspace ./assets ./tests

tar -tzf "$archive"
sha256sum "$archive"
