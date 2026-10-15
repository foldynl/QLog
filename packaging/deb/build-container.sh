#!/usr/bin/env bash

exec "$(dirname "$0")/../package-container.sh" deb "$@"
