#!/bin/sh
#
# $Id: mkrundir.sh,v 1.2 2002/01/27 16:02:55 pem Exp $
#
# Per-Erik Martin (pem@pem.nu) 1998-04-07
#
# Create a run-time directory for mlrate.
#

if [ $# -ne 2 ]; then
  echo 'Usage: mkrundir.sh bindir rundir'
  exit 1
fi

BINDIR=$1
RUNDIR=$2

# scopy src dst
scopy() {
  if [ -f $1 ]; then
    if [ -f $2 ]; then
      echo "$2 already exists"
    else
      cp $1 $2
    fi
  else
    echo "$1 does not exist"
  fi
}

if [ ! -d $RUNDIR ]; then
  mkdir -p $RUNDIR
fi

if [ ! -f $RUNDIR/mlrate ]; then
   ln -s $BINDIR/mlrate $RUNDIR/
fi

if [ ! -f $RUNDIR/anchor ]; then
  ln -s $BINDIR/anchor $RUNDIR/
fi

scopy sample-anchor.conf $RUNDIR/anchor.conf
scopy sample-mlrate.conf $RUNDIR/mlrate.conf
scopy sample-anchor.sh $RUNDIR/anchor.sh
scopy sample-mlrate.sh $RUNDIR/mlrate.sh

chmod u+w,+x $RUNDIR/anchor.sh $RUNDIR/mlrate.sh
