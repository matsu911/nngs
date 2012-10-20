# $Id: Makefile,v 1.8 2003/05/05 13:27:59 pem Exp $
#
# Makefile for mlrate

BINDIR=/usr/local/bin

#
# Needed on Solaris 2:	-D__EXTENSIONS__
# Needed on Linux:	-D_BSD_SOURCE -D_POSIX_C_SOURCE=2
# Needed on FreeBSD:	(none)
#
# Other options:	-DRDBM_NDBM	use ndbm for rdbm.
#			-DRDBM_GDBM	use gdbm (Note: -lgdbm needed)
#			-DHDBM_GDBM	use gdbm for hdbm (Note: -lgdbm needed)
#			-DNDEBUG	turn assert() off
#			-DPLAYER_MAX_NAMELEN=XX	set the max. name length.
#
# Note: -fomit-frame-pointer with gcc 2.7.2.2 on Intels may not work.
#       (Possibly a compiler bug; may occur with other gcc versions or
#        platforms as well. It's known to crash 'anchor'.)

CC=gcc -ansi

#CFLAGS=-O3 -fomit-frame-pointer -pedantic -Wall -DNDEBUG $(D)
CFLAGS=-O3 -fomit-frame-pointer -pedantic -Wall -D__EXTENSIONS__ -DNDEBUG $(D)
#CFLAGS=-g -pedantic -Wall -D__EXTENSIONS__ -DRDBM_NDBM $(D)

LIBS= #-lgdbm


#
# You should not have to change anything below this.
#

LDFLAGS=-L.
LDLIBS=-lmlr -lm $(LIBS)

PROGS=mlrate anchor rhistory t2res

AUXPROGS=itime rdbmtool old2newres

OBJ=mlr.o player.o game.o rank.o weight.o cmd.o \
    error.o shash.o list.o ilog2.o rdbm.o hdbm.o result.o \
    circular.o

PACKFILES=*.c *.h sample-* Makefile mkrundir.sh CHANGES COPYING FILES README

most:	$(PROGS)

all:	most $(AUXPROGS)


mlrate:	mlrate.o libmlr.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mlrate.o libmlr.a $(LDLIBS)

anchor:	anchor.o libmlr.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ anchor.o libmlr.a $(LDLIBS)

rdbmtool: rdbmtool.o libmlr.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ rdbmtool.o libmlr.a $(LDLIBS)

t2res:	t2res.o libmlr.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ t2res.o libmlr.a $(LDLIBS)

old2newres:	old2newres.o oldresult.o rank.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ old2newres.o oldresult.o rank.o error.o $(LDLIBS)

rhistory:	rhistory.o libmlr.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ rhistory.o libmlr.a $(LDLIBS)

itime:	itime.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ itime.o $(LDLIBS)


libmlr.a:	$(OBJ)
	rm -f libmlr.a
	$(AR) rc libmlr.a $(OBJ)
	ranlib libmlr.a

install: most
	if [ ! -d $(BINDIR) ]; then \
	    mkdir -p $(BINDIR) ; \
	fi
	cp $(PROGS) $(BINDIR)

package:	make.deps
	./mkpackage.sh $(V) $(PACKFILES) make.deps

clean:
	rm -f $(OBJ) $(PROGS:%=%.o) $(AUXPROGS:%=%.o) core libmlr.a

cleanall: clean
	rm -f $(PROGS) $(AUXPROGS) make.deps



TAGS:	$(PROGS:%=%.c) $(AUXPROGS:%=%.c) $(OBJ:%.o=%.c) $(OBJ:%.o=%.h)
	etags $(PROGS:%=%.c) $(AUXPROGS:%=%.c) $(OBJ:%.o=%.c) $(OBJ:%.o=%.h)

make.deps:
	gcc -c -MM $(OBJ:%.o=%.c) $(PROGS:%=%.c) $(AUXPROGS:%=%.c) > make.deps

include make.deps
