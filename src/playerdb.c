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

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "playerdb.h"
#include "ladder.h"
#include "common.h"
#include "command.h"
#include "comproc.h"
#include "servercodes.h"
#include "utils.h"
#include "multicol.h"
#include "network.h"
#include "channel.h"
#include "gamedb.h"

#include "censor.h"
#include "plan.h"
#include "alias.h"

#if WANT_NNGSRATED
#include "rdbm.h"
#endif

struct player_ parray[PARRAY_SIZE]; 
int parray_top = 0;
static int parray_index_fd[CONNECTION_COUNT];

int sort_alpha[COUNTOF(parray)];
int sort_ladder9[COUNTOF(parray)];
int sort_ladder19[COUNTOF(parray)];
static void player_swap(int p0, int p1, int *sort_array);

static int player_get_empty_slot(void);
static void player_free(int p);
static void player_zero(int p);
static void player_write(int p);
static void player_swapslots(int p0, int p1, int keepfds);

static int got_player_attr_value(int p, char *attr, char *value, FILE * fp, char *fname);

char * player_dumpslot(int p)
{
  size_t pos;
  int diff;
  char buff[100];

  if (p < 0) {
    pos = sprintf(buff,"DumpSlot: negative slotnr :%d", p);
    return statstr_dup(buff, pos);
  }

  diff = (int) (parray[p].slotstat.timestamp - globclock.time);

  pos = sprintf(buff,"[%d/%d]", p, parray[p].session.socket);
  buff[pos++] = (parray[p].slotstat.is_inuse) ? 'u' : '-' ;
  buff[pos++] = (parray[p].slotstat.is_valid) ? 'v' : '-' ;
  buff[pos++] = (parray[p].slotstat.is_dirty) ? 'd' : '-' ;
  buff[pos++] = (parray[p].slotstat.is_connected) ? 'c' : '-' ;
  buff[pos++] = (parray[p].slotstat.is_online) ? 'o' : '-' ;
  buff[pos++] = (parray[p].slotstat.is_registered) ? 'r' : '-' ;
  pos += sprintf(buff+pos,"%u", (unsigned) parray[p].slotstat.fixcount);
  pos += sprintf(buff+pos,"[%+d]", diff);

  pos += sprintf(buff+pos,"%s", parray[p].login);
  pos += sprintf(buff+pos," %s", parray[p].pname);
  return statstr_dup(buff, pos);
}

void player_fix(int p)
{
  if (p < 0) return;
  parray[p].slotstat.is_inuse = 1;
  parray[p].slotstat.timestamp = globclock.time;
  parray[p].slotstat.fixcount += 1;
  if (!parray[p].slotstat.fixcount) {
    parray[p].slotstat.fixcount -= 1;
    Logit("Slot %s: Fixcount overflow", player_dumpslot(p));
  }
}

void player_unfix(int p)
{
  if (p < 0) return;
  if (parray[p].slotstat.fixcount) parray[p].slotstat.fixcount -= 1;
  else {
    Logit("Unfix, Slot %s: Fixcount zero", player_dumpslot(p));
  }
  if (!parray[p].slotstat.fixcount && !parray[p].slotstat.is_registered)
    player_clear(p);
}

void player_dirty(int p)
{
  if (p < 0) return;

  parray[p].slotstat.timestamp = globclock.time;
  if (parray[p].slotstat.is_dirty) return;
  parray[p].slotstat.is_dirty = 1;
  if (!parray[p].slotstat.is_valid) {
    if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
      Logit("Slot %s: Dirty=Invalid", player_dumpslot(p));
    }
  }
}

static int player_get_empty_slot(void)
{
  int best_u = -1; /* Unused slot available */
  int best_v = -1; /* Valid slot available */
  int best_i = -1; /* invalid slot available */
  int best_d = -1; /* Dirty slot available */
  int idx= 0;

  if (!parray_top) parray[idx].slotstat.timestamp = startuptime;
#define LRU_COMPARE(old,new) (old <0 \
	|| parray[old].slotstat.timestamp > parray[new].slotstat.timestamp)

  for (idx = 0; idx < parray_top; idx++) {
    if (parray[idx].slotstat.fixcount) continue;
    if (parray[idx].slotstat.is_connected) continue;
    if (!parray[idx].slotstat.is_inuse) {
      if (best_u < 0) best_u=idx; }
    else if (parray[idx].slotstat.is_dirty) {
      if (LRU_COMPARE(best_d,idx)) best_d=idx; }
    else if (parray[idx].slotstat.is_valid) {
      if (LRU_COMPARE(best_v,idx)) best_v=idx; }
    else {
      if (LRU_COMPARE(best_i,idx)) best_i=idx; }
  }
#undef LRU_COMPARE

  idx=best_u;			/* this one is empty */
  if (idx < 0) idx = best_i;	/* this one is invalid */
  if (idx < 0) idx = best_v;	/* this one is valid */
  if (idx < 0) idx = best_d;	/* this one is dirty */
  if (idx < 0) idx = parray_top;	/* allocate from top */
  if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
    Logit("Empty_slot: {u=%d i=%d v=%d d=%d t=%d} -->> %d"
    , best_u,best_i,best_v,best_d,parray_top,idx);
  }

  if (idx == best_d) {
    if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
      Logit("Slot %s: Flushed", player_dumpslot(idx));
    }
    player_save(best_d);
  }

  if (idx < parray_top && parray[idx].slotstat.is_valid && parray_top < COUNTOF(parray)) {
    if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
      Logit("Slot %s: moved to %d", player_dumpslot(idx), parray_top);
    }
    player_swapslots(idx, parray_top,0);
    parray_top++;
    if (parray_top<COUNTOF(parray)) parray[idx].slotstat.timestamp = globclock.time;
  }
  if (idx == parray_top && parray_top < COUNTOF(parray)) {
    parray_top++;
    if (parray_top<COUNTOF(parray)) parray[idx].slotstat.timestamp = globclock.time;
    if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
      Logit("Slot %s: top <- %d", player_dumpslot(idx), parray_top);
    }
  }
  player_clear(idx);
  parray[idx].slotstat.is_inuse = 1;
  parray[idx].slotstat.timestamp = globclock.time;
  return idx;
}

	/* Swap contents of two player-slots.
	** behaviour is governed by keepfds:
	** 0: 'normal' swap: fds are swapped as well
	** 1: keepfds: data is swapped, but fds (and connection) dont move
	** -1: fds-only: {fds&connection&sessionstate}
        **  are swapped, but playerdata is preserved.
	*/
static void player_swapslots(int p0, int p1, int keepfds)
{
int fd0,fd1;
char temp[sizeof parray[0]];
struct slotstat tempslots[2];

  fd0 = parray[p0].session.socket;
  fd1 = parray[p1].session.socket;
  tempslots[0] = parray[p0].slotstat;
  tempslots[1] = parray[p1].slotstat;
  if (keepfds > 0) {
    if (fd0 >= 0) parray_index_fd[fd0] = p1;
    if (fd1 >= 0) parray_index_fd[fd1] = p0;
  }

	/* Swap everything */
  if (keepfds >= 0) {
    memcpy(temp,&parray[p0], sizeof parray[p0]);
    memcpy(&parray[p0], &parray[p1], sizeof parray[p0]);
    memcpy(&parray[p1], temp , sizeof parray[p0]);
    player_swap(p0, p1, sort_alpha);
    player_swap(p0, p1, sort_ladder9);
    player_swap(p0, p1, sort_ladder19);
  }

	/* unswap the sockets,refcount, connectstatus */
  if (keepfds>0) {
    parray[p0].session.socket = fd0 ;
    parray[p1].session.socket = fd1 ;
    parray[p1].slotstat = tempslots[0];
    parray[p0].slotstat = tempslots[1];
    parray[p1].slotstat.is_connected = tempslots[1].is_connected;
    parray[p1].slotstat.is_online = tempslots[1].is_online;
    parray[p1].slotstat.fixcount = tempslots[1].fixcount;
    parray[p0].slotstat.is_connected = tempslots[0].is_connected;
    parray[p0].slotstat.is_online = tempslots[0].is_online;
    parray[p0].slotstat.fixcount = tempslots[0].fixcount;
  }
  if (keepfds<0) {
    parray[p0].session.socket = fd1 ;
    parray[p1].session.socket = fd0 ;
    parray[p0].slotstat.is_connected = tempslots[1].is_connected;
    parray[p0].slotstat.is_online = tempslots[1].is_online;
    parray[p0].slotstat.fixcount = tempslots[1].fixcount;
    parray[p1].slotstat.is_connected = tempslots[0].is_connected;
    parray[p1].slotstat.is_online = tempslots[0].is_online;
    parray[p1].slotstat.fixcount = tempslots[0].fixcount;
  }
}

