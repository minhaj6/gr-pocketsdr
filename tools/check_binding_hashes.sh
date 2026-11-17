#!/bin/sh
# Print each binding whose stored header hash does not match the header. Exit 1 if any.
status=0
for cc in python/*/bindings/*_python.cc; do
    header=$(sed -n 's/.*BINDTOOL_HEADER_FILE(\([^)]*\)).*/\1/p' "$cc")
    stored=$(sed -n 's/.*BINDTOOL_HEADER_FILE_HASH(\([^)]*\)).*/\1/p' "$cc")
    [ -z "$header" ] || [ "$stored" = "0" ] && continue
    path=$(find include -name "$header" | head -n 1)
    actual=$(md5sum "$path" | cut -d' ' -f1)
    if [ "$stored" != "$actual" ]; then
        echo "$cc: stored $stored, $path is $actual"
        status=1
    fi
done
exit $status
