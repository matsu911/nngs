/* gamedb.c
 *
 */
/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996 Erik Van Riper (geek@willent.com)
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

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "gamedb.h"
#include "gameproc.h"
#include "common.h"
#include "command.h"
#include "servercodes.h"
#include "playerdb.h"
#include "utils.h"

#if WANT_NNGSRATED
#include "rdbm.h"		/* PEM */
#endif

#ifndef DEBUG_GAME_SLOT
#define DEBUG_GAME_SLOT 0
#endif

static int get_empty_game_slot(void);
static void game_zero(struct game *g0, int size);
static void game_free(struct game *gp);

/* [PEM]: A desperate attempt to fix the komi bug when loading
** adjourned games. :-(
**
** Note: This is not a complete string-to-float parser, it doesn't
**       handle exponents.
*/
static float
nngs_strtof(char *str, char **p)
{
  int ok = 0, sign = 1;
  float f = 0.0;
  char *s = str;

  while (isspace((int)*s)) ++s;
  if (*s == '+') ++s;
  else if (*s == '-') {
    sign = -1;
    ++s;
  }
  while (isdigit((int)*s)) {
    f = 10 * f + (*s - '0');
    ok = 1;
    ++s;
  }
  if (*s == '.') {
    float d = 10.0;

    ++s;
    while (isdigit((int)*s)) {
      f += (*s - '0')/d;
      d *= 10;
      ok = 1;
      ++s;
    }
  }
  if (ok) {
    *p = s;
    return sign*f;
  }
  *p = str;
  return 0.0;
}

struct game garray[GARRAY_SIZE];
int garray_top = 0;

static int get_empty_game_slot(void)
{
  int i;

  for (i = 0; i < garray_top; i++) {
#if (DEBUG_GAME_SLOT >= 2)
    Logit("empty_game_slot:= %d/%d st=%d", i, garray_top, garray[i].gstatus );
#endif
    if (garray[i].slotstat.in_use) continue;
    /* if (garray[i].gstatus != GSTATUS_EMPTY) continue; */
#if DEBUG_GAME_SLOT
      Logit("get_empty_game_slot:= %d/%d (Found one)", i, garray_top);
#endif
    return i;
  }
  if (garray_top >= COUNTOF(garray)) return -1;
  garray_top++;
#if 0
  if (!garray) {	/* garray_top was 0 before */
    garray = malloc(sizeof *garray );
  } else {
    garray = realloc(garray, garray_top * sizeof *garray );
  }
#endif
  if (conffile.debug_general)
    Logit("get_empty_game_slot, garray_top = %d, i = %d (Had to alloc)", garray_top,i);
  return garray_top - 1;
}

int game_new(int type, int size)
{
  int i; 
  
  i = get_empty_game_slot();
  if (conffile.debug_general) Logit("In game_new, i = %d", i);
  game_zero(&garray[i], size);
  garray[i].slotstat.in_use = 1;
  garray[i].gotype = type;
  return i;
}

static void game_zero(struct game *gp, int size)
{
  memset(gp, 0, sizeof *gp);
  gp->gotype = GAMETYPE_GO;
  gp->white.pnum = -1;
  gp->black.pnum = -1;
  gp->white.old_pnum = -1;
  gp->black.old_pnum = -1;
  gp->slotstat.is_playing = 0;
  gp->slotstat.is_stored = 0;
  gp->gstatus = (size > 0) ? GSTATUS_NEW : GSTATUS_EMPTY;
  gp->rated = 1;
  /* gp->nocaps = 0; */
  gp->Private = 0;
  gp->komi = 0.5;
  gp->result = END_NOTENDED;
  gp->num_pass = 0;
  gp->gtitle = NULL;
  gp->gevent = NULL;
  gp->minkg = NULL;
  gp->teach = 0;
  gp->ts.time_type = TIMETYPE_UNTIMED;
  gp->ts.totalticks = SECS2TICS(300);
  gp->ts.byoticks = SECS2TICS(300);
  gp->ts.byostones = 25;
  gp->white.ticksleft = SECS2TICS(300);
  gp->white.byostones = -1;
  gp->white.byoperiods = 0;
  gp->black.ticksleft = SECS2TICS(300);
  gp->black.byostones = -1;
  gp->black.byoperiods = 0;
  gp->Ladder9 = 0;
  gp->Ladder19 = 0;
  gp->Tourn = 0;
  gp->Ladder_Possible = 0;
#if WANT_PAIR
  gp->pairwith = 0;;
  gp->pairstate = NOTPAIRED;
#endif
  if (size) {
    gp->mvinfos = malloc(sizeof *gp->mvinfos);
    gp->mvinfos[0].kibitz = NULL;
    gp->mvinfos[0].last = &(gp->mvinfos[0].kibitz);
    gp->nmvinfos = 1;
  } else {
    gp->mvinfos = NULL;
    gp->nmvinfos = 0;
  }
}

static void freekib(struct kibitz *k)
{
  if (!k) return;
  freekib(k->next);
  free(k->mess);
  free(k);
}

static void freemvinfos(struct game *gp)
{
  int i;

  for (i=0; i<gp->nmvinfos; i++) freekib(gp->mvinfos[i].kibitz);
  free(gp->mvinfos);
  gp->nmvinfos = 0;
  gp->mvinfos = NULL;
}

