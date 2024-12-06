#!/bin/sh
set -e

if [ $# -lt 1 ]; then
	echo "usage: $0 [version]"
	exit 1
fi

VERSION=$1

DESTDIR=build/p-8-$VERSION

mkdir -p $DESTDIR

cp -a .gitignore build.rs Cargo.lock Cargo.toml CHANGELOG.md examples LICENSE postbuild README.md src tools $DESTDIR

sed -i -e "s/^version = .*/version = \"$VERSION\"/g" $DESTDIR/Cargo.toml

cd $DESTDIR
mkdir -p .cargo
cat >.cargo/config.toml <<EOF
[source.crates-io]
replace-with = "vendored-sources"

[source.vendored-sources]
directory = "vendor"
EOF
cargo vendor
cd ..

tar jcvf p-8-$VERSION.tar.bz2 p-8-$VERSION
