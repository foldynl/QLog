#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
STATUS=0

usage()
{
    echo "Usage: $0 check" >&2
    echo "       $0 list" >&2
    echo "       $0 update COMPONENT" >&2
}

list_components()
{
    printf '%s\n' \
        csv-parser \
        zonedetect \
        hamlib \
        qtkeychain \
        linuxdeploy \
        linuxdeploy-plugin-qt \
        timezone-boundary \
        natural-earth
}

run_check()
{
    local title="$1"
    local result
    shift

    echo
    echo "== $title =="
    if "$@"; then
        return
    else
        result=$?
    fi

    case "$result" in
        2)
            [[ "$STATUS" -eq 1 ]] || STATUS=2
            ;;
        *)
            STATUS=1
            ;;
    esac
}

COMMAND="${1:-}"
case "$COMMAND" in
    check)
        [[ "$#" -eq 1 ]] || { usage; exit 1; }
        run_check "Vendored libraries" \
            "$ROOT/devtools/3rdparty/update.sh" --check all
        run_check "AppImage tools and source dependencies" \
            "$ROOT/packaging/appimage/update-dependencies.sh" --check all
        run_check "Timezone data" \
            "$ROOT/3rdparty/bertoldvdb/zonedetect/database/update.sh" --check
        exit "$STATUS"
        ;;
    list)
        [[ "$#" -eq 1 ]] || { usage; exit 1; }
        list_components
        ;;
    update)
        [[ "$#" -eq 2 ]] || { usage; exit 1; }
        case "$2" in
            csv-parser|zonedetect)
                exec "$ROOT/devtools/3rdparty/update.sh" --update "$2"
                ;;
            hamlib|qtkeychain|linuxdeploy|linuxdeploy-plugin-qt)
                exec "$ROOT/packaging/appimage/update-dependencies.sh" --update "$2"
                ;;
            timezone-boundary|natural-earth)
                exec "$ROOT/3rdparty/bertoldvdb/zonedetect/database/update.sh" \
                    --update "$2"
                ;;
            *)
                echo "Unknown component: $2" >&2
                echo "Available components:" >&2
                list_components >&2
                exit 1
                ;;
        esac
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage
        exit 1
        ;;
esac