void player_array_init(void)
{
  int i;

  for (i = 0; i < COUNTOF(parray); i++) {
    player_zero(i);
  }

  for (i = 0; i < COUNTOF(parray_index_fd); i++) {
    parray_index_fd[i] = -1;
  }
  pending_init();
}

void player_init(void)
{
  int i;

  for (i = 0; i < COUNTOF(parray); i++) {
    sort_alpha[i] = sort_ladder9[i] = sort_ladder19[i] = i;
  }
}


int player_cmp(int p1, int p2, int sorttype)
{
  const struct ladderplayer *lp1, *lp2;
  int a, b;

  if (!parray[p1].slotstat.is_online) {
    if (!parray[p2].slotstat.is_online) return 0;
    else return -1;
  }
  if (!parray[p2].slotstat.is_online) return 1;

  switch (sorttype) {
  case SORT_LADDER9:
    lp1 = ladder_player_named(Ladder9, parray[p1].pname);
    lp2 = ladder_player_named(Ladder9, parray[p2].pname);
    if (!lp1) a = num_9 + 1; else a = lp1->idx;
    if (!lp2) b = num_9 + 1; else b = lp2->idx;
    if (a > b) return 1;
    else return -1;
    break;
  case SORT_LADDER19:
    lp1 = ladder_player_named(Ladder19, parray[p1].pname);
    lp2 = ladder_player_named(Ladder19, parray[p2].pname);
    if (!lp1) a = num_19 + 1; else a = lp1->idx;
    if (!lp2) b = num_19 + 1; else b = lp2->idx;
    if (a > b) return 1;
    else return -1;
    break;
  case SORT_ALPHA:
    if (parray[p1].rating == parray[p2].rating)
      return strcmp(parray[p1].login, parray[p2].login);
    return(parray[p1].rating > parray[p2].rating);
    break;
  default:
    break;
  }
  return 0;
}

static void player_swap(int p1, int p2, int *sort_array)
{
  int tmp;

  tmp = sort_array[p1];
  sort_array[p1] = sort_array[p2];
  sort_array[p2] = tmp;
}

/* Yuck bubble sort! */
void player_resort(void)
{
  int i, j;
  int anychange = 1;
  int count = 0;

  for (i = 0; i < parray_top; i++) {
    sort_alpha[i] = sort_ladder9[i] = sort_ladder19[i] = i;
    count++;
  }
  for (i = 0; (i < count - 1) && anychange; i++) {
    anychange = 0;
    for (j = 0; j < count - 1; j++) {
      if (player_cmp(sort_alpha[j], sort_alpha[j + 1], SORT_ALPHA) > 0) {
	player_swap(j, j + 1, sort_alpha);
	anychange = 1;
      }
      if (player_cmp(sort_ladder9[j], sort_ladder9[j + 1], SORT_LADDER9) > 0) {
	player_swap(j, j + 1, sort_ladder9);
	anychange = 1;
      }
      if (player_cmp(sort_ladder19[j], sort_ladder19[j+1], SORT_LADDER19) > 0) {
	player_swap(j, j + 1, sort_ladder19);
	anychange = 1;
      }
    }
  }
}


int player_new(void)
{
  int slot ;

  slot = player_get_empty_slot();
  if (slot < 0) return slot;
  parray[slot].session.pstatus = PSTATUS_NEW;
  parray[slot].slotstat.fixcount = 1;
  return slot;
}


static void player_zero(int p)
{

  memset(&parray[p], 0, sizeof parray[p]);
  parray[p].session.pstatus = PSTATUS_EMPTY;
  parray[p].pname[0] = '\0';
  parray[p].login[0] = '\0';
  parray[p].fullname[0] = '\0';
  parray[p].email[0] = '\0';
  do_copy(parray[p].prompt,"#> ", sizeof parray[p].prompt);
  parray[p].passwd[0] = '\0';
  parray[p].RegDate[0] = '\0';
  parray[p].session.socket = -1;
  parray[p].slotstat.is_registered = 0;
  parray[p].water = 3;
  parray[p].busy[0]='\0';
  parray[p].d_height = 24;
  parray[p].d_width = 79;
  parray[p].forget.last_file[0] = '\0';
  parray[p].forget.last_file_line = 0;
  parray[p].flags.is_open = 1;
  parray[p].numgam = 0;
  parray[p].which_client = 0;
  parray[p].flags.is_rated = 0;
  parray[p].flags.is_client = 0;
  parray[p].ropen = 1;
  parray[p].flags.want_bell = 0;
  parray[p].extprompt = 0;
  parray[p].forget.notifiedby = 0;
  parray[p].flags.want_logins = 1;
  parray[p].flags.want_gshouts = 1;
  parray[p].i_verbose = 1;
  parray[p].i_shout = 1;
  parray[p].i_gshout = 1;
  parray[p].i_lshout = 1;
  parray[p].flags.want_tells = 1;
  parray[p].i_robot = 0;
  parray[p].i_kibitz = 1;
  parray[p].flags.is_looking = 0;
  parray[p].Private = 0;
  parray[p].automail = 0;
  parray[p].session.gnum = -1;
  parray[p].forget.last_tell = -1;
  parray[p].forget.last_tell_from = -1;
  parray[p].forget.last_pzz = -1;
  parray[p].last_channel = -1;
  parray[p].session.logon_time = 0;
  parray[p].forget.last_command_time = globclock.time;
  parray[p].outgoing = 0;
  parray[p].incoming = 0;
  parray[p].adminLevel = ADMIN_GUEST;
  parray[p].num_logons = 0;
  parray[p].nochannels = 0;
  parray[p].session.num_observe = 0;
  parray[p].def_time = 90;
  parray[p].def_size = 19;
  parray[p].def_byo_time = 10;
  parray[p].def_byo_stones = 25;
  parray[p].bmuzzled = 0;
  parray[p].muzzled = 0;
  parray[p].gmuzzled = 0;
  parray[p].tmuzzled = 0;
  parray[p].kmuzzled = 0;
  parray[p].last_problem = 0;
  parray[p].thisHost = 0;
  parray[p].lastHost = 0;
  /* parray[p].cruft.lastColor = PLAYER_WHITE; */
  parray[p].rating = 0;
  parray[p].orating = 0;
  parray[p].silent_login = 0;
  parray[p].language = 0;
  parray[p].session.opponent = -1;
  parray[p].cruft.last_opponent = -1;
  do_copy(parray[p].ranked, "NR", sizeof parray[p].ranked);
  do_copy(parray[p].rank, " ", sizeof parray[p].rank);
  do_copy(parray[p].srank, "NR", sizeof parray[p].srank);
  parray[p].gowins = 0;
  parray[p].golose = 0;
  parray[p].match_type = 0;
  parray[p].gonum_white = 0;
  parray[p].gonum_black = 0;

  parray[p].alias_list = alias_init();
  assert(parray[p].alias_list != NULL);

  parray[p].plan_lines = plan_init();
  parray[p].censor_list = censor_init();

  return ;
} 