static void game_free(struct game *gp)
{
  if (gp->gotype >= GAMETYPE_GO) {
    gp->gotype = 0;
    if (gp->minkg) {freeminkgame(gp->minkg); gp->minkg = NULL; }
    if (gp->mvinfos) freemvinfos(gp);
    if (gp->gtitle) {free(gp->gtitle); gp->gtitle = NULL; }
    if (gp->gevent) {free(gp->gevent); gp->gevent = NULL; }
  }
}


int game_remove(int g0)
{

  /* Should remove game from players observation list */
  game_free(&garray[g0]);
  game_zero(&garray[g0],0);
  garray[g0].slotstat.in_use = 0;
  garray[g0].slotstat.is_playing = 0;
  garray[g0].slotstat.is_stored = 0;
  garray[g0].gstatus = GSTATUS_EMPTY; 
  return 0;
}

/* This may eventually save old games for the 'oldmoves' command */
int game_finish(int g0)
{
  player_game_ended(g0);	/* Alert playerdb that game ended */
  NewOldGame(g0);	/* remember game until both players disconnect */
  return 0;
}

void add_kib(struct game *gp, int movecnt, char *s)
{
  int i;
  struct kibitz **k;

  if (!s) {Logit("s is null"); return; }
  /*Logit("Adding kibitz >%s< at move %d", s, movecnt); */
  while (movecnt >= gp->nmvinfos) {
    i = gp->nmvinfos;
    gp->nmvinfos *= 2;
    gp->mvinfos = realloc(gp->mvinfos, gp->nmvinfos * sizeof *gp->mvinfos);
    for (; i< gp->nmvinfos; i++) {
      gp->mvinfos[i].kibitz = NULL;
      gp->mvinfos[i].last = &(gp->mvinfos[i].kibitz);
    }
  }
  k = gp->mvinfos[movecnt].last;
  *k = malloc(sizeof **k);
  (*k)->next = NULL;
  (*k)->mess = mystrdup(s);
  gp->mvinfos[movecnt].last = &((*k)->next);
}

void send_go_board_to(int g0, int p)
{
  int side, count;
  int observing = 0;
  int yy, wc, bc, bp, wp;
  twodstring statstring;
  char buf[20];
  char bbuf[20], wbuf[20];

  if (parray[p].session.gnum == g0) {
    side = parray[p].session.side;
  } else {
    observing = 1;
    side = PLAYER_WHITE;
  }
  /*game_update_time(g0);*/
  bp = garray[g0].black.pnum;
  wp = garray[g0].white.pnum;
  mink_getcaps(garray[g0].minkg, &wc, &bc);
  bbuf[0] = wbuf[0] = 0;
  if (parray[p].i_verbose) {
    if (garray[g0].black.byostones > 0) sprintf(bbuf, " B %d", garray[g0].black.byostones);
    if (garray[g0].white.byostones > 0) sprintf(wbuf, " B %d", garray[g0].white.byostones);
    mink_printboard(garray[g0].minkg, statstring);
    count = mink_movenum(garray[g0].minkg);
    if (count > 0) mink_listmove(garray[g0].minkg, count, buf);
    pcn_out(p, CODE_CR1|CODE_OBSERVE, FORMAT_GAME_d_I_s_ss_VS_s_ss_n,
                /*SendCode(p, CODE_BEEP),*/
		g0 + 1,
		parray[wp].pname,
		parray[wp].srank,
                parray[wp].flags.is_rated ? "*" : " ",
		parray[bp].pname,
		parray[bp].srank,
                parray[bp].flags.is_rated ? "*" : " ");
    for(yy=0; yy<(garray[g0].minkg->height) + 2; yy++){
      if (yy == 0) 
        pcn_out(p, CODE_OBSERVE, FORMAT_s_H_CAP_d_KOMI_fn, 
		/*SendCode(p, CODE_BEEP),*/
		statstring[yy],
		garray[g0].minkg->handicap,
		garray[g0].komi);
      else if (yy==1)
	pcn_out(p, CODE_OBSERVE, FORMAT_s_CAPTURED_BY_dn, statstring[yy], wc);
      else if (yy==2)
	pcn_out(p, CODE_OBSERVE, FORMAT_s_CAPTURED_BY_O_dn, statstring[yy], bc);
      else if (yy==4 && garray[g0].minkg->height > 3)
	pcn_out(p, CODE_OBSERVE, FORMAT_s_WH_TIME_ssn
               , statstring[yy]
               , secs2hms_mask(TICS2SECS(garray[g0].white.ticksleft), 7)
               , wbuf );
      else if (yy==5 && garray[g0].minkg->height > 4)
	pcn_out(p, CODE_OBSERVE, FORMAT_s_BL_TIME_ssn
               , statstring[yy]
               , secs2hms_mask(TICS2SECS(garray[g0].black.ticksleft), 7)
               , bbuf);
      else if (yy==7 && garray[g0].minkg->height > 6)
	if (count == 0)
	  pcn_out(p, CODE_OBSERVE, FORMAT_s_LAST_MOVE_n, statstring[yy]);
 	else
	  pcn_out(p, CODE_OBSERVE, FORMAT_s_LAST_MOVE_sn, statstring[yy], buf + 1);
      else if (yy==8 && garray[g0].minkg->height > 7)
 	if (count == 0)
	  pcn_out(p, CODE_OBSERVE, FORMAT_s_0_O_WHITE_n, statstring[yy]);
   	else
	  pcn_out(p, CODE_OBSERVE, FORMAT_s_d_s_s_n, statstring[yy],
                      count, (buf[0] == 'B') ? "#" : "O", 
		      (buf[0] == 'B') ? "Black" : "White");
      else if (yy>=10 && garray[g0].minkg->height > (yy - 1)) {
	if (count > 1) {
	  count--;
          mink_listmove(garray[g0].minkg, count, buf);
          pcn_out(p, CODE_OBSERVE, FORMAT_s_c_d_sn, statstring[yy],
		      buf[0], count, buf + 1);
	}
	else pcn_out(p, CODE_OBSERVE, FORMAT_sn, statstring[yy]);
      }
      else pcn_out(p, CODE_OBSERVE, FORMAT_sn, statstring[yy]);
    }
  }
  pprintf_prompt(p, ""); 
}

