#!/bin/sh
#
# $Id: sample-mlrate.sh,v 1.2 2002/01/27 17:56:53 pem Exp $
#

ROOT=/some/ratings/dir
SVAL=sval
RESULTS=results
GRAPH=graph
LOG=mlrate.log
RATDB=ratdb
CONF=mlrate.conf

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

cd $ROOT

if [ -f $RESULTS ]; then

  shiftfiles $GRAPH
  shiftfiles $LOG

  date '+%n%Y-%m-%d %H:%M' > $LOG

  if [ -f $RATDB ]; then
    rm $RATDB
  fi
  if [ -f $RATDB.dir -a -f $RATDB.pag ]; then
    rm $RATDB.dir $RATDB.pag
  fi

  if [ ! -f $SVAL ]; then
    echo 26.55 > $SVAL
  fi

  nice ./mlrate -v - $CONF >> $LOG 2>&1

#  ./anchor.sh

fi
