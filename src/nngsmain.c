/* nngsmain.c
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996 Erik Van Riper (geek@nngs.cosmic.org)
    and John Tromp (tromp@daisy.uwaterloo.ca/tromp@cwi.nl)

    Adapted from:
    fics - An internet chess server.
    Copyright (C) 1993  Richard V. Nash

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <stdlib.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <pwd.h>
#include <grp.h>
#include <netdb.h>

#define __USE_XOPEN 1
#include <unistd.h>
#undef __USE_XOPEN

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "common.h"
#include "network.h"
#include "command.h"
#include "channel.h"
#include "playerdb.h"
#include "adminproc.h"
#include "utils.h"
#include "ladder.h"
#include "emote2.h"
#include "mink.h"
#include "ip_ban.h"

/* Scoreboard */
int Ladder9, Ladder19, num_19, num_9, completed_games,
       num_logins, num_logouts, new_players;
unsigned long bytes_sent = 0UL;
unsigned long bytes_received = 0UL;

static char confname[1024] = "nngs.cnf";

void player_array_init(void);
void player_init(void);
static void usage(char *);

static void usage(char *progname);
static void GetArgs(int argc, char *argv[]);
static void main_event_loop(void);
static void read_ban_ip_list(void);
static int all_the_internets(void);
static int daemonise(void);

	/* signal handlers */
static void TerminateServer(int sig);
static void BrokenPipe(int sig);
static void reapchild(int sig);

static void usage(char *progname) {
  fprintf(stderr, "Usage: %s [-c <conffilename>] [-h]\n", progname);
  fprintf(stderr, "\t\t-h\t\tDisplay this information.\n");
  main_exit(1);
}

static void GetArgs(int argc, char *argv[])
{
  int i;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'c':
	if (i == argc - 1) usage(argv[0]);
	i++;
	if (sscanf(argv[i], "%s", &confname[0]) != 1)
	  usage(argv[0]);
	break;
      case 'h':
	usage(argv[0]);
	break;
      }
    } else {
      usage(argv[0]);
    }
  }
}


static void main_event_loop(void)
{
  net_select(1);
}


static void TerminateServer(int sig)
{
  fprintf(stderr, "Got signal %d\n", sig);
  Logit("Got signal %d", sig);
  TerminateCleanup();
  net_closeAll();
  main_exit(1);
}

static void reapchild(int sig)
{
  int pid, status;
  struct rusage usag;
  fprintf(stderr, "Got signal %d\n", sig);
  pid = wait3(&status, WNOHANG, &usag);
  Logit("Reaped Child %d", pid);
	/* Obsolete: reinstall signal handler. Won't harm */
  signal(SIGCHLD, reapchild);
}


static void BrokenPipe(int sig)
{
  static time_t lasttime=0;
  static unsigned count=0;

	/* Obsolete: reinstall signal handler. Won't harm */
  signal(SIGPIPE, BrokenPipe);
  count++;
  if (globclock.time - lasttime > 10) {
    Logit("Got %u Broken Pipes in %d seconds: sig %d\n"
	, count, globclock.time - lasttime, sig);
    lasttime=globclock.time;
    count=0;
    }
}


int main(int argc, char *argv[])
{
  FILE *fp;

#if USING_DMALLOC
  dmalloc_debug(1);
#endif

	/* start the clock (which is used by the Logit fnc) */
  (void) refetch_ticker();
  GetArgs(argc, argv);
  if (conf_file_read(confname)) {
    Logit("Failed to read config file \"%s\"", confname);
    strcpy(confname, "./nngs.cnf");
    conf_file_write(confname);
    Logit("Created \"%s\"", confname);
  }
  Logit("Starting %s (%s %s) From: %s"
  , conffile.version_string, conffile.compile_date, conffile.compile_time, confname);
  if (daemonise()) {
    Logit("Failed to daemonise, giving up");
    main_exit(1);
  }
  conf_file_write("written.cnf");
  signal(SIGTERM, TerminateServer);
  signal(SIGINT, TerminateServer);
#if 0
  signal(SIGPIPE, SIG_IGN);
#else
  signal(SIGPIPE, BrokenPipe);
#endif
  signal(SIGCHLD, reapchild);
  mink_init();
  startuptime = time(NULL);
  srand(startuptime);
  read_ban_ip_list();
  if (!all_the_internets() ) {
    fprintf(stderr, "Network initialize failed on ports %s.\n"
    , conffile.server_ports);
    main_exit(1);
  }
  player_high = 0;
  game_high = 0;
  bytes_sent = 0;
  bytes_received = 0;

#ifdef SGI
  /*mallopt(100, 1);*/  /* Turn on malloc(3X) debugging (Irix only) */
#endif
  command_init();
  EmoteInit(conffile.emotes_file);
  help_init();
  /*Logit("commands_init()");*/
  commands_init();
  /*Logit("channel_init()");*/
  channel_init();
  /*Logit("player_array_init()");*/
  player_array_init();
  player_init();
  ladder_init(NUM_LADDERS);
  Ladder9 = ladder_new(LADDERSIZE);
  Ladder19 = ladder_new(LADDERSIZE);

  completed_games = 0;
  num_logins = num_logouts = new_players = 0;

  num_9 = 0;
  fp = xyfopen(FILENAME_LADDER9, "r");
  if (fp) {
    num_9 = ladder_load(fp, Ladder9);
    Logit("%d players loaded from file %s", num_9, filename() );
    fclose(fp);
  }

  num_19 = 0;
  fp = xyfopen(FILENAME_LADDER19, "r");
  if (fp) {
    num_19 = ladder_load(fp, Ladder19);
    Logit("%d players loaded from file %s", num_19, filename() );
    fclose(fp);
  }

  // mink_init();
  if (conffile.admin_name) create_admin_account(conffile.admin_name );
  Logit("Server up and running.");
  main_event_loop();
  Logit("Closing down.");
  net_closeAll();
  main_exit(0);
  return 0;
}