void send_go_boards(int g0, int players_only)
{
  int p, bc, wc, wp, bp;
  char buf[20], outStr[1024];

  mink_listmove(garray[g0].minkg, mink_movenum(garray[g0].minkg), buf); 
  mink_getcaps(garray[g0].minkg, &wc, &bc);

  bp = garray[g0].black.pnum;
  wp = garray[g0].white.pnum;
  sprintf(outStr, "Game %d %s: %s (%d %d %d) vs %s (%d %d %d)\n",
        g0 + 1, "I",
        parray[wp].pname, bc,
        TICS2SECS(garray[g0].white.ticksleft),
        garray[g0].white.byostones,

        parray[bp].pname, wc,
        TICS2SECS(garray[g0].black.ticksleft),
        garray[g0].black.byostones);

  if (parray[wp].i_verbose && garray[g0].teach != 1 ) 
    send_go_board_to(g0, wp);

  else if (parray[wp].session.protostate != STAT_SCORING && garray[g0].teach != 1)  { 
    pcn_out(wp, CODE_MOVE, FORMAT_s,outStr);
    if (mink_movenum(garray[g0].minkg) - 1 >= 0) {
      pcn_out(wp, CODE_MOVE, FORMAT_d_c_sn,
        mink_movenum(garray[g0].minkg) - 1, buf[0], buf + 1); 
    }
    if (parray[wp].flags.want_bell) pcn_out_prompt(wp, CODE_CR1|CODE_BEEP, FORMAT_n);
    else pcn_out_prompt(wp, CODE_CR1|CODE_NONE,FORMAT_n);
  }

  if (parray[bp].i_verbose) send_go_board_to(g0, bp);

  else if (parray[bp].session.protostate != STAT_SCORING) {
    pcn_out(bp, CODE_MOVE, FORMAT_s, outStr);
    if (mink_movenum(garray[g0].minkg) - 1 >= 0) {
      pcn_out(bp, CODE_MOVE, FORMAT_d_c_sn,
        mink_movenum(garray[g0].minkg) - 1, buf[0], buf + 1); 
    }
    if (parray[bp].flags.want_bell) pcn_out_prompt(bp, CODE_BEEP, FORMAT_n);
    else pcn_out_prompt(bp, CODE_NONE, FORMAT_n);
  }

  if (players_only) return;

  for (p = 0; p < parray_top; p++) {
    if (!parray[p].slotstat.is_online) continue;
    if (!player_is_observe(p, g0)) continue;
    if (parray[p].session.gnum == g0) continue;
    if (parray[p].i_verbose) send_go_board_to(g0, p);
    else {
      pcn_out(p, CODE_MOVE, FORMAT_s, outStr);
      pcn_out(p, CODE_MOVE, FORMAT_d_c_sn,
         mink_movenum(garray[g0].minkg) - 1, buf[0], buf + 1);
      if (parray[p].flags.want_bell) pcn_out_prompt(p, CODE_BEEP, FORMAT_n);
      else pcn_out_prompt(p, CODE_NONE, FORMAT_n);
    }
  }
} 


int game_get_num_ob(int g0)
{
  int p, t, count = 0;

  for(p = 0; p < parray_top; p++) {
    for(t = 0; t < parray[p].session.num_observe; t++) {
      if (parray[p].session.observe_list[t] == g0) count++;
      }
  }
  return count;
}

static int oldGameArray[MAXOLDGAMES];
static int numOldGames = 0;

static int RemoveOldGame(int g0)
{
  int i;

  for (i = 0; i < numOldGames; i++) {
    if (oldGameArray[i] == g0) break;
  }
  if (i == numOldGames)
    return -1;			/* Not found! */
  for (; i < numOldGames - 1; i++)
    oldGameArray[i] = oldGameArray[i + 1];
  numOldGames--;
  game_remove(g0);
  return 0;
}

