# Dependency maintenance

Dependency checks are read-only. Updates must be requested explicitly so that
each dependency can be reviewed and committed separately.

## Common interface

List all components that can be updated:

```bash
./devtools/dependencies/manage.sh list
```

Check all tracked dependencies:

```bash
./devtools/dependencies/manage.sh check
```

The exit status is `0` when everything is current, `2` when an update is
available, and `1` when a check failed. The command checks vendored libraries,
AppImage source and deployment dependencies, and timezone data.

Update one component explicitly:

```bash
./devtools/dependencies/manage.sh update hamlib
```

The common interface delegates to the component-specific scripts documented
below. It does not support updating all components at once, so each update can
be reviewed and committed separately.

## Vendored libraries

Check or update csv-parser:

```bash
./devtools/3rdparty/update.sh --check csv-parser
./devtools/3rdparty/update.sh --update csv-parser
```

Check or update ZoneDetect:

```bash
./devtools/3rdparty/update.sh --check zonedetect
./devtools/3rdparty/update.sh --update zonedetect
```

The updater changes the vendored files and `3rdparty/versions.sh`.

## AppImage dependencies

Check every pinned AppImage source and deployment dependency:

```bash
./packaging/appimage/update-dependencies.sh --check all
```

Check or update one component:

```bash
./packaging/appimage/update-dependencies.sh --check hamlib
./packaging/appimage/update-dependencies.sh --update hamlib

./packaging/appimage/update-dependencies.sh --check qtkeychain
./packaging/appimage/update-dependencies.sh --update qtkeychain

./packaging/appimage/update-dependencies.sh --check linuxdeploy
./packaging/appimage/update-dependencies.sh --update linuxdeploy

./packaging/appimage/update-dependencies.sh --check linuxdeploy-plugin-qt
./packaging/appimage/update-dependencies.sh --update linuxdeploy-plugin-qt
```

Hamlib and QtKeychain are tracked by release version and SHA-256 checksum.
linuxdeploy and its Qt plugin publish mutable `continuous` assets, so their
x86_64 and aarch64 checksums are compared directly. The updater changes
`packaging/appimage/versions.sh` and `packaging/appimage/checksums.sha256`.

Ubuntu packages in `packaging/appimage/packages.txt` and
`packaging/appimage/runtime/bundle-packages.txt` do not have versions pinned in
the repository. Rebuild the AppImage to pull the current Ubuntu 22.04 image and
install the current package updates:

```bash
./packaging/appimage/build-container.sh
```

## Timezone data

Check all timezone builder inputs:

```bash
./3rdparty/bertoldvdb/zonedetect/database/update.sh --check all
```

Check or update one input:

```bash
./3rdparty/bertoldvdb/zonedetect/database/update.sh --check timezone-boundary
./3rdparty/bertoldvdb/zonedetect/database/update.sh --update timezone-boundary

./3rdparty/bertoldvdb/zonedetect/database/update.sh --check natural-earth
./3rdparty/bertoldvdb/zonedetect/database/update.sh --update natural-earth
```

An update downloads the selected input, verifies all pinned checksums, rebuilds
the database, and replaces QLog's `res/data/timezone21.bin`. Use
`./3rdparty/bertoldvdb/zonedetect/database/update.sh --update all` to update
both inputs together.

The update requires a C++ compiler, shapelib development files, `curl`,
`unzip`, and `zip`. The generated data is built in a temporary directory. The
updater changes `3rdparty/bertoldvdb/zonedetect/database/versions.sh` and
`res/data/timezone21.bin`.

The low-level builder uses the pinned release by default and is still available
for manual regeneration:

```bash
./3rdparty/bertoldvdb/zonedetect/database/builder/makedb.sh
```

Always inspect the resulting diff and build the affected target before
committing an update.
