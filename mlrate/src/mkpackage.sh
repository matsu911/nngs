#!/bin/sh
#
# $Id: mkpackage.sh,v 1.3 2002/01/27 20:02:59 pem Exp $
#
# Per-Erik Martin (pem@pem.nu) 2002-01-27
#

if [ $# -eq 0 ]; then
    echo "mkpackage.sh version files..."
    exit 1
fi

version=$1
shift

if [ -z "$version" ]; then
    echo "No version given"
    exit 1
fi

x=`echo $version | sed 's/[0-9][0-9]*\.[0-9][0-9]*[.0-9]*/x/`

if [ "$x" != x ]; then
    echo "Bad version format"
    exit 1
fi

pdir=mlrate-$version
pack=$pdir.tar.gz

if [ -d $pdir ]; then
    echo "Directory $pdir already exists"
    exit 1
fi

if [ -f $pack ]; then
    echo "File $pack already exists"
    exit 1
fi

mkdir $pdir

cp $* $pdir

chmod -w $pdir/*
if [ -f $pdir/Makefile ]; then
    chmod u+w $pdir/Makefile
fi
if [ -f $pdir/make.deps ]; then
    chmod u+w $pdir/make.deps
fi

tar czvf $pack $pdir

if [ $? -eq 0 -a -f $pack ]; then
    echo
    echo "$pack created"
    rm -rf $pdir
fi