static int AddOldGame(int g0)
{
  if (numOldGames == MAXOLDGAMES)	/* Remove the oldest */
    RemoveOldGame(oldGameArray[0]);
  oldGameArray[numOldGames] = g0;
  numOldGames++;
  return 0;
}

int FindOldGameFor(int p)
{
  int i;

  if (p < 0)
    return -1;
  for (i = numOldGames - 1; i >= 0; i--) {
    if (garray[oldGameArray[i]].white.old_pnum == p) return oldGameArray[i];
    if (garray[oldGameArray[i]].black.old_pnum == p) return oldGameArray[i];
  }
  return -1;
}

/* This just removes the game if both players have new-old games */
int RemoveOldGamesForPlayer(int p)
{
  int g0;

  g0 = FindOldGameFor(p);
  if (g0 < 0) return 0;
  if (garray[g0].white.old_pnum == p) garray[g0].white.old_pnum = -1;
  if (garray[g0].black.old_pnum == p) garray[g0].black.old_pnum = -1;
  if (garray[g0].white.old_pnum == -1 && garray[g0].black.old_pnum == -1) {
    RemoveOldGame(g0);
  }
  return 0;
}

/* This recycles any old games for players who disconnect */
int ReallyRemoveOldGamesForPlayer(int p)
{
  int g0;

  g0 = FindOldGameFor(p);
  if (g0 < 0) return 0;
  RemoveOldGame(g0);
  return 0;
}

int NewOldGame(int g0)
{
  RemoveOldGamesForPlayer(garray[g0].white.pnum);
  RemoveOldGamesForPlayer(garray[g0].black.pnum);
  garray[g0].white.old_pnum = garray[g0].white.pnum;
  garray[g0].black.old_pnum = garray[g0].black.pnum;
  garray[g0].slotstat.is_playing = 0;
  garray[g0].slotstat.is_stored = 1;
  garray[g0].gstatus = GSTATUS_STORED;
  AddOldGame(g0);
  return 0;
}

void game_disconnect(int g0, int p)
{
#if WANT_PAIR
  if (paired(parray[p].session.gnum)) {
    game_ended(garray[g0].pairwith, PLAYER_NEITHER, END_LOSTCONNECTION);
  }
#endif /* WANT_PAIR */
  game_ended(g0, PLAYER_NEITHER, END_LOSTCONNECTION);
}

static void savekib(FILE *fp, struct game *gp)
{
  int i;
  struct kibitz *kp;

  fprintf(fp, "kibitz: oink\n");
  for (i=0; i < gp->nmvinfos; i++) {
    for (kp = gp->mvinfos[i].kibitz; kp; kp = kp->next) {
       fprintf(fp,"%d %s\n", i, kp->mess);
    }
  }
  fputc('\n', fp);
}

static void loadkib(FILE *fp, struct game *gp)
{
  char buf[256];
  int i,k;
  size_t len;

  while (fgets(buf, sizeof buf, fp)) {
    len = strlen(buf);
    if (!len) continue;
    if (len >= sizeof buf) len = sizeof buf ;
    buf[len-1] = '\0';	/* strip '\n' */
    if (is_totally_blank(buf)) {
      /* if (conffile.debug_general) */ Logit("Got my blank line in loadkib");
      continue;
    }
    sscanf(buf, "%d %n", &i,&k);
    add_kib(gp, i, buf+k);
  }
}

