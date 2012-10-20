/*
 * udp_commands.c
*/

/*
    NNGS - The No Name Go Server
    Copyright (C) 2005 Adriaan W.D. van Kessel

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
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "nngsmain.h"
#include "conffile.h"
#include "mink.h"
#include "utils.h"
#include "playerdb.h"
#include "gamedb.h"
#include "command.h"
#include "network.h"
#include "common.h"

#include "udp_commands.h"

/* static */ int udp_uptime(char *dst, size_t dstlen);
/* static */ int udp_shutdown(char *dst, size_t dstlen, char *opt);
/* static */ int udp_slots(char *dst, size_t dstlen);
/* static */ int udp_net(char *dst, size_t dstlen);
/* static */ int udp_players(char *dst, size_t dstlen);
/* static */ int udp_games(char *dst, size_t dstlen);
/* static */ int udp_board(char *dst, size_t dstlen, int num);


/***********************************************************************/
int udp_command(char *dst, size_t dstlen, char * src )
{
int len=0, opt=0;

if (!strcmp(src,"uptime")) len = udp_uptime(dst, dstlen);
else if (!strcmp(src,"players")) len = udp_players(dst, dstlen);
else if (!strcmp(src,"slots")) len = udp_slots(dst, dstlen);
else if (!strcmp(src,"net")) len = udp_net(dst, dstlen);
else if (!strcmp(src,"games")) len = udp_games(dst, dstlen);
else if (!strncmp(src,"shutdown ", 9)) len = udp_shutdown(dst, dstlen, src+9);
else if (1 == sscanf(src,"board %d", &opt)) len = udp_board(dst, dstlen, opt);
else len = my_snprintf(dst, dstlen, "Kuttje!:%s\n", src);

return len;
}

int udp_net(char *dst, size_t dstlen)
{
int ii, len;
size_t pos=0;
char *cp;

  len = my_snprintf(dst, dstlen, "Net\n");
  if (len < 0) return pos; else pos = len;
  for (ii = 0; ii < 200; ii++) {
    if (pos >= dstlen-80) break; /* assume well-behaved */
    cp = net_dumpslot(ii);
    if (!cp) break;
/* fprintf(stderr, "[%u]%s\n", pos, cp ); */
    len = my_snprintf(dst+pos, (size_t)(dstlen-pos), "%s\n", cp); /* assume well-behaved */
    if (len < 0) break; else pos += len;
  }

return pos;
}

int udp_games(char *dst, size_t dstlen)
{
int g0, len;
int pb, pw, mnum;
size_t pos=0;

  len = my_snprintf(dst, dstlen
    , "Games (%u) inuse isplaying gstatus pb pw + bname brank wname wrank + movenum\n"
    , (unsigned) garray_top);
  if (len < 0) return len; else pos = len;

  for (g0 = 0; g0 < garray_top; g0++) {
    if (pos+80 >= dstlen) break;
    pb = garray[g0].black.pnum;
    pw = garray[g0].white.pnum;
    len = my_snprintf(dst+pos, dstlen-pos, "%d:%u:%u:%u:%d:%d\n"
      , g0
      , (unsigned)garray[g0].slotstat.in_use
      , (unsigned)garray[g0].slotstat.is_playing
      , (int)garray[g0].gstatus
      , pb, pw);
    if (len < 0) break; else pos += len;
    if (!garray[g0].slotstat.in_use) continue;
    if (!garray[g0].slotstat.is_playing) continue;
    if (pb < 0 || pw < 0) continue;
    if (pb >= parray_top || pw >= parray_top) continue;
    pos--; /* eat \n */
    len = my_snprintf(dst+pos, dstlen-pos, ":%s:%s:%s:%s\n"
      , parray[pb].pname, parray[pb].srank
      , parray[pw].pname, parray[pw].srank
      );
    if (len < 0) return pos; else pos += len;
    if (!garray[g0].minkg) continue;
    pos--;
    mnum = mink_movenum(garray[g0].minkg);
    len = my_snprintf(dst+pos, dstlen-pos , ":%d\n" , (int) mnum);
    if (len < 0) return pos; else pos += len;
#if 1
    pos--; /* eat \n */
    len = my_snprintf(dst+pos, dstlen-pos
    , ":%d:%d:%d:%f:%d:%c:%c\n"
    , (int) mnum
    , (int) garray[g0].minkg->width
    , (int) garray[g0].minkg->handicap
    , (float) garray[g0].komi
    , (int) TICS2SECS(garray[g0].ts.byoticks) / 60
    , (garray[g0].rated) ? ' ' : garray[g0].teach ? 'T' : 'F'
    , (garray[g0].rules == RULES_NET) 
      ? (parray[pw].match_type == GAMETYPE_TNETGO) ? '*' : 'I'
      : 'G'
    );
    if (len < 0) break; else pos += len;
#endif
  }

return pos;
}

