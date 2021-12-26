#!/bin/sh
set -e

if [ $# -lt 1 ]; then
	echo "usage: $0 [version]"
	exit 1
fi

VERSION=$1

mkdir -p build/p-8-$VERSION
cp -a .gitignore common COPYING doc examples handler init.sh m2adapter Makefile proxy p-8 qzmq README.md runner tools build/p-8-$VERSION
rm -rf build/p-8-$VERSION/qzmq/.git build/p-8-$VERSION/common/.git
cd build
tar jcvf p-8-$VERSION.tar.bz2 p-8-$VERSION