static void player_free(int p)
{
  int ch;

  if (parray[p].plan_lines) plan_free(parray[p].plan_lines);
  parray[p].plan_lines = NULL;

  if (parray[p].censor_list) censor_free(parray[p].censor_list);
  parray[p].censor_list = NULL;

  if (parray[p].alias_list) alias_free(parray[p].alias_list);
  parray[p].alias_list = NULL;

  for (ch = 0; ch < MAX_NCHANNEL; ch++) channel_remove(ch, p);

  return ;
}


void player_clear(int p)
{
  player_free(p);
  player_zero(p);
  parray[p].session.pstatus = PSTATUS_EMPTY;
  parray[p].slotstat.is_inuse = 0;
  parray[p].slotstat.is_valid = 0;
  parray[p].slotstat.is_dirty = 0;
  parray[p].slotstat.is_connected = 0;
  parray[p].slotstat.is_online = 0;
  parray[p].slotstat.fixcount = 0;
  parray[p].slotstat.is_registered = 0;
  return ;
}


void player_disconnect(int p)
{
  int p1;

  if (p < 0) return ;

  if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
    Logit( "Disconnect %s: top=%d", player_dumpslot(p), parray_top);
  }

  player_decline_offers(p, -1, -1);
  player_withdraw_offers(p, -1, -1);

  /* [PEM]: Skip these loops if the p is empty or new. */
  if (parray[p].slotstat.is_online) {    
    ReallyRemoveOldGamesForPlayer(p);
  }
  player_resort();
  player_save(p);

  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_connected) continue;
    if (parray[p1].forget.last_tell_from == p) parray[p1].forget.last_tell_from = -1;
    if (parray[p1].forget.last_tell == p) parray[p1].forget.last_tell = -1;
    if (parray[p1].forget.last_pzz == p) parray[p1].forget.last_pzz = -1;
    if (parray[p1].cruft.last_opponent == p) parray[p1].cruft.last_opponent = -1;
  }
  for (p1 = 0; p1 < MAX_NCHANNEL; p1++) channel_remove(p1, p);

  parray[p].slotstat.is_connected = 0;
  parray[p].slotstat.is_online = 0;
  parray[p].session.pstatus = PSTATUS_EMPTY;
  parray[p].session.socket = -1;
  return ;
}


static int got_player_attr_value(int p, char *attr, char *value, FILE * fp, char *fname)
{
  int i, len;
  char *cp;
  char tmp[MAX_LINE_SIZE], *tmp1;

  if (!strcmp(attr, "vars:")) {
    cp = strtok(value, ":"); if (!cp) return 0;
    parray[p].flags.is_open = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].water = atoi(cp);
    if (parray[p].water < 0) parray[p].water = 3;
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.is_client = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].ropen = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.want_bell = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.want_logins = atoi(cp);
    if (parray[p].flags.want_logins) channel_add(CHANNEL_LOGON, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.want_gshouts = atoi(cp);
    if (parray[p].flags.want_gshouts) channel_add(CHANNEL_GAME, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_shout = atoi(cp);
    if (parray[p].i_shout) channel_add(CHANNEL_SHOUT, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_gshout = atoi(cp);
    if (parray[p].i_gshout) channel_add(CHANNEL_GSHOUT, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_lshout = atoi(cp);
    if (parray[p].i_lshout) channel_add(CHANNEL_LSHOUT, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.want_tells = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_robot = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_kibitz = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].i_verbose = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].flags.is_looking = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].Private = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].automail = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].adminLevel = atoi(cp);
#if EXISTING_USERS_ARE_REGISTERED
	/* Silently convert users to registered users ... */
    if (parray[p].adminLevel <= ADMIN_USER)
      parray[p].adminLevel = ADMIN_REGISTERED_USER;
#endif
    if (parray[p].adminLevel >= ADMIN_ADMIN)
      channel_add(CHANNEL_ASHOUT, p);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].bmuzzled = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].muzzled = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].gmuzzled = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].tmuzzled = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].kmuzzled = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].last_problem = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].gonum_white = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].gonum_black = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].gowins = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].golose = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].def_time = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].def_size = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].def_byo_time = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].def_byo_stones = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].last_channel = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].extprompt = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].which_client = atoi(cp);
    cp = strtok(NULL, ":"); if (!cp) return 0;
    parray[p].num_logons = atoi(cp);
  } else if (!strcasecmp(attr, "name:")) {
    do_copy(parray[p].pname, value, sizeof parray[p].pname);
  } else if (!strcasecmp(attr, "password:")) {
    do_copy(parray[p].passwd, value, sizeof parray[p].passwd);
  } else if (!strcasecmp(attr, "fullname:")) {
    do_copy(parray[p].fullname, value, sizeof parray[p].fullname);
  } else if (!strcasecmp(attr, "email:")) {
    do_copy(parray[p].email, value, sizeof parray[p].email);
  } else if (!strcasecmp(attr, "regdate:")) {
    do_copy(parray[p].RegDate, value, sizeof parray[p].RegDate);
  } else if (!strcasecmp(attr, "rank:")) {
    do_copy(parray[p].rank, value, sizeof parray[p].rank);
  } else if (!strcasecmp(attr, "ranked:")) {
    do_copy(parray[p].ranked, value, sizeof parray[p].ranked);
  } else if (!strcasecmp(attr, "prompt:")) {
    do_copy(parray[p].prompt, value, sizeof parray[p].prompt);
  } else if (!strcasecmp(attr, "lasthost:")) {
    parray[p].lastHost = atoi(value);
  } else if (!strcasecmp(attr, "channel:")) {
    channel_add(atoi(value), p);
  } else if (!strcasecmp(attr, "num_plan:")) {
    for(i = atoi(value); i-- > 0; ) {
      fgets(tmp, sizeof tmp, fp);
      if (!(len = strlen(tmp))) {
	Logit("Error bad plan in file %s", fname);
        continue;
      }
      tmp[len-1] = '\0';	/* Zap '\n' */
      plan_add(tmp, parray[p].plan_lines);
    }
  } else if (!strcasecmp(attr, "num_alias:")) {
    for(i = atoi(value); i-- > 0; ) {
      fgets(tmp, sizeof tmp, fp);
      if (!(len = strlen(tmp))) {
	Logit( "Error bad alias in file %s\n", fname);
        continue;
      }
      tmp[len - 1] = '\0';	/* Get rid of '\n' */
      tmp1 = eatword(tmp);
      if (*tmp1) {*tmp1++ = '\0'; }
      tmp1 = eatwhite(tmp1);
      alias_add(tmp, tmp1, parray[p].alias_list);
    }
  } else if (!strcasecmp(attr, "num_censor:")) {
    for(i = atoi(value); i-- > 0; ) {
      fgets(tmp, sizeof tmp, fp);
      if (!(len = strlen(tmp))) {
	Logit("Error bad censor in file %s", fname);
	continue; }
      tmp[len-1] = '\0';	/* Zap '\n' */
      censor_add(tmp, parray[p].censor_list);
    }
  } else if (!strcasecmp(attr, "water:")) {
    parray[p].water = atoi(value);
    if (parray[p].water < 0) parray[p].water = 3;
  } else if (!strcasecmp(attr, "open:")) {
    parray[p].flags.is_open = atoi(value);
  } else if (!strcasecmp(attr, "client:")) {
    parray[p].flags.is_client = atoi(value);
  } else if (!strcasecmp(attr, "ropen:")) {
    parray[p].ropen = atoi(value);
  } else if (!strcasecmp(attr, "bell:")) {
    parray[p].flags.want_bell = atoi(value);
  } else if (!strcasecmp(attr, "i_login:")) {
    parray[p].flags.want_logins = atoi(value);
  } else if (!strcasecmp(attr, "i_game:")) {
    parray[p].flags.want_gshouts = atoi(value);
  } else if (!strcasecmp(attr, "i_shout:")) {
    parray[p].i_shout = atoi(value);
  } else if (!strcasecmp(attr, "i_lshout:")) {
    parray[p].i_lshout = atoi(value);
  } else if (!strcasecmp(attr, "i_gshout:")) {
    parray[p].i_gshout = atoi(value);
  } else if (!strcasecmp(attr, "i_tell:")) {
    parray[p].flags.want_tells = atoi(value);
  } else if (!strcasecmp(attr, "i_robot:")) {
    parray[p].i_robot = atoi(value);
  } else if (!strcasecmp(attr, "i_verbose:")) {
    parray[p].i_verbose = atoi(value);
  } else if (!strcasecmp(attr, "i_kibitz:")) {
    parray[p].i_kibitz = atoi(value);
  } else if (!strcasecmp(attr, "looking:")) {
    parray[p].flags.is_looking = atoi(value);
  } else if (!strcasecmp(attr, "private:")) {
    parray[p].Private = atoi(value);
  } else if (!strcasecmp(attr, "automail:")) {
    parray[p].automail = atoi(value);
  } else if (!strcasecmp(attr, "admin_level:")) {
    parray[p].adminLevel = atoi(value);
  } else if (!strcasecmp(attr, "goblack_games:")) {
    parray[p].gonum_black = atoi(value);
  } else if (!strcasecmp(attr, "gowhite_games:")) {
    parray[p].gonum_white = atoi(value);
  } else if (!strcasecmp(attr, "gowins:")) {
    parray[p].gowins = atoi(value);
  } else if (!strcasecmp(attr, "golose:")) {
    parray[p].golose = atoi(value);
  } else if (!strcasecmp(attr, "def_time:")) {
    parray[p].def_time = atoi(value);
  } else if (!strcasecmp(attr, "def_size:")) {
    parray[p].def_size = atoi(value);
  } else if (!strcasecmp(attr, "def_byo_time:")) {
    parray[p].def_byo_time = atoi(value);
  } else if (!strcasecmp(attr, "last_chan:")) {
    parray[p].last_channel = atoi(value);
  } else if (!strcasecmp(attr, "def_byo_stones:")) {
    parray[p].def_byo_stones = atoi(value);
  } else if (!strcasecmp(attr, "bmuzzled:")) {
    parray[p].bmuzzled = atoi(value);
  } else if (!strcasecmp(attr, "muzzled:")) {
    parray[p].muzzled = atoi(value);
  } else if (!strcasecmp(attr, "tmuzzled:")) {
    parray[p].tmuzzled = atoi(value);
  } else if (!strcasecmp(attr, "kmuzzled:")) {
    parray[p].kmuzzled = atoi(value);
  } else if (!strcasecmp(attr, "last_problem:")) {
    parray[p].last_problem = atoi(value);
  } else if (!strcasecmp(attr, "gmuzzled:")) {
    parray[p].gmuzzled = atoi(value);
  } else if (!strcasecmp(attr, "silent_login:")) {
    parray[p].silent_login = atoi(value);
  } else if (!strcasecmp(attr, "language:")) {
    parray[p].language = atoi(value);
  } else {
    Logit( "Error bad attribute <%s> value <%s> in file %s"
    , attr, value, fname);
  }
  return 0;
}