int udp_board(char *dst, size_t dstlen, int gnum)
{
int len;
size_t pos=0;

len = my_snprintf(dst, dstlen, "Board %d\n", gnum);

if (len < 0) return len;
pos = len;
if (pos >= dstlen) {
	len = my_snprintf(dst, dstlen, "%u/%u\n", (unsigned) pos, (unsigned) dstlen);
	return len; }

if (gnum < 0 || gnum >= garray_top )  {
	len = my_snprintf(dst, dstlen, "[invalid: %d >= %u]\n", gnum, garray_top);
	return len;
	}

if (!garray[gnum].slotstat.in_use)  {
	len = my_snprintf(dst, dstlen, "[Invalid %d] Inuse=%u Isplaying=%u gstat=%u p=%p\n"
	, gnum
	, (unsigned) garray[gnum].slotstat.in_use
	, (unsigned) garray[gnum].slotstat.is_playing
	, (unsigned) garray[gnum].gstatus
	, (void*) garray[gnum].minkg);
	return len;
	}

len = mink_raw_printboard(dst+pos, dstlen-pos, garray[gnum].minkg);
if (len > 0) pos += len;

return pos;
}

int udp_slots(char *dst, size_t dstlen)
{
int len, idx;
char *cp;
size_t pos=0;

len = my_snprintf(dst, dstlen, "Slots\n");
if (len < 0) return pos; pos = len;
for (idx=0; idx < parray_top; idx++) {
	cp = player_dumpslot(idx) ;
	len = strlen(cp);
	if (len + pos +2 >= dstlen) break;
	memcpy(dst+pos, cp, len);
	pos += len;
	len = my_snprintf(dst+pos, dstlen-pos, ":%d\n" , parray[idx].session.gnum);
	if (len < 0) break;
	pos += len;
	}
for (cp = dst; *cp; cp++) switch (*cp) {
	case ' ': case '/': case '[': case ']': *cp = ':';
	default: break;
	}
return pos;
}

int udp_players(char *dst, size_t dstlen)
{
int len,idx;
size_t pos=0;

len = my_snprintf(dst, dstlen, "Players\n");
if (len < 0) return pos; pos = len;
for (idx=0; idx < parray_top; idx++) {
	if (pos +40 > dstlen) break;
	if (!parray[idx].slotstat.is_inuse) continue;
	if (!parray[idx].slotstat.is_online) continue;
	len = my_snprintf(dst+pos, dstlen-pos, "%d:%s", idx, parray[idx].pname);
	if (len < 0) return pos; pos += len;
	len = my_snprintf(dst+pos, dstlen-pos, ":%s:%s:%s:%d"
	, parray[idx].srank
	, parray[idx].flags.is_looking? "!" : "-"
	, parray[idx].flags.is_open? "O" : "-"
	, parray[idx].session.gnum);
	if (len < 0) return pos; pos += len;
	memcpy(dst+pos, "\n" , 2); pos += 1;
	}
return pos;
}

int udp_shutdown(char *dst, size_t dstlen, char *opt)
{
int len, slot;
size_t pos=0;
char *name="*shutdown*";

  slot = player_find_login(name);
  if (slot < 0) {
    slot = player_new(); if (slot <0) slot = 0; /*!!*/
    parray[slot].session.socket = open("/dev/null", 0, 0);
    do_copy(parray[slot].login, name, sizeof parray[slot].login);
    do_copy(parray[slot].pname, name, sizeof parray[slot].pname);
  }
  len = my_snprintf(dst, dstlen, "shutdown %s\n", opt);
  if (len < 0) return pos; pos = len;
  pcommand(slot, dst);

return pos;
}

int udp_uptime(char *dst, size_t dstlen)
{
int len;
time_t uptime, now;
unsigned int count;
size_t pos=0;

  len = my_snprintf(dst, dstlen, "Uptime\n");
  if (len < 0) return pos; pos = len;
  now = globclock.time;
  uptime = now - startuptime;

  len = my_snprintf(dst+pos, dstlen-pos, "Servername: %s\n", conffile.server_name);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Serveraddress: %s\n", conffile.server_address);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Serverports: %s\n", conffile.server_ports);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Version: %s\n", conffile.version_string);
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Compile_date(local): %s %s\n"
                , __DATE__, __TIME__);
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Date(UTC): %s\n", time2str_utc(&now));
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Date(local): %s\n", time2str_local(&now));
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Startuptime(local): %s\n", time2str_local(&startuptime));
  if (len < 0) return pos; pos += len;

  if (uptime > (24*60*60) )
    len = my_snprintf(dst+pos, dstlen-pos, "Uptime: %d days,%s\n", 
	    uptime/(24*60*60) , secs2hms_long(uptime%(24*60*60) ));
  else
    len = my_snprintf(dst+pos, dstlen-pos, "Uptime: %s\n", secs2hms_long(uptime));
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Fd_count: %d\n", net_fd_count);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Completed_games: %d\n", completed_games);
  if (len < 0) return pos; pos += len;
  count = player_count();
  len = my_snprintf(dst+pos, dstlen-pos, "Player_count: %u\n", count);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Player_high: %u\n", player_high);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Logins: %u\n", num_logins);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Logouts: %u\n", num_logouts);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "New_players: %u\n", new_players);
  if (len < 0) return pos; pos += len;
  count = game_count();
  len = my_snprintf(dst+pos, dstlen-pos, "Game_count: %u\n", count);
  if (len < 0) return pos; pos += len;
  len = my_snprintf(dst+pos, dstlen-pos, "Game_high: %u\n", game_high);
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Bytes_received: %u\n", bytes_received);
  if (len < 0) return pos; pos += len;

  len = my_snprintf(dst+pos, dstlen-pos, "Bytes_sent: %u\n", bytes_sent);
  if (len < 0) return pos; pos += len;

return pos;
}