static int all_the_internets(void)
{
int port;
int pos, len, rc, cnt;

  for(pos=cnt=0;conffile.server_ports[pos];pos += len	) {
    rc = sscanf(conffile.server_ports+pos, "%d%n", &port, &len );
    if (rc < 0) break;
    if (rc < 1) { len=1; continue; }
    if (net_init(port, conffile.want_udp_port > 0 ? 1 : 0)) {
      Logit("Init failed on port %d.", port);
      continue;
      }
    Logit("Initialized on port %d.", port);
    cnt++;
  }
return cnt;
}

void main_exit(int code)
{

#if USING_DMALLOC
  dmalloc_log_unfreed();
  dmalloc_shutdown();
#endif
  exit(code);
}


static void read_ban_ip_list(void)
{
  FILE *fp;
  int rc,cnt=0;
  char buff[100], bots[40], tops[40];
  unsigned bot, top;

  fp = xyfopen(FILENAME_LIST_BAN, "r");
  if (!fp) return;
  while(fgets(buff, sizeof buff, fp)) {
    rc = sscanf(buff, "%x %x", &bot, &top);
    if (rc < 2) {
      rc = sscanf(buff, "%s %s", bots, tops);
      if (rc < 2) continue;
#if 0
#include <netinet/in.h>
#include <arpa/inet.h>
      bot = inet_addr(bots);
      if ((int)bot == -1) continue;
      top = inet_addr(tops);
      if ((int)top == -1) continue;
      bot = ntohl(bot); /* could use asc2ipaddr(bots, &bot); */
      top = ntohl(top);
#else
      if (asc2ipaddr(bots, &bot)) continue;
      if (asc2ipaddr(tops, &top)) continue;
#endif
      }
    cnt += range_add(bot,top);
  }
  fclose(fp);
  Logit("Ipban: Read %d ranges from %s", cnt, filename() );
  bot = top = 0xf7000001;
  cnt = range_remove(bot,top);
  Logit("Ipban: Avoided %d ranges %x-%x", cnt, bot, top );
}


static int daemonise(void)
{
  int rc;

  struct passwd *pp = NULL;
  struct group *gp = NULL;
  uid_t uid = 0, euid =0;
  gid_t gid = 0;

	/* we need to do these before chroot()ing, because they need
	** the /etc/ directory.
	*/
  if (conffile.chroot_user) pp = getpwnam(conffile.chroot_user);
  if (conffile.chroot_group) gp = getgrnam(conffile.chroot_group);

  if (conffile.chroot_user) {
    if (!pp) fprintf(stderr, "Could not find user %s\n", conffile.chroot_user );
    else { uid = pp->pw_uid; gid = pp->pw_gid; }
  }
  if (conffile.chroot_group) {
    if(!gp) fprintf(stderr, "Could not find group %s\n", conffile.chroot_group);
    else gid = gp->gr_gid;
  }

  if (conffile.chroot_dir) {
    (void) gethostbyname("localhost") ;
    rc = chdir(conffile.chroot_dir);
    if (rc==-1) { rc=errno;
      fprintf(stderr, "Failed chdir(%s): %d (%s)\n"
      , conffile.chroot_dir, rc, strerror(rc) );
    }
    rc = chroot(conffile.chroot_dir);
    if (rc==-1) { rc=errno;
      fprintf(stderr, "Failed chroot(%s): %d (%s)\n"
      , conffile.chroot_dir, rc, strerror(rc) );
    }
    if (rc) return rc;
  }

  if (gid) rc = setgid(gid);
  if (rc==-1) { rc=errno;
    fprintf(stderr, "Failed setgid(%d:%s): %d (%s)\n"
    , gid, conffile.chroot_group, rc, strerror(rc) );
    return rc;
  }
  if (uid) rc = setuid(uid);
  if (rc==-1) { rc=errno;
    fprintf(stderr, "Failed setuid(%d:%s): %d (%s)\n"
    , uid, conffile.chroot_user, rc, strerror(rc) );
    return rc;
  }
  if (conffile.want_fork > 0) {
    if ((rc=fork())) { fprintf(stderr, "Fork1 = %d\n", rc); _exit(0); }
    if ((rc=fork())) { fprintf(stderr, "Fork2 = %d\n", rc);  _exit(0); }
  }

  if (uid && (euid = geteuid()) != uid) {
    fprintf(stderr, "Failed setuid(%d:%s): euid=%d\n"
    , uid, conffile.chroot_user, euid );
    return -1;
  }

  if (conffile.chroot_dir) {
    if (!uid || !(euid = geteuid()) ) {
      fprintf(stderr, "Refuse to run as root, uid=%d, euid=%d\n", uid, euid );
      conf_file_write("refused.cnf");
      return rc;
    }
    conf_file_fixup();
  }
  return 0;
}