static int got_attr_value(struct game *gp, char *attr, char *value, FILE * fp, char *fname)
{
  if (!strcmp(attr, "timestart:")) {
    gp->timeOfStart = atoi(value);
  } else if (!strcmp(attr, "timetype:")) {
    gp->ts.time_type = atoi(value);
  } else if (!strcmp(attr, "rules:")) {
    gp->rules = atoi(value);
  } else if (!strcmp(attr, "totalticks:")) {
    gp->ts.totalticks = atoi(value);
  } else if (!strcmp(attr, "b_penalty:")) {
    gp->black.penalty = atoi(value);
  } else if (!strcmp(attr, "b_over:")) {
    gp->black.byoperiods = atoi(value);
  } else if (!strcmp(attr, "w_penalty:")) {
    gp->white.penalty = atoi(value);
  } else if (!strcmp(attr, "w_over:")) {
    gp->white.byoperiods = atoi(value);
  } else if (!strcmp(attr, "title:")) {
    gp->gtitle = mystrdup(value);
  } else if (!strcmp(attr, "event:")) {
    gp->gevent = mystrdup(value);
  } else if (!strcmp(attr, "handicap:")) {
    /*sethcap(gp->minkg, atoi(value))*/;
  } else if (!strcmp(attr, "size:")) {
    gp->size = atoi(value);
    gp->minkg = mink_initgame(gp->size, gp->size, gp->rules);
  } else if (!strcmp(attr, "onmove:")) {
    ; /* PEM: Ignore. gp->onMove = atoi(value); */
  } else if (!strcmp(attr, "ladder9:")) {
    gp->Ladder9 = atoi(value);
  } else if (!strcmp(attr, "teach2:")) {
    gp->teach = 2;
  } else if (!strcmp(attr, "teach:")) {
    gp->teach = atoi(value);
  } else if (!strcmp(attr, "ladder_possible:")) {
    gp->Ladder_Possible = atoi(value);
  } else if (!strcmp(attr, "ladder19:")) {
    gp->Ladder19 = atoi(value);
  } else if (!strcmp(attr, "tourn:")) {
    gp->Tourn = atoi(value);
    if (gp->Tourn == 1) {
      parray[gp->white.pnum].match_type = GAMETYPE_TNETGO;
      parray[gp->black.pnum].match_type = GAMETYPE_TNETGO;
    }
  } else if (!strcmp(attr, "w_time:")) {
    gp->white.ticksleft = atoi(value);
  } else if (!strcmp(attr, "b_time:")) {
    gp->black.ticksleft = atoi(value);
  } else if (!strcmp(attr, "byo:")) {
    gp->ts.byoticks = atoi(value);
  } else if (!strcmp(attr, "byos:")) {
    gp->ts.byostones = atoi(value);
  } else if (!strcmp(attr, "w_byo:")) {
    gp->white.byoperiods = atoi(value);
  } else if (!strcmp(attr, "b_byo:")) {
    gp->black.byoperiods = atoi(value);
  } else if (!strcmp(attr, "w_byostones:")) {
    gp->white.byostones = atoi(value);
  } else if (!strcmp(attr, "b_byostones:")) {
    gp->black.byostones = atoi(value);
  } else if (!strcmp(attr, "clockstopped:")) {
    gp->clockStopped = atoi(value);
  } else if (!strcmp(attr, "rated:")) {
    gp->rated = atoi(value);
/*  } else if (!strcmp(attr, "nocaps:")) {
    gp->nocaps = atoi(value); */
  } else if (!strcmp(attr, "private:")) {
    gp->Private = atoi(value);
  } else if (!strcmp(attr, "type:")) {
    gp->type = atoi(value);
  } else if (!strcmp(attr, "time_type:")) {
    gp->ts.time_type = atoi(value);
  } else if (!strcmp(attr, "gotype:")) {
    gp->gotype = atoi(value);
  } else if (!strcmp(attr, "numpass:")) {
    gp->num_pass = atoi(value);
  } else if (!strcmp(attr, "komi:")) {
    /*
    ** PEM: Changed to the locally defined nngs_strtof().
    */
    char *pp;

    gp->komi = nngs_strtof(value, &pp);
    if (pp == value || gp->komi < -12.0 || 12.0 < gp->komi)
    {
      if (pp == value)
	Logit("Bad komi value \"%s\"", value);
      else
	Logit("Bad komi value \"%s\" --> %g", value, gp->komi);
      switch(gp->size) {
        case 9 : gp->komi = conffile.default_komi9; break;
        case 13 : gp->komi = conffile.default_komi13; break;
        default:
        case 19 : gp->komi = conffile.default_komi19; break;
      }
    }
  } else if (!strcmp(attr, "movesrnext:")) {  /* value meaningless */
    /* PEM: Get the true onMove. */
    switch (mink_loadgame(fp, gp->minkg)) {
    case MINK_BLACK:
      gp->onMove = PLAYER_BLACK;
      break;
    case MINK_WHITE:
      gp->onMove = PLAYER_WHITE;
      break;
    }
  } else if (!strcmp(attr, "kibitz:")) {  /* value meaningless */
    loadkib(fp, gp);
  } else {
    Logit("Error bad attribute >%s< from file %s", attr, fname);
  }
  /* setnocaps(gp->minkg, gp->nocaps); */
  return 0;
}


#define MAX_GLINE_SIZE 1024
int game_read(struct game *gp, int wp, int bp)
{
  FILE *fp;
  int len;
  char *attr, *value;
  char line[MAX_GLINE_SIZE];

  gp->white.pnum = wp;
  gp->black.pnum = bp;
  gp->white.old_pnum = -1;
  gp->black.old_pnum = -1;
  /* gp->gameNum = g0; */

  fp = xyfopen(FILENAME_GAMES_ws_s, "r", parray[wp].login, parray[bp].login);
  if (!fp) return -1;

  /* Read the game file here */
  while (fgets(line, sizeof line, fp)) {
    if ((len = strlen(line)) <= 1) continue;
    line[len - 1] = '\0';
    attr = eatwhite(line);
    if (attr[0] == '#') continue;			/* Comment */
    if (attr[0] == ';') {
      Logit("Read move %s from game record %s!", attr, filename() );
      continue;			/* Move!  Should not get here! */
    }
    value = eatword(attr);
    if (!*value) {
      Logit("Error reading file %s", filename() );
      continue;
    }
    *value = '\0';
    value++;
    value = eatwhite(value);
    if (!*value) {
      Logit("NNGS: Error reading file %s", filename() );
      continue;
    }
    stolower(attr);
    if (got_attr_value(gp, attr, value, fp, filename() )) {
      fclose(fp);
      return -1;
    }
  }

  fclose(fp);
  gp->slotstat.is_playing = 1;
  gp->gstatus = GSTATUS_ACTIVE;
  gp->starttick = globclock.tick;
  gp->lastMovetick = gp->starttick;
  gp->lastDectick = gp->starttick;

  /* PEM: This used to be done when saving, but that broke things. */
  if (gp->num_pass >= 2) {
    mink_back(gp->minkg);
    gp->num_pass = 1;
    if (gp->onMove == PLAYER_WHITE) gp->onMove = PLAYER_BLACK;
    else gp->onMove = PLAYER_WHITE;
  }
  /* Need to do notification and pending cleanup */
  return 0;
}

