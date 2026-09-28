# Database files

Please download the database files [here](https://cdn.bertold.org/zonedetect/db/db.zip) or create them using the builder.

To use the builder, first install [shapelib](https://github.com/OSGeo/shapelib), then:

    cd builder
    ./makedb.sh
    
This will create database files in `out` and `out_v1`, as well as a `db.zip` containing both directories.

The builder uses the release and checksums pinned in `versions.sh`. Dependency
check and update commands are documented in
`../../../../devtools/dependencies/README.md`.

The files in the folder out\_v1/ use a newer format and use less space to encode the same information.

QLog uses out\_v1/timezone21.bin
