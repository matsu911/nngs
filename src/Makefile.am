bin_PROGRAMS = nngssrv

EXTRA_DIST =

INCLUDES = \
        -I$(top_srcdir)/mlrate/src

LDADD = \
        $(top_srcdir)/mlrate/src/libmlr.a

nngssrv_SOURCES = \
	network.c \
	bm.c \
	channel.c \
	utils.c \
	language.c \
	variable.c \
	legal.c \
	gameproc.c \
	adminproc.c \
	nngsmain.c \
	multicol.c \
	mink.c \
	alias.c \
	censor.c \
	plan.c \
	ip_ban.c \
	chkaddr.c \
	command.c \
	comproc.c \
	gamedb.c \
	playerdb.c \
	emote2.c \
	pending.c \
	udp_commands.c \
	conffile.c \
	mailer.c \
	ladder.c
#avk emote2.c

noinst_HEADERS =

# Remove these files here... they are created locally
DISTCLEANFILES = *~ *%

@NNGS_ALL_WARNINGS@