int player_read(int p)
{
  char line[MAX_LINE_SIZE];
  char *attr, *value;
  FILE *fp;
  int len, rat, gm, slot;
  time_t tt = globclock.time;
  char trnk;

  gm = 0;

  /* Check if there is still something in the cache.
  ** On the fly: write out all dirty slots.
  ** To avoid stale pointers and socket fd's
  ** we dont try to reuse the cached data.
  ** Maybe later.
  */
  for (slot = 0; slot < parray_top; slot++) {
    if (!parray[slot].slotstat.is_inuse) continue;
    if (slot == p) continue;
    if (parray[slot].slotstat.is_dirty) player_save(slot);
    if (strcmp(parray[slot].login, parray[p].login)) continue;
    if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
      Logit("Slot %s shadowed by %d", player_dumpslot(slot), p);
    }
    /* we don't want player_clear(slot); for valid slots.
    ** if the player is online, he will be kicked out later */
    if (parray[slot].slotstat.is_valid 
	&& parray[slot].slotstat.is_connected) continue;
    /* we could reuse the data here, by copying p<-slot, and avoid a read.
    ** Dirty data (if any) has already been written out to disk here.
    */
    if (parray[slot].slotstat.is_valid && slot > p) {
      player_swapslots(slot, p, 1);
      if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
        Logit("Slot %d copied down from %s", p, player_dumpslot(slot));
      }
    }
#if PLAYER_SWAPSLOTS_WOULD_BE_CORRECT
    if (parray[slot].slotstat.is_valid && slot < p) {
      player_swapslots(slot, p, -1);
      if (conffile.debug_parray & DEBUG_PLAYER_SLOT) {
        Logit("Slot %d copied up from %s", p, player_dumpslot(slot));
      }
      player_clear(p);
      p = slot; continue;
      }
    player_clear(slot);
#endif
    }

  if (parray[p].slotstat.is_valid) goto skip_fysical_read;
  /* open the player data file */
  fp = xyfopen(FILENAME_PLAYER_cs, "r", parray[p].login );
  if (!fp) {
    do_copy(parray[p].pname, parray[p].login, sizeof parray[p].pname);
    parray[p].slotstat.is_valid = 1;
    parray[p].slotstat.is_registered = 0;
    parray[p].flags.is_rated = 0;
    return -1; /* unregistered */
  }

  /* if the player data file exists, the player is registered */
  parray[p].slotstat.is_valid = 1;
  parray[p].slotstat.is_registered = 1;

  while (fgets(line, sizeof line, fp)) {
    if ((len = strlen(line)) <= 1) continue;
    line[len - 1] = '\0';
    attr = eatwhite(line);
    if (attr[0] == '#') continue;			/* Comment */
    value = eatword(attr);
    if (!*value) {
      Logit( "Error reading %s(1attr: %s) Line=", filename(), attr, line);
      continue;
    }
    *value = '\0';
    value++;
    value = eatwhite(value);
    if (!*value) {
      Logit( "Error reading %s(2attr: %s) Line=", filename(), attr, line);
      continue;
    }
    stolower(attr);
    /*    Logit("attr: %s    value: %s", attr, value); */
    /* process the line */
    got_player_attr_value(p, attr, value, fp, filename() );
  }

  /* done reading the player data file */
  fclose(fp);

skip_fysical_read:
  if (!parray[p].pname[0]) {
    Logit( "Error reading %s for %s, set invalid", filename(), parray[p].login);
    do_copy(parray[p].pname, parray[p].login, sizeof parray[p].pname);
    pcn_out(p, CODE_CR1|CODE_ERROR, FORMAT_WARNING_YOUR_DATA_FILE_IS_CORRUPT_PLEASE_TELL_AN_ADMIN_n);
    /* (dirty && !valid) This will cause "Dirty=Invalid" to be logged */
    player_dirty(p);
    parray[p].slotstat.is_valid = 0;
  }
  if (!parray[p].RegDate[0]) {
    player_dirty(p);
    do_copy(parray[p].RegDate, time2str_utc(&tt), sizeof parray[p].RegDate);
  }
#if WANT_NNGSRATED
  if (!strcasecmp(parray[p].ranked, "NR") ) {
    parray[p].rating = 0;
    parray[p].orating = 0;
    parray[p].flags.is_rated = 0;
  }
  else if (!strcmp(parray[p].ranked, "???")) {
    parray[p].rating = 1;
    parray[p].orating = 1;
  } else {
    rdbm_t rdb;
    char fname[MAX_FILENAME_SIZE];

    xyfilename(fname, FILENAME_NRATINGS );
    if ((rdb = rdbm_open(fname,0))) {
      rdbm_player_t rp;

      if (rdbm_fetch(rdb, parray[p].pname, &rp)) {
        player_dirty(p); /* This will force the playerfile to be written */
	do_copy(parray[p].srank, rp.rank, sizeof parray[p].srank);
	if (rp.star) parray[p].flags.is_rated = 1;
	parray[p].rating = parray[p].orating = (int)(rp.rating * 100);
	parray[p].numgam = rp.wins + rp.losses;
      }
      rdbm_close(rdb);
    } else {
      Logit("Cannot open nratings file");
    }
  }
  if (parray[p].rating == 0) {
    /* player_dirty(p); */
    do_copy(parray[p].srank, parray[p].ranked, sizeof parray[p].srank);
    if (strcmp(parray[p].srank, "NR")) {
      sscanf(parray[p].srank, "%u%c", &rat, &trnk);
      rat = parse_rank(rat, trnk);
      parray[p].orating = rat * 100;
    } else
      parray[p].orating = 0;
  }