int game_delete(int wp, int bp)
{

  xyunlink(FILENAME_GAMES_bs_s, parray[wp].login, parray[bp].login);
  if (wp != bp) 
  xyunlink(FILENAME_GAMES_ws_s, parray[wp].login, parray[bp].login);
  return 0;
}

int game_save_complete(int g0, FILE *fp, twodstring statstring)
{
  time_t now;
  int wp, bp, owp = 0, obp = 0;	/* Init. to shut up warnings. */
  char resu[10];
  char *tmp;

  if (!fp) return -1;
  wp = garray[g0].white.pnum;
  bp = garray[g0].black.pnum;
#if WANT_PAIR
  if (paired(g0)) {
    int g2 = garray[g0].pairwith;
    obp = garray[g2].black.pnum;
    owp = garray[g2].white.pnum;
  }
#endif
  now = globclock.time;
	/* This is ugly: depends on static/global last filename used */
  tmp = strrchr(filename(), '/');
  if (!tmp) tmp = filename();
  else if (*tmp == '/') tmp++;
  if (garray[g0].gresult == 0.0) sprintf(resu, "Resign");
  else if (garray[g0].gresult == -1.0) sprintf(resu, "Time");
  else sprintf(resu, "%.1f", garray[g0].gresult);
  fprintf(fp, "\n(;\n");
  fprintf(fp, "GM[1]FF[4]AP[NNGS:%s]\n", conffile.version_string);
  fprintf(fp, "US[Brought to you by %s]\n", conffile.server_name);
  fprintf(fp, "CP[\n\
  Copyright This game was played on %s\n\
  Permission to reproduce this game is given,\n\
  as long as this copyright notice is preserved.]\n", conffile.server_name);
  if (garray[g0].Ladder9 == 1 || garray[g0].Ladder19 == 1) {
    fprintf(fp, "GN[%s-%s(B) NNGS (LADDER RATED)]\n",
     parray[wp].pname, parray[bp].pname);
#if WANT_PAIR
  } else if (paired(g0)) {
    fprintf(fp, "GN[%s-%s vs %s-%s(B) NNGS (RENGO)]\n",
     parray[wp].pname, parray[owp].pname,
     parray[bp].pname, parray[obp].pname);
#endif
  } else if (garray[g0].Tourn == 1) {
    fprintf(fp, "GN[%s-%s(B) NNGS (Tournament)]\n",
     parray[wp].pname, parray[bp].pname);
  } else {
    fprintf(fp, "GN[%s-%s(B) NNGS]\n",
     parray[wp].pname, parray[bp].pname);
  }
  fprintf(fp, "EV[%s]\n", IFNULL(garray[g0].gevent, "none") );
  fprintf(fp, "RE[%s+%s]\n",
     (garray[g0].winner == wp ? "W" : "B"), resu);
  fprintf(fp, "PW[%s]WR[%s%s]\n", 
               parray[wp].pname, 
               parray[wp].slotstat.is_registered ? parray[wp].srank : "UR",
               parray[wp].flags.is_rated ? "*" : " ");
  fprintf(fp, "PB[%s]BR[%s%s]\n", 
               parray[bp].pname, 
               parray[bp].slotstat.is_registered ? parray[bp].srank : "UR",
               parray[bp].flags.is_rated ? "*" : " ");
  fprintf(fp, "PC[%s: %s %s]\n"
  , conffile.server_name, conffile.server_address, conffile.server_ports);
  fprintf(fp, "DT[%s]\n", time2str_sgf(&now));
  fprintf(fp, "SZ[%d]TM[%d]KM[%.1f]\n\n", garray[g0].minkg->width,
     TICS2SECS(garray[g0].ts.totalticks), garray[g0].komi);
  if (conffile.debug_general) Logit("garray[g0].nmvinfos = %d", garray[g0].nmvinfos);
  mink_savegame(fp, garray[g0].minkg, garray[g0].mvinfos, garray[g0].nmvinfos);

  if (statstring) {   /* record territory in SGF file */
    int x, y, n;      /*  - added on 11/19/98 by nic */

    fprintf(fp, ";");
    n = 0;
    for (x = 0; x < garray[g0].minkg->height; x++)
      for (y = 0; y < garray[g0].minkg->width; y++) {
        if (statstring[x][y] != '5') continue;
        if (!(n%18)) fprintf(fp, "\n");
        if (!n++) fprintf(fp, "TB");
        fprintf(fp, "[%c%c]", 'a' + x, 'a' + y);
        }

    n = 0;
    for (x = 0; x < garray[g0].minkg->height; x++)
      for (y = 0; y < garray[g0].minkg->width; y++) {
        if (statstring[x][y] != '4') continue;
        if (!(n%18)) fprintf(fp, "\n");
        if (!n++) fprintf(fp, "TW");
        fprintf(fp, "[%c%c]", 'a' + x, 'a' + y);
        }
    fprintf(fp, "\n");
  }

  fprintf(fp, ")\n\n---\n");
  fclose(fp);
  if (garray[g0].teach != 1)
    if (parray[wp].automail) {
      mail_asn(parray[wp].email, tmp, filename() );
    }
  if (parray[bp].automail) {
    mail_asn(parray[wp].email, tmp, filename() );
  }

  return 1;
}

