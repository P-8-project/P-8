#!/bin/sh
set -e

if [ $# -lt 1 ]; then
	echo "usage: $0 [version]"
	exit 1
fi

VERSION=$1

mkdir -p build/p-8-$VERSION
cp -a .gitignore CHANGELOG.md configure COPYING docs examples p-8.pro p-8.qc qcm README.md src tools build/p-8-$VERSION
rm -rf build/p-8-$VERSION/src/corelib/qzmq/.git build/p-8-$VERSION/src/corelib/common/.git
echo $VERSION > build/p-8-$VERSION/version
cd build
tar jcvf p-8-$VERSION.tar.bz2 p-8-$VERSION