#else
  parray[p].rating = 0;
  strcpy(parray[p].srank,parray[p].ranked);
  if (!strcmp(parray[p].srank, "NR")) parray[p].orating = 0;
  else {
    sscanf(parray[p].srank, "%u%c", &rat, &trnk);
    rat = parse_rank(rat, trnk);
    parray[p].orating = rat * 100;
  }
#endif /* WANT_NNGSRATED */
  return 0;
}

int player_delete(int p)
{

  if (!parray[p].slotstat.is_registered) {	/* Player must be registered */
    player_clear(p);
    return -1;
  }
  xyunlink(FILENAME_PLAYER_cs, parray[p].login);
  player_clear(p);
  return 0;
}


int player_markdeleted(int p)
{
  FILE *fp;

  if (parray[p].adminLevel >= ADMIN_ADMIN)
    if (!in_list("admin", parray[p].pname))
      return -1;  /* Not an admin, corruption, refuse to save. */

  if (!parray[p].slotstat.is_registered) {	/* Player must be registered */
    return -1;
  }

  xyrename(FILENAME_PLAYER_cs,FILENAME_PLAYER_cs_DELETE, parray[p].login);
  fp = xyfopen(FILENAME_PLAYER_cs_DELETE, "a");	/* Touch the file */
  if (fp) {
    player_clear(p);
    fprintf(fp, "\n");
    fclose(fp);
  }
  return 0;
}


/* NL undebugged, and the keywords won't all work!  Still need to look
 * at this in more detail. */
#if 0
static int player_save_extended(int p)
{
  FILE *fp;
  int i;

  parray[p].slotstat.is_dirty=0;
  /* non-registered player info is not saved! */
  if (!parray[p].slotstat.is_registered) {
    return -1;
    }

  fp = xyfopen(FILENAME_PLAYER_cs, "w",parray[p].login);
  if (!fp) return -1;

  fprintf(fp, "Name: %s\n", parray[p].pname);
  fprintf(fp, "Fullname: %s\n", parray[p].fullname);
  fprintf(fp, "Password: %s\n", parray[p].passwd);
  fprintf(fp, "Email: %s\n", parray[p].email);
  fprintf(fp, "rank: %s\n", parray[p].rank);
  fprintf(fp, "ranked: %s\n", parray[p].ranked);
  fprintf(fp, "Prompt: %s\n", parray[p].prompt);

  fprintf(fp, "RegDate: %s\n",  parray[p].RegDate);

  fprintf(fp, "LastHost: %d\n", parray[p].lastHost);

  fprintf(fp, "VARS:\n");
  fprintf(fp, "open:%d\n", parray[p].flags.is_open);
  fprintf(fp, "water:%d\n", parray[p].water);
  fprintf(fp, "client:%d\n", parray[p].flags.is_client);
  fprintf(fp, "ropen:%d\n", parray[p].ropen);
  fprintf(fp, "bell:%d\n", parray[p].flags.want_bell);
  fprintf(fp, "i_login:%d\n", parray[p].flags.want_logins);
  fprintf(fp, "i_game:%d\n", parray[p].flags.want_gshouts);
  fprintf(fp, "i_shout:%d\n", parray[p].i_shout);

  fprintf(fp, "i_gshout:%d\n", parray[p].i_gshout);
  fprintf(fp, "i_lshout:%d\n", parray[p].i_lshout);
  fprintf(fp, "i_tell:%d\n", parray[p].flags.want_tells);
  fprintf(fp, "i_robot:%d\n", parray[p].i_robot);
  fprintf(fp, "i_kibitz:%d\n", parray[p].i_kibitz);
  fprintf(fp, "i_verbose:%d\n", parray[p].i_verbose);
  fprintf(fp, "looking:%d\n", parray[p].flags.is_looking);
  fprintf(fp, "Private:%d\n", parray[p].Private);

  fprintf(fp, "automail:%d\n", parray[p].automail);
  fprintf(fp, "adminLevel:%d\n", parray[p].adminLevel);
  fprintf(fp, "bmuzzled:%d\n", parray[p].bmuzzled);
  fprintf(fp, "muzzled:%d\n", parray[p].muzzled);
  fprintf(fp, "gmuzzled:%d\n", parray[p].gmuzzled);
  fprintf(fp, "tmuzzled:%d\n", parray[p].tmuzzled);
  fprintf(fp, "kmuzzled:%d\n", parray[p].kmuzzled);
  fprintf(fp, "last_problem:%d\n", parray[p].last_problem);

  fprintf(fp, "gonum_white:%d\n", parray[p].gonum_white);
  fprintf(fp, "gonum_black:%d\n", parray[p].gonum_black);
  fprintf(fp, "gowins:%d\n", parray[p].gowins);
  fprintf(fp, "golose:%d\n", parray[p].golose);
  fprintf(fp, "def_time:%d\n", parray[p].def_time);
  fprintf(fp, "def_size:%d\n", parray[p].def_size);
  fprintf(fp, "def_byo_time:%d\n", parray[p].def_byo_time);
  fprintf(fp, "def_byo_stones:%d\n", parray[p].def_byo_stones);

  fprintf(fp, "last_channel:%d\n", parray[p].last_channel);
  fprintf(fp, "extprompt:%d\n", parray[p].extprompt);
  fprintf(fp, "which_client:%d\n", parray[p].which_client);
  fprintf(fp, "num_logons:%d\n", parray[p].num_logons);

  fprintf(fp, "\n");
  fprintf(fp, "silent_login: %d\n", parray[p].silent_login);
  fprintf(fp, "language: %d\n", parray[p].language);

  for (i = 0; i < MAX_NCHANNEL; i++) {
    if (on_channel(i, p))
      fprintf(fp, "Channel: %d\n", i);
  }

  {
    char *c, *a;

    fprintf(fp, "Num_plan: %d\n", plan_count(parray[p].plan_lines));
    plan_start(parray[p].plan_lines);
    while (plan_next(&c, parray[p].plan_lines))
      fprintf(fp, "%s\n", c);

    fprintf(fp, "Num_censor: %d\n", censor_count(parray[p].censor_list));
    censor_start(parray[p].censor_list);
    while (censor_next(&c, parray[p].censor_list))
      fprintf(fp, "%s\n", c);

    fprintf(fp, "Num_alias: %d\n", alias_count(parray[p].alias_list));
    alias_start(parray[p].alias_list);
    while (alias_next(&c, &a, parray[p].alias_list))
      fprintf(fp, "%s %s\n", c, a);
  }
  fclose(fp);
  return 0;
}
#endif


void player_save(int p)
{

  /* invalid or non-registered player info is not saved! */
  if (!parray[p].slotstat.is_registered || !parray[p].slotstat.is_valid) {
    parray[p].slotstat.is_dirty=0;
    return;
  }
  if (parray[p].slotstat.is_dirty) player_write(p);
}