int game_save(int g0)
{
  FILE *fp;
  int wp, bp;
  
  wp = garray[g0].white.pnum;
  bp = garray[g0].black.pnum;
  
  if (mink_movenum(garray[g0].minkg) < conffile.game_save_treshold) return 1;
  fp = xyfopen(FILENAME_GAMES_ws_s, "w",parray[wp].login,parray[bp].login);
  if (!fp) return -1;

  /* Create link for easier stored game finding */
  if (wp!=bp)
    xylink(FILENAME_GAMES_bs_s, parray[wp].login,parray[bp].login);

  fprintf(fp, "Rules: %d\n", garray[g0].rules);
  fprintf(fp, "B_Penalty: %d\n", garray[g0].black.penalty);
  fprintf(fp, "W_Penalty: %d\n", garray[g0].white.penalty);
  fprintf(fp, "Size: %d\n", (int) garray[g0].minkg->width);
  fprintf(fp, "TimeStart: %d\n", (int) garray[g0].timeOfStart);
  fprintf(fp, "W_Time: %d\n", garray[g0].white.ticksleft);
  fprintf(fp, "B_Time: %d\n", garray[g0].black.ticksleft);
  fprintf(fp, "Byo: %d\n", garray[g0].ts.byoticks);
  fprintf(fp, "ByoS: %d\n", garray[g0].ts.byostones);
  fprintf(fp, "W_Byo: %d\n", garray[g0].white.byoperiods);
  fprintf(fp, "B_Byo: %d\n", garray[g0].black.byoperiods);
  fprintf(fp, "W_ByoStones: %d\n", garray[g0].white.byostones);
  fprintf(fp, "B_ByoStones: %d\n", garray[g0].black.byostones);
  fprintf(fp, "ClockStopped: %d\n", garray[g0].clockStopped);
  fprintf(fp, "Rated: %d\n", garray[g0].rated);
  fprintf(fp, "Private: %d\n", garray[g0].Private);
  fprintf(fp, "Type: %d\n", garray[g0].type);
  fprintf(fp, "TimeType: %d\n", garray[g0].ts.time_type);
  fprintf(fp, "Totalticks: %d\n", garray[g0].ts.totalticks);
  fprintf(fp, "GoType: %d\n", garray[g0].gotype);
  fprintf(fp, "NumPass: %d\n", garray[g0].num_pass);
  fprintf(fp, "Komi: %.1f\n", garray[g0].komi);
  fprintf(fp, "Teach: %d\n", garray[g0].teach);
  /*fprintf(fp, "Teach2: %d\n", garray[g0].Teach2); */
  fprintf(fp, "Ladder9: %d\n", garray[g0].Ladder9);
  fprintf(fp, "Ladder19: %d\n", garray[g0].Ladder19);
  fprintf(fp, "Tourn: %d\n", garray[g0].Tourn);
  if (garray[g0].gtitle)
    fprintf(fp, "Title: %s\n", garray[g0].gtitle);
  if (garray[g0].gevent)
    fprintf(fp, "Event: %s\n", garray[g0].gevent);
 /* if (garray[g0].minkg->handicap > 0)
    fprintf(fp, "Handicap: %d\n", garray[g0].minkg->handicap); */
  fprintf(fp, "Ladder_Possible: %d\n", garray[g0].Ladder_Possible);
  fprintf(fp, "OnMove: %d\n", garray[g0].onMove);
  fprintf(fp, "MovesRNext: oinkoink\n");
  mink_savegame(fp, garray[g0].minkg,NULL,0);
  savekib(fp, &garray[g0]);
  fclose(fp);
  return 0;
}

int write_g_out(int g0, FILE *fp, int maxlines, char *fdate)
{
  int wp, bp;
  char wrnk[6], brnk[6];
  char resu[10];

  if (!fp) return 0;
  wp = garray[g0].white.pnum;
  bp = garray[g0].black.pnum;

  if (!parray[wp].slotstat.is_registered || !parray[bp].slotstat.is_registered) {
    fclose(fp);
    return 0;
    }

  sprintf(wrnk, "%3.3s%s", parray[wp].srank, parray[wp].flags.is_rated ? "*" : " ");
  sprintf(brnk, "%3.3s%s", parray[bp].srank, parray[bp].flags.is_rated ? "*" : " ");

  if (garray[g0].gresult == 0.0) sprintf(resu, "Resign");
  else if (garray[g0].gresult == -1.0) sprintf(resu, "Time");
  else sprintf(resu, "%.1f", garray[g0].gresult);
  fprintf(fp, "%-10s [%s](%s) : %-10s [%s](%s) H %d K %.1f %dx%d %s+%s %s\n",
    (garray[g0].winner == wp ? parray[wp].pname : parray[bp].pname),
    (garray[g0].winner == wp ? wrnk : brnk),
    (garray[g0].winner == wp ? "W" : "B"),
    (garray[g0].winner == wp ? parray[bp].pname : parray[wp].pname),
    (garray[g0].winner == wp ? brnk : wrnk),
    (garray[g0].winner == wp ? "B" : "W"),
     garray[g0].minkg->handicap,
     garray[g0].komi,
     garray[g0].minkg->width, garray[g0].minkg->width,
    (garray[g0].winner == wp ? "W" : "B"),
     resu, fdate);
  fclose(fp);
    /* AvK: sorry, this is ugly ... */
  truncate_file(filename(), maxlines);
  return 1;
}

