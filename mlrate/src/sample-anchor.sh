#!/bin/sh
#
# $Id: sample-anchor.sh,v 1.2 2002/01/27 17:56:52 pem Exp $
#

ACONF=anchor.conf
ANCHORS=anchors
SVALF=sval
RDBF=ratdb
LOGF=anchor.log

shiftfiles() {
  i=9
  while [ $i -gt 0 ] ; do
    n=`expr $i - 1`
    if [ -f $1.$n ]; then
      mv $1.$n $1.$i
    fi
    i=$n
  done
  if [ -f $1 ]; then
    mv $1 $1.0
  fi
}

shiftfiles $SVALF
cp $SVALF.0 $SVALF

if [ ! -f $ANCHORS ]; then
  echo 0 > $ANCHORS
fi

shiftfiles $ANCHORS

if [ ! -f $LOGF ]; then
  echo 'Initial run' > $LOGF
fi

shiftfiles $LOGF

./anchor $ACONF < $ANCHORS.0 > $ANCHORS 2> $LOGF