static void player_write(int p)
{
  FILE *fp;
  int i;
  int old_size, new_size;
  old_size = xystat(NULL, FILENAME_PLAYER_cs, "w",parray[p].login);

  fp = xyfopen(FILENAME_PLAYER_cs, "w",parray[p].login);
  if (!fp) {
    Logit("Player_write(%s) failed", filename() );
    return; }

  if (parray[p].pname[0]) fprintf(fp, "Name: %s\n", parray[p].pname);
  if (parray[p].fullname[0]) fprintf(fp, "Fullname: %s\n", parray[p].fullname);
  if (parray[p].passwd[0]) fprintf(fp, "Password: %s\n", parray[p].passwd);
  if (parray[p].email[0]) fprintf(fp, "Email: %s\n", parray[p].email);
  if (parray[p].rank[0]) fprintf(fp, "Rank: %s\n", parray[p].rank);
  if (parray[p].ranked[0]) fprintf(fp, "Ranked: %s\n", parray[p].ranked);
  if (parray[p].prompt[0]) fprintf(fp, "Prompt: %s\n", parray[p].prompt);

  fprintf(fp, "RegDate: %s\n",  parray[p].RegDate);
  fprintf(fp, "LastHost: %d\n", parray[p].lastHost);

  fprintf(fp, "VARS: %d:%d:%d:%d:%d:%d:%d:%d",
  parray[p].flags.is_open,
  parray[p].water,
  parray[p].flags.is_client,
  parray[p].ropen,
  parray[p].flags.want_bell,
  parray[p].flags.want_logins,
  parray[p].flags.want_gshouts,
  parray[p].i_shout);
  fprintf(fp, " :%d:%d:%d:%d:%d:%d:%d:%d",
  parray[p].i_gshout,
  parray[p].i_lshout,
  parray[p].flags.want_tells,
  parray[p].i_robot,
  parray[p].i_kibitz,
  parray[p].i_verbose,
  parray[p].flags.is_looking,
  parray[p].Private);
  fprintf(fp, " :%d:%d:%d:%d:%d:%d:%d:%d",
  parray[p].automail,
  parray[p].adminLevel,
  parray[p].bmuzzled,
  parray[p].muzzled,
  parray[p].gmuzzled,
  parray[p].tmuzzled,
  parray[p].kmuzzled,
  parray[p].last_problem);
  fprintf(fp, " :%d:%d:%d:%d:%d:%d:%d:%d",
  parray[p].gonum_white,
  parray[p].gonum_black,
  parray[p].gowins,
  parray[p].golose,
  parray[p].def_time,
  parray[p].def_size,
  parray[p].def_byo_time,
  parray[p].def_byo_stones);
  fprintf(fp, " :%d:%d:%d:%d\n",
  parray[p].last_channel,
  parray[p].extprompt,
  parray[p].which_client,
  parray[p].num_logons);
  fprintf(fp, "silent_login: %d\n", parray[p].silent_login);
  fprintf(fp, "language: %d\n", parray[p].language);

  for (i = 0; i < MAX_NCHANNEL; i++) {
    if (on_channel(i, p))
      fprintf(fp, "Channel: %d\n", i);
  }

  {
    char *c, *a;

    fprintf(fp, "Num_plan: %d\n", plan_count(parray[p].plan_lines));
    plan_start(parray[p].plan_lines);
    while (plan_next(&c, parray[p].plan_lines))
      fprintf(fp, "%s\n", c);

    fprintf(fp, "Num_censor: %d\n", censor_count(parray[p].censor_list));
    censor_start(parray[p].censor_list);
    while (censor_next(&c, parray[p].censor_list))
      fprintf(fp, "%s\n", c);

    fprintf(fp, "Num_alias: %d\n", alias_count(parray[p].alias_list));
    alias_start(parray[p].alias_list);
    while (alias_next(&c, &a, parray[p].alias_list))
      fprintf(fp, "%s %s\n", c, a);
  }
  fclose(fp);

  new_size = xystat(NULL, FILENAME_PLAYER_cs, "w",parray[p].login);
  if (new_size >= old_size/2) parray[p].slotstat.is_dirty=0;
  return ;
}


int player_find_fd(int fd)
{
  int slot;

  if (fd >= 0 && fd < COUNTOF(parray_index_fd)) {
    slot= parray_index_fd[fd];
    if (slot >=0 && parray[slot].session.socket == fd) return slot;
  }
  for (slot = 0; slot < parray_top; slot++) {
    if (!parray[slot].slotstat.is_inuse) continue;
    if (!parray[slot].slotstat.is_connected) continue;
    if (parray[slot].session.socket != fd) continue;
    parray_index_fd[fd]=slot;
    return slot;
  }
  parray_index_fd[fd] = -1;
  return -1;
}


int player_find_login(const char *name)
{
  int slot;

  for (slot = 0; slot < parray_top; slot++) {
    if (!parray[slot].slotstat.is_inuse) continue;
    if (!parray[slot].slotstat.is_online) continue;
    if (!strcmp(parray[slot].login, name)) return slot;
  }
  return -1;
}


int player_find_part_login(const char *name)
{
  int i;
  int found = -1;
  size_t len,cnt;

  cnt = 0;
  len = strlen(name);
  for (i = 0; i < parray_top; i++) {
    if (!parray[i].slotstat.is_inuse) continue;
    if (!parray[i].slotstat.is_online) continue;
    if (!strcmp(parray[i].login, name)) return i; /* exact match */
    if (strncmp(parray[i].login, name, len)) continue;
    if (found < 0) found = i;
    cnt++;
  }
  if (cnt > 1) return -2;
  return found;
}


int check_censored(int p, const char *name) 
{

  return (censor_lookup(name, parray[p].censor_list) ? 1 : 0 );
}


int player_censored(int p, int p1)
{

  return (censor_lookup(parray[p1].login, parray[p].censor_list) ? 1 : 0);
}


unsigned int player_count()
{
  unsigned int count = 0;
  int i;

  for (i = 0; i < parray_top; i++) {
    if (!parray[i].slotstat.is_inuse) continue;
    if (!parray[i].slotstat.is_online) continue;
    count++;
  }
  if (count > player_high) player_high = count;
  return count;
}


unsigned int player_idle(int p)
{
  if (parray[p].slotstat.is_online)
    return globclock.time - parray[p].forget.last_command_time;
  else
    return globclock.time - parray[p].session.logon_time;
}


unsigned int player_ontime(int p)
{
  return globclock.time - parray[p].session.logon_time;
}


static void write_p_inout(int inout, int p, FILE *fp, int maxlines)
{
  if (!fp)
    return;
  fprintf(fp, "%d %s %d %d %s\n", inout, parray[p].pname, (int) globclock.time, 
                  parray[p].slotstat.is_registered, 
                  dotQuad(parray[p].thisHost));
  fclose(fp);
  if (parray[p].num_logons % 100 == 0) {
    if (conffile.debug_general) Logit("About to truncate");
    if (maxlines >= 1) truncate_file(filename(), maxlines);
    if (conffile.debug_general) Logit("done with  truncate");
  }
}

void player_write_loginout(int p, int inout)
{
  FILE *fp;

  fp=xyfopen(FILENAME_LOGONS, "a");
  write_p_inout(inout, p, fp, 100);
  if (parray[p].slotstat.is_registered) {
    fp=xyfopen(FILENAME_PLAYER_cs_LOGONS, "a", parray[p].login);
    write_p_inout(inout, p, fp, 24);
  }
  switch(inout) {
  case P_LOGIN:
    parray[p].num_logons++;
    Logit("Login : %s %d/%d/%s%s %s", 
                  parray[p].pname, p, parray[p].session.socket,
                  parray[p].slotstat.is_registered ? "R" : "U", 
                  parray[p].adminLevel >= ADMIN_ADMIN ? "***" : "", 
                  dotQuad(parray[p].thisHost));
    break;
  case P_LOGOUT:
    Logit("Logout : %s %d/%d/%s%s %s", 
                  parray[p].pname, p, parray[p].session.socket,
                  parray[p].slotstat.is_registered ? "R" : "U", 
                  parray[p].adminLevel >= ADMIN_ADMIN ? "***" : "", 
                  dotQuad(parray[p].thisHost));
    break;
    }
}