int pgames(int p, FILE *fp)
{
  char line[1000];
  
  if (!fp) {
    return COM_OK;
  }
  if (parray[p].flags.is_client) pcn_out(p, CODE_THIST, FORMAT_FILEn);
  while (fgets(line, sizeof line, fp)) {
    pprintf(p, "%s",line);
  }
  fclose(fp);
  if (parray[p].flags.is_client) pcn_out(p, CODE_THIST, FORMAT_FILEn);
  return COM_OK;
}

void game_write_complete(int g0, twodstring statstring)
{
  char fdate[40];
  int wp, bp;
  time_t now = globclock.time;
  char wname[sizeof parray[0].pname], bname[sizeof parray[0].pname];
  FILE *fp;

  bp = garray[g0].black.pnum;
  wp = garray[g0].white.pnum;
  strcpy(wname, parray[wp].pname);
  strcpy(bname, parray[bp].pname);

  stolower(wname);
  stolower(bname);
  sprintf(fdate, time2str_file((time_t *) &now));
  fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "a", wname);
  write_g_out(g0, fp, 23, fdate);
  if (wp != bp) {
    fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "a", bname);
    write_g_out(g0, fp, 23, fdate);
  }
  if (garray[g0].rated
     && parray[wp].slotstat.is_registered
     && parray[bp].slotstat.is_registered
     && garray[g0].teach != 1
#if WANT_PAIR
     && !paired(g0)
#endif
     && mink_movenum(garray[g0].minkg) >= 20
     && garray[g0].minkg->width == 19) {
    fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "a", wname);
    write_g_out(g0, fp, 23, fdate);
    fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "a", bname);
    write_g_out(g0, fp, 23, fdate);
  }
  fp = xyfopen(FILENAME_RESULTS, "a" );
  write_g_out(g0, fp, 250, fdate);

  fp = xyfopen(FILENAME_CGAMES_ws_s_s, "w", wname, bname, fdate);
  game_save_complete(g0, fp, statstring);
  xylink(FILENAME_CGAMES_bs_s_s, wname, bname, fdate);

  if (!parray[wp].slotstat.is_registered
     || !parray[bp].slotstat.is_registered
     || !garray[g0].rated
     || garray[g0].teach
#if WANT_PAIR
     || paired(g0)
#endif
     || mink_movenum(garray[g0].minkg) <= 20
     || garray[g0].minkg->width != 19)  return;

  fp = xyfopen(FILENAME_RESULTS, "a");
  if (!fp) {
    return;
  }
  fprintf(fp, "%s -- %s -- %d %.1f %s %s\n", 
               parray[wp].pname,
               parray[bp].pname,
               garray[g0].minkg->handicap,
               garray[g0].komi,
               (garray[g0].winner == wp ? "W" : "B"),
               ResultsDate(fdate));
  fclose(fp);
#if WANT_NNGSRATED
  /* [PEM]: New results file for nrating. */
  {
    rdbm_t rdb;
    rdbm_player_t rp;
    char wrank[8], brank[8];
    struct tm *tp = localtime((time_t *)&now);
    char fname[MAX_FILENAME_SIZE];

    xyfilename(fname, FILENAME_NRATINGS );
    if (!(rdb = rdbm_open(fname,0)))
    {
      strcpy(wrank, "-");
      strcpy(brank, "-");
    }
    else
    {
      if (rdbm_fetch(rdb, parray[wp].pname, &rp)) strcpy(wrank, rp.rank);
      else strcpy(wrank, "-");
      if (rdbm_fetch(rdb, parray[bp].pname, &rp)) strcpy(brank, rp.rank);
      else strcpy(brank, "-");
      rdbm_close(rdb);
    }

    fp = xyfopen(FILENAME_NRESULTS, "a");
    if (!fp) return;
    fprintf(fp, "\"%s\" %s \"%s\" %s %u %.1f %c %02u-%02u-%02u\n",
	    parray[wp].pname,
	    wrank,
	    parray[bp].pname,
	    brank,
	    garray[g0].minkg->handicap,
	    garray[g0].komi,
	    garray[g0].winner == wp ? 'W' : 'B',
	    tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday);
    fclose(fp);
  }
#endif /* WANT_NNGSRATED */
}

unsigned int game_count()
{
  int g0;
  unsigned count = 0;

  for (g0 = 0; g0 < garray_top; g0++) {
    if (!garray[g0].slotstat.in_use ) continue;
    if (!garray[g0].slotstat.is_playing) continue;
    if (garray[g0].gstatus != GSTATUS_ACTIVE) continue;
    count++;
  }
  if (count > game_high) game_high = count;
  return count;
}
