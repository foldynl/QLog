#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

for fedora in 43 44; do
    "$ROOT/packaging/rpm/build-container.sh" "$fedora"
done

"$ROOT/packaging/appimage/build-container.sh"