time_t player_lastconnect(int p)
{
  FILE *fp;
  int inout;
  time_t thetime,registered,last = 0;
  char loginName[MAX_LOGIN_NAME+1];
  char ipstr[20];
  char buff[MAX_LINE_SIZE];

  fp = xyfopen(FILENAME_PLAYER_cs_LOGONS, "r", parray[p].login);
  if (!fp)
    return 0;
  while (fgets(buff,sizeof buff, fp)) {
    if (sscanf(buff, "%d %s %d %d %s", &inout, loginName, (int*) &thetime, 
                       (int*) &registered, ipstr) != 5) {
      Logit( "Error in login info format. %s: %s", filename(), statstr_trim(buff, 0) );
      fclose(fp);
      return 0;
    }
    if (inout == P_LOGIN) last = thetime;
  }
  fclose(fp);
  return last;
}

int player_lastdisconnect(int p)
{
  FILE *fp;
  int inout;
  time_t thetime, registered, last = 0;
  char ipstr[20];
  char loginName[MAX_LOGIN_NAME+1];
  char buff[MAX_LINE_SIZE];

  fp = xyfopen(FILENAME_PLAYER_cs_LOGONS, "r", parray[p].login);
  if (!fp) return 0;
  while (fgets(buff,sizeof buff, fp)) {
    if (sscanf(buff, "%d %s %d %d %s", &inout, loginName, (int*) &thetime, (int*) &registered, ipstr) != 5) {
      Logit( "Error in login info format. %s: %s", filename(), statstr_trim(buff, 0) );
      fclose(fp);
      return 0;
    }
    if (inout == P_LOGOUT) last = thetime;
  }
  fclose(fp);
  return last;
}

void player_pend_print(int p, struct pending * pend)
{
  char outstr[512];
  char tmp[512];

  if (p == pend->whofrom) {
    sprintf(outstr, "You are offering ");
  } else {
    sprintf(outstr, "%s is offering ", parray[pend->whofrom].pname);
  }
  if (p == pend->whoto) {
    strcpy(tmp, "");
  } else {
    sprintf(tmp, "%s ", parray[pend->whoto].pname);
  }
  strcat(outstr, tmp);
  switch (pend->type) {
  case PEND_GOEMATCH:
    sprintf(tmp, "%d %d.", pend->param3, pend->param2);
    break;
  case PEND_TMATCH:
  case PEND_GMATCH:
  case PEND_MATCH:
    sprintf(tmp, "%d %d %d.", pend->param3, pend->param2, pend->param4);
    break;
  case PEND_PAUSE:
    sprintf(tmp, "to pause the clock.");
    break;
  case PEND_TEACH:
    sprintf(tmp, "to make this a teaching game.");
    break;
  case PEND_ADJOURN:
    sprintf(tmp, "an adjournment.");
    break;
  case PEND_PAIR:
    sprintf(tmp, "to pair.");
    break;
  case PEND_DONE:
    sprintf(tmp, "that the board is acceptable for scoring.");
    break;
  case PEND_KOMI:
    sprintf(tmp, "a to set komi %f.", pend->float1);
    break;
  }
  strcat(outstr, tmp);
  pprintf(p, "%s\n", outstr);
}

struct pending * player_pending_new(int p, int p1, int type)
{
  struct pending * ptr;

  if (parray[p].outgoing >= MAX_PENDING) return NULL;
  ptr = pending_new(p,p1,type);
  if (!ptr) return NULL;

  if (p >=0 && parray[p].slotstat.is_online) parray[p].outgoing++;
  if (p1 >=0 && parray[p1].slotstat.is_online) parray[p1].incoming++;
  return ptr;
}

void player_pending_delete(struct pending * ptr)
{
  int p1;

  if (!ptr) return ;
  if (!ptr->is_valid) return;
  p1=ptr->whofrom;
  if (p1 >=0 && parray[p1].slotstat.is_connected) parray[p1].outgoing--;
  p1=ptr->whoto;
  if (p1 >=0 && parray[p1].slotstat.is_connected) parray[p1].incoming--;
  pending_delete(ptr);
  return ;
}

int player_add_request(int p, int p1, int type, int param)
{
  struct pending * ptr;

  if ((ptr=pending_find(p, p1, type))) {
    ptr->param1 = param;
    return -1;			/* Already exists */
    }

  ptr = player_pending_new(p, p1, type);
  if (!ptr) {
    return -1;
  }
  ptr->param1 = param;
  return 0;
}

int player_remove_requests(int p, int p1, int type)
{
  struct pending *ptr;

  while ((ptr = pending_find(p, p1, type))) {
    player_pending_delete(ptr);
  }
  return 0;
}


int player_decline_offers(int p, int p1, int offerType)
{
  struct pending *ptr;
  int pf, pt;
  int count = 0;

  while ((ptr = pending_find(p1, p, offerType))) {
    pt=ptr->whoto;
    pf=ptr->whofrom;
    switch (ptr->type) {
    case PEND_GMATCH:
    case PEND_TMATCH:
    case PEND_GOEMATCH:
    case PEND_MATCH:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_DECLINES_YOUR_REQUEST_FOR_A_MATCH_n, 
                     parray[pt].pname);
      pcn_out(pt, CODE_INFO, FORMAT_YOU_DECLINE_THE_MATCH_OFFER_FROM_s_n, 
                     parray[pf].pname);
      break;
    case PEND_TEACH:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_DECLINES_YOUR_REQUEST_FOR_A_TEACHING_MATCH_n, parray[pt].pname);
      pcn_out(pt, CODE_INFO, FORMAT_YOU_DECLINE_THE_TEACHING_MATCH_OFFER_FROM_s_n , parray[pf].pname);
      break;
    case PEND_PAUSE:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_DECLINES_PAUSE_REQUEST_n, parray[pt].pname);
      pcn_out(pt, CODE_INFO, FORMAT_YOU_DECLINE_THE_PAUSE_REQUEST_FROM_s_n, parray[pf].pname);
      break;
    case PEND_ADJOURN:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_DECLINES_THE_ADJOURN_REQUEST_n, parray[pt].pname);
      pcn_out(pt, CODE_INFO, FORMAT_YOU_DECLINE_THE_ADJOURN_REQUEST_FROM_s_n, parray[pf].pname);
      break;
    case PEND_PAIR:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_DECLINES_YOUR_PAIR_REQUEST_n, parray[pt].pname);
      pcn_out(pt, CODE_INFO, FORMAT_YOU_DECLINE_THE_PAIR_REQUEST_FROM_s_n, parray[pf].pname);
      break;
    case PEND_DONE:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_s_WAS_NOT_FINISHED_REMOVING_STONES_PLEASE_TYPE_qDONEq_AGAIN_n, parray[pt].pname);
      break;
    }
    player_pending_delete(ptr);
    count++;
  }
  return count;
}


int player_withdraw_offers(int p, int p1, int offerType)
{
  struct pending *ptr;
  int pf, pt;
  int count = 0;

  while ((ptr = pending_find(p, p1, offerType))) {
    pt = ptr->whoto;
    pf = ptr->whofrom;
    switch (ptr->type) {
    case PEND_GMATCH:
    case PEND_TMATCH:
    case PEND_GOEMATCH:
    case PEND_MATCH:
      pcn_out_prompt(pt, CODE_INFO, FORMAT_s_WITHDRAWS_THE_MATCH_OFFER_n, parray[pf].pname);
      pcn_out(pf, CODE_INFO, FORMAT_sYOU_WITHDRAW_THE_MATCH_OFFER_TO_s_n, parray[pt].pname);
      break;
    case PEND_TEACH:
      pcn_out_prompt(pt, CODE_INFO, FORMAT_s_WITHDRAWS_THE_TEACHING_MATCH_OFFER_n, parray[pf].pname);
      pcn_out(pf, CODE_INFO, FORMAT_YOU_WITHDRAW_THE_TEACHING_MATCH_OFFER_TO_s_n, parray[pt].pname);
      break;
    case PEND_PAUSE:
      pcn_out_prompt(pt, CODE_INFO, FORMAT_s_WITHDRAWS_PAUSE_REQUEST_n, parray[pf].pname);
      pcn_out(pf, CODE_INFO, FORMAT_YOU_WITHDRAW_THE_PAUSE_REQUEST_TO_s_n, parray[pt].pname);
      break;
    case PEND_ADJOURN:
      pcn_out_prompt(pt, CODE_INFO, FORMAT_s_WITHDRAWS_THE_ADJOURN_REQUEST_n, parray[pf].pname);
      pcn_out(pf, CODE_INFO, FORMAT_YOU_WITHDRAW_THE_ADJOURN_REQUEST_TO_s_n, parray[pt].pname);
      break;
    case PEND_PAIR:
      pcn_out_prompt(pt, CODE_INFO, FORMAT_s_WITHDRAWS_THE_PAIR_REQUEST_n, parray[pf].pname);
      pcn_out(pf, CODE_INFO, FORMAT_YOU_WITHDRAW_THE_PAIR_REQUEST_TO_s_n, parray[pt].pname);
      break;
    case PEND_DONE:
      pcn_out_prompt(pf, CODE_INFO, FORMAT_YOU_WILL_NEED_TO_TYPE_qDONEq_AGAIN_n);
      break;
    }
    player_pending_delete(ptr);
    count++;
  }
  return count;
}

int player_is_observe(int p, int g)
{
  int i;

  for (i = 0; i < parray[p].session.num_observe; i++) {
    if (parray[p].session.observe_list[i] == g)
      break;
  }
  if (i == parray[p].session.num_observe)
    return 0;
  else
    return 1;
}

int player_add_observe(int p, int g)
{
  if (parray[p].session.num_observe == MAX_OBSERVE)
    return -1;
  parray[p].session.observe_list[parray[p].session.num_observe] = g;
  parray[p].session.num_observe++;
  parray[p].session.protostate = STAT_OBSERVING;
  return 0;
}

int player_remove_observe(int p, int g)
{
  int i;

  for (i = 0; i < parray[p].session.num_observe; i++) {
    if (parray[p].session.observe_list[i] == g)
      break;
  }
  if (i == parray[p].session.num_observe)
    return -1;			/* Not found! */
  for (; i < parray[p].session.num_observe - 1; i++) {
    parray[p].session.observe_list[i] = parray[p].session.observe_list[i + 1];
  }
  parray[p].session.num_observe--;
  if (parray[p].session.num_observe == 0) {
    parray[p].session.protostate = STAT_WAITING;
    parray[p].session.observe_list[0] = -1;
  }
  parray[p].forget.last_command_time = globclock.time;
  return 0;
}

int player_game_ended(int g)
{
  int p;

  for (p = 0; p < parray_top; p++) {
    if (!parray[p].slotstat.is_connected) continue;
    player_remove_observe(p, g);
  }
  player_remove_requests(garray[g].white.pnum, garray[g].black.pnum, -1);
  player_remove_requests(garray[g].black.pnum, garray[g].white.pnum, -1);
  return 0;
}


int player_num_messages(int p)
{

  if (!parray[p].slotstat.is_registered) return 0;
  return xylines_file(FILENAME_PLAYER_cs_MESSAGES, parray[p].login);
}


int player_add_message(int top, int fromp, char *message)
{
  FILE *fp;
  time_t tt = globclock.time;

  if (!parray[top].slotstat.is_registered) return -1;
  if (!parray[fromp].slotstat.is_registered) return -1;
  if ((xylines_file(FILENAME_PLAYER_cs_MESSAGES,parray[top].login) >= MAX_MESSAGES) && (parray[top].adminLevel < ADMIN_ADMIN))
    return -1;
  fp = xyfopen(FILENAME_PLAYER_cs_MESSAGES, "a", parray[top].login);
  if (!fp) return -1;
  fprintf(fp, "%s at %s UTC: %s\n", parray[fromp].pname, time2str_utc(&tt), message);
  fclose(fp);
  return 0;
}


int player_show_messages(int p)
{

  if (!parray[p].slotstat.is_registered) return -1;
  /* if (lines_file(fname) <= 0) return -1; */
  pxysend_file(p, FILENAME_PLAYER_cs_MESSAGES, parray[p].login );
  pcn_out(p, CODE_INFO, FORMAT_PLEASE_TYPE_qERASEq_TO_ERASE_YOUR_MESSAGES_AFTER_READINGn);
  return 0;
}


int player_clear_messages(int p)
{

  if (!parray[p].slotstat.is_registered) return -1;
  xyunlink(FILENAME_PLAYER_cs_MESSAGES, parray[p].login );
  return 0;
}

/* 
 * look for EXACT match in parray.
 * If found: fix slot and return slotnumber
 * If not found: check registered user's files and maybe read in.
 */
int player_fetch(const char *name)
{
  int slot, rc;

  /* exact match with connected player? */
  if ((slot = player_find_login(name)) >= 0) {
    player_fix(slot);
    return slot ;
  }
  /* exact match with registered player? */
  rc = xystat(NULL, FILENAME_PLAYER_cs, name);
  if (rc<0) return -1;

  slot = player_new();
  if (slot < 0) return slot;
  do_copy(parray[slot].login, name, sizeof parray[slot].login);
  do_copy(parray[slot].pname, name, sizeof parray[slot].pname);
  if (player_read(slot)) {
    player_clear(slot);
    return -1;
  }
  return slot;
}


int player_find_sloppy(const char *name)
/*
 * Find player matching the given string.
 * (1) exact match with a logged in player,
 * (2) exact match with a registered player,
 * (3) a partial unique match with a logged in player,
 * {{{{{{(4) a partial match with a registered player. }}}}}}
 *
 *  Returns slot number if the player was found
 *  -1 if no player was found
 *  -cnt if more players were found
 *  iff one player was found, it's slot is fixed.
 */
{
  int slot;
  char lower[sizeof parray[0].pname];

  do_copy(lower,name, sizeof lower);
  stolower(lower);
  /* exact match with connected player? */
  /* exact match with registered player? */
  if ((slot = player_fetch(lower)) >= 0) {
    return slot ;
  }
  /* partial match with connected player? */
  if ((slot = player_find_part_login(lower)) >= 0) {
    player_fix(slot);
    return slot ;
  }
  return -1;
}


int player_kill(char *name)
{
  char fname[MAX_FILENAME_SIZE], fname2[MAX_FILENAME_SIZE];

  sprintf(fname, "%s/%c/%s", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s", conffile.player_dir, name[0], name);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.games", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.games", conffile.player_dir, name[0], name);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.logons", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.logons", conffile.player_dir, name[0], name);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.messages", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.messages", conffile.player_dir, name[0], name);
  rename(fname, fname2);
  return 0;
}


int player_rename(char *name, char *newname)
{
  char fname[MAX_FILENAME_SIZE], fname2[MAX_FILENAME_SIZE];

  sprintf(fname, "%s/%c/%s", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/%s", conffile.player_dir, newname[0], newname);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.games", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/%s.games", conffile.player_dir, newname[0], newname);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.logons", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/%s.logons", conffile.player_dir, newname[0], newname);
  rename(fname, fname2);
  sprintf(fname, "%s/%c/%s.messages", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/%s.messages", conffile.player_dir, newname[0], newname);
  rename(fname, fname2);
  return 0;
}


int player_raise(char *name)
{
  char fname[MAX_FILENAME_SIZE], fname2[MAX_FILENAME_SIZE];

  sprintf(fname, "%s/%c/%s", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s", conffile.player_dir, name[0], name);
  rename(fname2, fname);
  sprintf(fname, "%s/%c/%s.games", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.games", conffile.player_dir, name[0], name);
  rename(fname2, fname);
  sprintf(fname, "%s/%c/%s.logons", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.logons", conffile.player_dir, name[0], name);
  rename(fname2, fname);
  sprintf(fname, "%s/%c/%s.messages", conffile.player_dir, name[0], name);
  sprintf(fname2, "%s/%c/.rem.%s.messages", conffile.player_dir, name[0], name);
  rename(fname2, fname);
  return 0;
}
