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

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsmain.h"
#include "nngsconfig.h"
#include "gameproc.h"
#include "common.h"
#include "conffile.h"
#include "servercodes.h"
#include "playerdb.h"
#include "gamedb.h"
#include "utils.h"
#include "ladder.h"

	/* Setting this will cause the first 2 moves not to cost any time */
#define WANT_HANDICAP_COURTESY 0

void game_ended(int g0, int winner, int why)
{
  const struct ladderplayer *Ladder_B, *Ladder_W;
  char outstr[180];
  char tmp[50];
  char statZ1[200];
  char statZ2[200];
  int p, p2, pb, pw,
  wterr,  /* W's territory */
  wocc,   /* W stones on the board */
  bterr,  /* B's territory */
  bocc,   /* B stones on the board */
  wcaps,  /* Number of captured W stones (add to B's score in Japanese score) */
  bcaps,  /* Number of captured B stones (add to W's score in Japanese score) */
  rate_change = 0,
  until, yy;

  twodstring statstring;
  float wscore, bscore;
  FILE *fp;
  time_t now;

  now = globclock.time;
  pb=garray[g0].black.pnum ;
  pw=garray[g0].white.pnum ;

  sprintf(outstr, "{Game %d: %s vs %s :",
          g0 + 1,
          parray[pw].pname,
          parray[pb].pname);

  garray[g0].result = why;
  garray[g0].winner = winner;

  switch (why) {
  case END_DONE:
    completed_games++;
    mink_getcaps(garray[g0].minkg, &wcaps, &bcaps);
    mink_countscore(garray[g0].minkg, statstring, &wterr, &bterr, &wocc, &bocc);
    if (conffile.debug_general)
      Logit("k=%.1f wtr=%d, btr=%d, wocc=%d, bocc=%d, wcaps=%d, bcaps=%d",
                 garray[g0].komi, wterr, bterr,  wocc, bocc, wcaps, bcaps);
#if WANT_CHINESESCORE
    if (garray[g0].komi > 0) wscore = wterr + wocc + garray[g0].komi;
    else wscore = wterr + wocc;
    if (garray[g0].komi < 0) bscore = bterr + bocc + fabs(garray[g0].komi);
    else bscore = bterr + bocc;
#endif /* WANT_CHINESESCORE */
    if (garray[g0].komi > 0) wscore = wterr + bcaps + garray[g0].komi;
    else wscore = wterr + bcaps;
    if (garray[g0].komi < 0) bscore = bterr + wcaps + fabs(garray[g0].komi);
    else bscore = bterr + wcaps;
    if (wscore > bscore) {
      garray[g0].gresult = (float) wscore - bscore;
      winner = pw;
    }
    else {
      garray[g0].gresult = (float) bscore - wscore;
      winner = pb;
    }
    sprintf(statZ1, "%s %3.3s%s %d %d %d T %.1f %d\n",
                parray[pw].pname,
                parray[pw].srank,
                parray[pw].flags.is_rated ? "*" : " ",
                bcaps,
                TICS2SECS(garray[g0].white.ticksleft),
                garray[g0].white.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);
    sprintf(statZ2, "%s %3.3s%s %d %d %d T %.1f %d\n",
                parray[pb].pname,
                parray[pb].srank,
                parray[pb].flags.is_rated ? "*" : " ",
                wcaps,
                TICS2SECS(garray[g0].black.ticksleft),
                garray[g0].black.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);

    if (parray[pb].flags.is_client) {
      pcn_out(pb, CODE_NONE, FORMAT_n);
      pcn_out(pb, CODE_STATUS, FORMAT_s, statZ1);
      pcn_out(pb, CODE_STATUS, FORMAT_s, statZ2);

      until = garray[g0].minkg->height;
      for(yy = 0; yy < until; yy++) {
        pcn_out(pb, CODE_STATUS, FORMAT_d_sn, yy, statstring[yy]);
      }
    }
    if (parray[pw].flags.is_client) {
      pcn_out(pw, CODE_NONE, FORMAT_n);
      pcn_out(pw, CODE_STATUS, FORMAT_s, statZ1);
      pcn_out(pw, CODE_STATUS, FORMAT_s, statZ2);

      until = garray[g0].minkg->height;
      for(yy = 0; yy < until; yy++) {
        pcn_out(pw, CODE_STATUS, FORMAT_d_sn, yy, statstring[yy]);
      }
    }
    pcn_out(pw, CODE_SCORE_M, FORMAT_s_W_O_f_TO_s_B_fn,
                          parray[pw].pname,
                          wscore,
                          parray[pb].pname,
                          bscore);

    pcn_out(pb, CODE_SCORE_M, FORMAT_s_W_O_f_TO_s_B_fn,
                          parray[pw].pname,
                          wscore,
                          parray[pb].pname,
                          bscore);

    pcn_out(pw, CODE_INFO, FORMAT_s_HAS_RESIGNED_THE_GAME_n,
           (winner == pw) ? \
           parray[pb].pname : parray[pw].pname);

    pcn_out(pb, CODE_INFO, FORMAT_s_HAS_RESIGNED_THE_GAME_n,
           (winner == pw) ? \
           parray[pb].pname : parray[pw].pname);

    sprintf(tmp, " %s resigns. W %.1f B %.1f}\n",
            (winner == pw) ? "Black" : "White",
            wscore, bscore);

    rate_change = 1;
    for (p = 0; p < parray_top; p++) {
      if (!parray[p].slotstat.is_online) continue;
      if (!parray[p].flags.want_gshouts && !player_is_observe(p, g0)) continue;
/*      pcn_out_prompt(p, CODE_SHOUT, "%s%s", outstr, tmp); */
      if (player_is_observe(p, g0)) player_remove_observe(p, g0); 
      /*pcn_out_prompt(p, CODE_INFO, "%s%s", outstr, tmp);*/
      if (parray[p].flags.is_client) {
        pcn_out(p, CODE_CR1|CODE_STATUS, FORMAT_s, statZ1);
        pcn_out(p, CODE_STATUS, FORMAT_s, statZ2);
        until = garray[g0].minkg->height;
        for(yy = 0; yy < until; yy++) {
          pcn_out(p, CODE_STATUS, FORMAT_d_sn, yy, statstring[yy]);
        }
        pcn_out_prompt(p, CODE_STATUS, FORMAT_ss, outstr, tmp);
      }
    }
    garray[g0].winner = winner;
    game_delete(pw, pb);
    break;

  case END_RESIGN:
    completed_games++;
    sprintf(tmp, " %s resigns.}\n",
            (winner == pw) ? "Black" : "White");
    rate_change = 1;
    garray[g0].winner = winner;
    garray[g0].gresult = 0.0;
    game_delete(pw, pb);

    pcn_out(pw, CODE_INFO, FORMAT_s_HAS_RESIGNED_THE_GAME_n,
           (winner == pw) ? \
           parray[pb].pname : parray[pw].pname);

    pcn_out(pb, CODE_INFO, FORMAT_s_HAS_RESIGNED_THE_GAME_n,
           (winner == pw) ? \
           parray[pb].pname : parray[pw].pname);

    break;

  case END_FLAG:
    completed_games++;
    sprintf(tmp, " %s forfeits on time.}\n",
            (winner == pw) ? "Black" : "White");
    rate_change = 1;
    garray[g0].winner = winner;
    garray[g0].gresult = -1.0;
    game_delete(pw, pb);
    break;

  case END_ADJOURN:
  case END_LOSTCONNECTION:
    sprintf(tmp, " has adjourned.}\n");
    rate_change = 0;
    game_save(g0);
    pcn_out(pb, CODE_INFO, FORMAT_GAME_HAS_BEEN_ADJOURNED_n);
    pcn_out(pb, CODE_INFO, FORMAT_GAME_d_s_VS_s_HAS_ADJOURNED_n,
          g0 + 1,
          parray[pw].pname,
          parray[pb].pname);

    if (garray[g0].teach != 1) {
      pcn_out(pw, CODE_INFO, FORMAT_GAME_HAS_BEEN_ADJOURNED_n );
      pcn_out(pw, CODE_INFO, FORMAT_GAME_d_s_VS_s_HAS_ADJOURNED_n,
          g0 + 1,
          parray[pw].pname,
          parray[pb].pname);
    }
    break;

  default:
    sprintf(tmp, " Hmm, the game ended and I don't know why(%d).} *\n", why);
    break;
  }
  strcat(outstr, tmp);
  
  for (p = 0; p < parray_top; p++) {
    if (!parray[p].slotstat.is_online) continue;
    if (parray[p].flags.want_gshouts || player_is_observe(p, g0))
    pcn_out_prompt(p, CODE_SHOUT, FORMAT_s, outstr);
    if (player_is_observe(p, g0)
      || p == pb || p == pw) {
      pcn_out_prompt(p, CODE_INFO, FORMAT_s, outstr);
    }
      player_remove_observe(p, g0); 
  }

  if (winner == pb && rate_change) {
    player_resort();
    if (garray[g0].Ladder9 == 1 || garray[g0].Ladder19 == 1) {
      if (garray[g0].Ladder9 == 1) {
        Ladder_W = ladder_player_named(Ladder9, parray[pw].pname);
        Ladder_B = ladder_player_named(Ladder9, parray[pb].pname);
        ladder_rotate(Ladder9, Ladder_W->idx, Ladder_B->idx);
        ladder_set_time(Ladder9, Ladder_W->idx, now);
        ladder_set_time(Ladder9, Ladder_B->idx, now);
        ladder_win(Ladder9, Ladder_B->idx);
        ladder_lose(Ladder9, Ladder_W->idx);
        fp = xyfopen(FILENAME_LADDER9, "w");
        if (!fp) {
          pcn_out(p, CODE_ERROR,FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        } else {
          num_9 = ladder_save(fp, Ladder9);
          fclose(fp);
          Ladder_W = ladder_player_named(Ladder9, parray[pw].pname);
          Ladder_B = ladder_player_named(Ladder9, parray[pb].pname);
          pcn_out(pb, CODE_INFO, FORMAT_YOU_ARE_NOW_AT_POSITION_d_IN_THE_9X9_LADDER_CONGRATS_n, (Ladder_B->idx) + 1);
          pcn_out(pw, CODE_INFO, FORMAT_YOU_ARE_NOW_AT_POSITION_d_IN_THE_9X9_LADDER_n, (Ladder_W->idx) + 1);
          for (p2 = 0; p2 < parray_top; p2++) {
            if (!parray[p2].slotstat.is_online) continue;
            if (!parray[p2].i_lshout) continue;
            pcn_out_prompt(p2, CODE_SHOUT,FORMAT_LADDER9_RESULT_s_TAKES_POSITION_d_FROM_s_n,
                  parray[pb].pname,
                  (Ladder_B->idx) + 1,
                  parray[pw].pname,
                  (Ladder_W->idx) + 1);
          }
        }
      }
      if (garray[g0].Ladder19 == 1) {
        Ladder_W = ladder_player_named(Ladder19, parray[pw].pname);
        Ladder_B = ladder_player_named(Ladder19, parray[pb].pname);
        ladder_rotate(Ladder19, Ladder_W->idx, Ladder_B->idx);
        ladder_set_time(Ladder19, Ladder_W->idx, now);
        ladder_set_time(Ladder19, Ladder_B->idx, now);
        ladder_win(Ladder19, Ladder_B->idx);
        ladder_lose(Ladder19, Ladder_W->idx);
        fp = xyfopen(FILENAME_LADDER19, "w");
        if (!fp) {
          pcn_out(p, CODE_ERROR,FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        } else {
          num_19 = ladder_save(fp, Ladder19);
          fclose(fp);
          Ladder_W = ladder_player_named(Ladder19, parray[pw].pname);
          Ladder_B = ladder_player_named(Ladder19, parray[pb].pname);
          pcn_out(pb, CODE_INFO, FORMAT_YOU_ARE_NOW_AT_POSITION_d_IN_THE_19X19_LADDER_CONGRATS_n, (Ladder_B->idx) + 1);
          pcn_out(pw, CODE_INFO, FORMAT_YOU_ARE_NOW_AT_POSITION_d_IN_THE_19X19_LADDER_n, (Ladder_W->idx) + 1);
          for (p2 = 0; p2 < parray_top; p2++) {
            if (!parray[p2].slotstat.is_online) continue;
            if (!parray[p2].i_lshout) continue;
            pcn_out_prompt(p2, CODE_SHOUT,FORMAT_LADDER19_RESULT_s_TAKES_POSITION_d_FROM_s_n,
                  parray[pb].pname,
                  (Ladder_B->idx) + 1,
                  parray[pw].pname,
                  (Ladder_W->idx) + 1);
          }
        }
      }
    }
  }
  else if (winner == pw && rate_change) {
    if (garray[g0].Ladder9 == 1 || garray[g0].Ladder19 == 1) {
      if (garray[g0].Ladder9 == 1) {
        Ladder_W = ladder_player_named(Ladder9, parray[pw].pname);
        Ladder_B = ladder_player_named(Ladder9, parray[pb].pname);
        ladder_set_time(Ladder9, Ladder_W->idx, now);
        ladder_set_time(Ladder9, Ladder_B->idx, now);
        ladder_win(Ladder9, Ladder_W->idx);
        ladder_lose(Ladder9, Ladder_B->idx);
        fp = xyfopen(FILENAME_LADDER9, "w");
        if (!fp) {
          pcn_out(p, CODE_ERROR,FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        } else {
          num_9 = ladder_save(fp, Ladder9);
          fclose(fp);
        }
      }
      if (garray[g0].Ladder19 == 1) {
        Ladder_W = ladder_player_named(Ladder19, parray[pw].pname);
        Ladder_B = ladder_player_named(Ladder19, parray[pb].pname);
        ladder_set_time(Ladder19, Ladder_W->idx, now);
        ladder_set_time(Ladder19, Ladder_B->idx, now);
        ladder_win(Ladder19, Ladder_W->idx);
        ladder_lose(Ladder19, Ladder_B->idx);
        fp = xyfopen(FILENAME_LADDER19, "w");
        if (!fp) {
          pcn_out(p, CODE_ERROR,FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        } else {
          num_19 = ladder_save(fp, Ladder19);
          fclose(fp);
        }
      }
    }
  }
  if (rate_change)
    game_write_complete(g0, why == END_DONE ? statstring : NULL);
  parray[pw].session.protostate = STAT_WAITING;
  parray[pb].session.protostate = STAT_WAITING;
  pprintf_prompt(pw, "\n");
  if (garray[g0].teach != 1) pprintf_prompt(pb, "\n");
  if (why == END_RESIGN || why == END_FLAG || why == END_DONE) {
    if (garray[g0].teach != 1) {
      /* parray[pb].cruft.lastColor = PLAYER_BLACK; */
      /* parray[pw].cruft.lastColor = PLAYER_WHITE; */
      parray[pb].gonum_black++;
      parray[pw].gonum_white++;
      if (garray[g0].winner == pw) {
        parray[pw].gowins++;
        parray[pw].water++;
        parray[pb].golose++;
      }
      else if (garray[g0].winner == pb) {
        parray[pw].golose++;
        parray[pb].gowins++;
        parray[pb].water++;
      }
    }
  }
  parray[pw].session.gnum = -1;
  parray[pb].session.gnum = -1;
  parray[pw].session.opponent = -1;
  parray[pb].session.opponent = -1;
  parray[pw].cruft.last_opponent = pb;
  parray[pb].cruft.last_opponent = pw;
  parray[pw].match_type = 0;
  parray[pb].match_type = 0;
  game_finish(g0);
}

#if WANT_PAIR
int paired(int g0)
{
  int g2;

  if (garray[g0].pairstate == NOTPAIRED) return 0;
  g2 = garray[g0].pairwith;
  if (garray[g2].pairstate == NOTPAIRED) return 0;
  if (garray[g2].pairwith != g0) return 0;
  if (!garray[g2].slotstat.is_playing) return 0;
  if (garray[g2].gstatus != GSTATUS_ACTIVE) return 0;
  return 1;
}
#endif

#if WANT_PAIR
void process_move(int p, char *command, int original)
#else
void process_move(int p, char *command)
#endif
{
  int g1=parray[p].session.gnum;
  int good;
  int gmove;
  int players_only = 0;
  int pb, pw;


  if (g1 < 0) {
    pcn_out_prompt(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_n);
    return;
  }

  player_decline_offers(p, -1, -1);

  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;

  if (!garray[g1].teach
     && parray[p].session.side != garray[g1].onMove
     && parray[p].session.protostate != STAT_SCORING) {
    pcn_out_prompt(p, CODE_ERROR, FORMAT_IT_IS_NOT_YOUR_MOVE_n);
    return;
  }

  if (garray[g1].clockStopped) {
    pcn_out_prompt(p, CODE_INFO, FORMAT_GAME_CLOCK_IS_PAUSED_USE_qUNPAUSEq_TO_RESUME_n);
    return;
  }

#if WANT_PAIR
/* pair1 = (W1,B1), pair2 = (W2,B2)
   movenum % 4      0   1   2   3   ( == movenum & 3)
   player to move   B1  W1  B2  W2
*/
  if (paired(g1) && original
      && parray[p].session.protostate != STAT_SCORING &&
      ((mink_movenum(garray[g1].minkg) & 3) / 2) != (garray[g1].pairstate == PAIR2)) {
    pcn_out_prompt(p, CODE_ERROR, FORMAT_IT_IS_YOUR_PARTNER_S_MOVE_n);
    return;
  }
#endif


  /* test if we are removing dead stones */
  if (parray[p].session.protostate == STAT_SCORING) {
    if (!strcmp(command, "pass") ) {    /* User passes */
      pcn_out(p, CODE_ERROR, FORMAT_PASS_IS_NOT_VALID_DURING_SCORING_n);
      assert(parray[pw].session.protostate == STAT_SCORING);
      assert(parray[pb].session.protostate == STAT_SCORING);
      return;
    }

    /* Remove all "done"'s, still removing stones..... */
    player_remove_requests(parray[p].session.opponent, p, PEND_DONE);
    player_remove_requests(p, parray[p].session.opponent, PEND_DONE);
    player_decline_offers(p, -1, PEND_DONE);
    player_decline_offers(parray[p].session.opponent, -1, PEND_DONE);
    player_withdraw_offers(p, -1, PEND_DONE);
    player_withdraw_offers(parray[p].session.opponent, -1, PEND_DONE);

/*    Logit("(Removing) g = %d, move = %d command = %s", g0, mink_is_valid_move(garray[g0].minkg, command), command); */
    good = mink_removedead(garray[g1].minkg, 
                      mink_is_valid_move(garray[g1].minkg, command),
                      p == pw ? MINK_WHITE : MINK_BLACK);
  /* We are scoring, and removed a stone */
    if (good) {
      players_only = 1;
      pcn_out_prompt(pw, CODE_INFO, FORMAT_REMOVING_sn, stoupper(command));
      pcn_out_prompt(pb, CODE_INFO, FORMAT_REMOVING_sn, stoupper(command));
    }
  }

  /* Play the move, test if valid */
  else {
    if (!strcmp(command, "pass")) {    /* User passes */
      /* pass is valid */
      garray[g1].num_pass = mink_pass(garray[g1].minkg); 
      /* Check if we need to start scoring.... */
      if (garray[g1].teach != 1 && garray[g1].num_pass >= 2) {
        parray[pw].session.protostate = STAT_SCORING;
        parray[pb].session.protostate = STAT_SCORING;
        pcn_out_prompt(pw, CODE_INFO, FORMAT_YOU_CAN_CHECK_YOUR_SCORE_WITH_THE_SCORE_COMMAND_TYPE_DONE_WHEN_FINISHED_n);
        pcn_out_prompt(pb, CODE_INFO, FORMAT_YOU_CAN_CHECK_YOUR_SCORE_WITH_THE_SCORE_COMMAND_TYPE_DONE_WHEN_FINISHED_n);
      }
    }
    else if (!mink_play(garray[g1].minkg, mink_is_valid_move(garray[g1].minkg, command),1)) { 
      pcn_out(p, CODE_ERROR, FORMAT_YOUR_MOVE_IS_NOT_VALID_n);
      pcn_out_prompt(p, CODE_ERROR, FORMAT_ILLEGAL_MOVE_n);
      return;
    }
    game_update_time(g1);
    if (!garray[g1].slotstat.is_playing) return;
    if (garray[g1].gstatus != GSTATUS_ACTIVE) return;
    garray[g1].lastMovetick = globclock.tick;
    if (garray[g1].onMove == PLAYER_WHITE) { 
      garray[g1].onMove = PLAYER_BLACK;
      if (garray[g1].white.byostones > 0) {
        if (--garray[g1].white.byostones == 0) {
          garray[g1].white.byostones = garray[g1].ts.byostones;
          garray[g1].white.ticksleft = garray[g1].ts.byoticks;
        }
      }
    } else {
      garray[g1].onMove = PLAYER_WHITE;
      if (garray[g1].black.byostones > 0) {
        if (--garray[g1].black.byostones == 0) {
          garray[g1].black.byostones = garray[g1].ts.byostones;
          garray[g1].black.ticksleft = garray[g1].ts.byoticks;
        }
      }
    }
  } 

  /* Check to see if we should be saving the game */
  gmove = mink_movenum(garray[g1].minkg);
  if (gmove && conffile.game_save_frequency 
	&& (gmove % conffile.game_save_frequency) == 0) game_save(g1);

  /* send out the boards to everyone.... */
  send_go_boards(g1, players_only);

#if WANT_PAIR
  if (paired(g1) && original) {
    int g2;
    g2 = garray[g1].pairwith ;
    process_move(garray[g1].onMove == PLAYER_WHITE ? pb: pw,
       stolower(command), 0);
  }
#endif
}

int com_title(int p, struct parameter* param)
{
  int g1=parray[p].session.gnum;
  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  free(garray[g1].gtitle);
  garray[g1].gtitle = mystrdup(param[0].val.string); 
  return COM_OK;
}

int com_event(int p, struct parameter* param)
{
  int g1=parray[p].session.gnum;
  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  free(garray[g1].gevent);
  garray[g1].gevent = mystrdup(param[0].val.string); 
  return COM_OK;
}

int com_resign(int p, struct parameter* param)
{
  int g1=parray[p].session.gnum;
  UNUSED(param);
  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  player_decline_offers(p, -1, -1);
  game_ended(g1, (garray[g1].white.pnum == p)
     ? garray[g1].black.pnum
     : garray[g1].white.pnum, END_RESIGN);
  return COM_OK;
}

int com_pause(int p, struct parameter* param)
{
  int g1=parray[p].session.gnum;
  struct pending *ptr;
  UNUSED(param);

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  g1 = parray[p].session.gnum;
  if (garray[g1].ts.time_type == TIMETYPE_UNTIMED) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_PAUSE_UNTIMED_GAMES_);
    return COM_OK;
  }
  if (garray[g1].clockStopped) {
    pcn_out(p, CODE_ERROR, FORMAT_GAME_IS_ALREADY_PAUSED_USE_qUNPAUSEq_TO_RESUME_);
    return COM_OK;
  }
  ptr=pending_find(parray[p].session.opponent, p, PEND_PAUSE);
  if (ptr) {
    player_remove_requests(parray[p].session.opponent, p, PEND_PAUSE);
    garray[g1].clockStopped = 1;
    /* Roll back the time */
    if (garray[g1].lastDectick < garray[g1].lastMovetick) {
      if (garray[g1].onMove == PLAYER_WHITE) {
	garray[g1].white.ticksleft += (garray[g1].lastDectick - garray[g1].lastMovetick);
      } else {
	garray[g1].black.ticksleft += (garray[g1].lastDectick - garray[g1].lastMovetick);
      }
    }
    garray[g1].lastMovetick = globclock.tick;
    garray[g1].lastDectick = globclock.tick;
    pcn_out_prompt(parray[p].session.opponent, CODE_INFO, FORMAT_s_ACCEPTED_PAUSE_GAME_CLOCK_PAUSED_n,
		   parray[p].pname);
    pcn_out(p, CODE_INFO, FORMAT_GAME_CLOCK_PAUSED_n);
  return COM_OKN;

  player_add_request(p, parray[p].session.opponent, PEND_PAUSE, 0);
  pcn_out_prompt(parray[p].session.opponent, CODE_INFO, FORMAT_s_REQUESTS_TO_PAUSE_THE_GAME_n,
		   parray[p].pname);
  pcn_out(p, CODE_INFO, FORMAT_PAUSE_REQUEST_SENT_n);
  }
  return COM_OKN;
}

int com_unpause(int p, struct parameter * param)
{
  int g1 = parray[p].session.gnum;
  UNUSED(param);

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (!garray[g1].clockStopped) {
    pcn_out(p, CODE_ERROR, FORMAT_GAME_IS_NOT_PAUSED_);
    return COM_OK;
  }
  garray[g1].clockStopped = 0;
  garray[g1].lastMovetick = globclock.tick;
  garray[g1].lastDectick = globclock.tick;
  pcn_out(p, CODE_INFO, FORMAT_GAME_CLOCK_RESUMED_);
  pcn_out_prompt(parray[p].session.opponent, CODE_INFO, FORMAT_GAME_CLOCK_RESUMED_n );
  return COM_OK;
}

int com_done(int p, struct parameter * param)
{
  int g1=parray[p].session.gnum;
  UNUSED(param);

  if (g1 < 0 || garray[g1].gotype < GAMETYPE_GO) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (garray[g1].num_pass < 2) {
    pcn_out(p, CODE_ERROR, FORMAT_CANNOT_TYPE_DONE_UNTIL_BOTH_SIDES_HAVE_PASSED_);
    return COM_OK;
  }
  if (pending_find(parray[p].session.opponent, p, PEND_DONE)) {
    player_remove_requests(parray[p].session.opponent, p, PEND_DONE);
    player_decline_offers(p, -1, -1);
    game_ended(g1, PLAYER_NEITHER, END_DONE);
    return COM_OK;
  }

  player_add_request(p, parray[p].session.opponent, PEND_DONE, 0);
  pcn_out_prompt(parray[p].session.opponent, CODE_INFO, FORMAT_empty);
  return COM_OK;
}

int com_adjourn(int p, struct parameter * param)
{
  int g1=parray[p].session.gnum;
  UNUSED(param);

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (garray[g1].teach == 1) {
    game_ended(g1, PLAYER_NEITHER, END_ADJOURN);
    return COM_OKN;
    }
  if (pending_find(parray[p].session.opponent, p, PEND_ADJOURN)) {
    player_remove_requests(parray[p].session.opponent, p, PEND_ADJOURN);
    player_decline_offers(p, -1, -1);
#if WANT_PAIR
    if (paired(g1)) {
      game_ended(garray[g1].pairwith, PLAYER_NEITHER, END_ADJOURN);
    }
#endif /* WANT_PAIR */
    game_ended(g1, PLAYER_NEITHER, END_ADJOURN);
  return COM_OKN;
  }
  player_add_request(p, parray[p].session.opponent, PEND_ADJOURN, 0);
  pcn_out(parray[p].session.opponent, CODE_INFO,
          FORMAT_s_WOULD_LIKE_TO_ADJOURN_THE_GAME_n, parray[p].pname);
  pcn_out_prompt(parray[p].session.opponent, CODE_INFO,
                 FORMAT_USE_ADJOURN_TO_ADJOURN_THE_GAME_n);
  pcn_out(p, CODE_INFO, FORMAT_REQUEST_FOR_ADJOURNMENT_SENT_n);
 
  return COM_OKN;
}

int com_pteach(int p, struct parameter * param)
{
  int g1=parray[p].session.gnum;
  int p2;
  UNUSED(param);

  g1 = parray[p].session.gnum;
  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  if (garray[g1].teach == 2) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THIS_IS_ALREADY_A_TEACHING_GAME_);
    return COM_OK;
  }
  p2 = parray[p].session.opponent;
 
  if (pending_find(p2, p, PEND_TEACH)) {
    player_remove_requests(p2, p, PEND_TEACH);
    player_decline_offers(p, -1, -1);

    pcn_out(p, CODE_INFO, FORMAT_THIS_IS_NOW_A_FREE_UNRATED_TEACHING_GAME_n);
    pcn_out(p2, CODE_INFO, FORMAT_THIS_IS_NOW_A_FREE_UNRATED_TEACHING_GAME_n);
    garray[g1].teach = 2;
    garray[g1].rated = 0;
    return COM_OK;
  }
 
  player_add_request(p, p2, PEND_TEACH, 0);
  return COM_OK;
}

int com_ladder(int p, struct parameter * param)
{
  int g1 = parray[p].session.gnum;
  int p2, pb, pw;
  int size;
  const struct ladderplayer *pl1, *pl2;
  UNUSED(param);

  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;
  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  if (garray[g1].Ladder_Possible == 0) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THIS_CANNOT_BE_A_LADDER_RATED_GAME_);
    return COM_OK;
  }
  p2 = parray[p].session.opponent;
  
  if (pending_find(parray[p].session.opponent, p, PEND_LADDER)) {
    player_remove_requests(parray[p].session.opponent, p, PEND_LADDER);
    player_decline_offers(p, -1, -1);

    size = garray[g1].minkg->width;

    if (size == 9) {
      pl1 = ladder_player_named(Ladder9, parray[pw].pname);
      pl2 = ladder_player_named(Ladder9, parray[pb].pname);
    } else {
      pl1 = ladder_player_named(Ladder19, parray[pw].pname);
      pl2 = ladder_player_named(Ladder19, parray[pb].pname);
    }

    if (!pl1 || !pl2) {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_MUST_FIRST_JOIN_THE_LADDER_WITH_qJOINq_);
      return COM_OK;
    }

    switch(size) {
      case 9:
      garray[g1].Ladder9 = 1; garray[g1].komi = conffile.default_komi9; break;
      default:
      case 19:
      garray[g1].Ladder19 = 1; garray[g1].komi = conffile.default_komi19; break;
      }
    pcn_out(p, CODE_INFO, FORMAT_THIS_IS_NOW_A_LADDER_RATED_GAME_n );
    pcn_out(p2, CODE_INFO, FORMAT_THIS_IS_NOW_A_LADDER_RATED_GAME_n);
    pcn_out_prompt(p2, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g1].komi);
    pcn_out(p, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g1].komi);
    for (p2 = 0; p2 < parray_top; p2++) {
      if (!parray[p2].slotstat.is_online) continue;
      if (!parray[p2].i_lshout) continue;
      pcn_out_prompt(p2, CODE_SHOUT,FORMAT_LADDERd_GAME_MATCH_d_s_d_VS_s_d_n, 
         size, g1 + 1,
         parray[pw].pname, pl1->idx + 1,
         parray[pb].pname, pl2->idx + 1);
    }
    return COM_OK;
  }

  player_add_request(p, parray[p].session.opponent, PEND_LADDER, 0);
  return COM_OK;
}

int com_komi(int p, struct parameter * param)
{
  int g1=parray[p].session.gnum;
  int p2;
  float newkomi = 0.5;		/* Init to shut up warnings. */
  struct pending * hers, *mine;
  int pendstat=0;

  switch(param[0].type) {
  case TYPE_NULL:
      return COM_OK;
  case TYPE_INT:
    newkomi = (float) param[0].val.integer;
    break;
  case TYPE_FLOAT:
    newkomi = param[0].val.f;
    break;
  }

  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  if (garray[g1].teach == 1) {
    garray[g1].komi = newkomi;
    pcn_out(p, CODE_INFO, FORMAT_SET_THE_KOMI_TO_f, newkomi);
    return COM_OK;
  }

  p2 = parray[p].session.opponent;
  
  mine = pending_find(p, p2, PEND_KOMI);
  hers = pending_find(p2, p, PEND_KOMI);
  if (mine) pendstat |= 1;
  else {
     mine = player_pending_new(p, p2, PEND_KOMI);
     if (!mine) { return COM_OK; }
     mine->float1 = newkomi;
  }
  if (hers) pendstat |= 2;

  if (hers && hers->float1 == mine->float1) {
    /* komi is agreed upon.  Alter game record */
    garray[g1].komi = newkomi;
    pcn_out(p, CODE_INFO, FORMAT_SET_THE_KOMI_TO_f, newkomi);
    pcn_out_prompt(p2, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, newkomi);
    for (p2 = 0; p2 < parray_top; p2++) {
      if (!parray[p2].slotstat.is_online) continue;
      if (player_is_observe(p2, g1)) {
        pcn_out(p2, CODE_INFO, FORMAT_KOMI_SET_TO_f_IN_MATCH_dn, newkomi, g1+1);
      }
    }
#if WANT_PAIR
    if (paired(g1)) {
      garray[garray[g1].pairwith].komi = newkomi;
    }
#endif
    player_pending_delete(mine);
    player_pending_delete(hers);
    return COM_OK;
  }
  switch(pendstat) {
  case 0:
    pcn_out(p, CODE_INFO, FORMAT_OFFERING_A_KOMI_OF_f_TO_s, newkomi, parray[p2].pname);
    pcn_out(p2, CODE_INFO, FORMAT_s_OFFERS_A_NEW_KOMI_OF_f_n, parray[p].pname,
            newkomi);
    break;
  case 1:
    pcn_out(p, CODE_INFO, FORMAT_UPDATING_KOMI_OFFER_TO_);
    pcn_out_prompt(p2, CODE_INFO, FORMAT_s_UPDATES_THE_KOMI_OFFER_n, parray[p].pname);
    pcn_out(p2, CODE_INFO, FORMAT_s_OFFERS_A_NEW_KOMI_OF_f_n,newkomi, parray[p].pname);
    break;
  case 2:
  case 3:
    pcn_out(p, CODE_INFO, FORMAT_DECLINING_KOMI_OFFER_FROM_s_AND_OFFERING_NEW_KOMI_n,
                parray[p2].pname);
    pcn_out_prompt(p2, CODE_INFO,FORMAT_s_DECLINES_YOUR_KOMI_OFFER_AND_OFFERS_A_NEW_KOMI_n,
		parray[p].pname);
    pcn_out(p2, CODE_INFO, FORMAT_s_OFFERS_A_NEW_KOMI_OF_f_n, parray[p].pname,newkomi);
    break;
  }


  pcn_out_prompt(p2, CODE_INFO, FORMAT_USE_qKOMI_fq_TO_ACCEPT_OR_qDECLINE_sq_TO_RESPOND_n, newkomi, parray[p].pname);
  return COM_OK;
}

int com_status(int p, struct parameter * param)
{
  int g1, p2, yy, pb, pw, wc, bc, until;
  twodstring statstring;

  until = 0;
  switch (param[0].type ) {
  case TYPE_NULL:
    if (parray[p].session.gnum >= 0) {
      g1 = parray[p].session.gnum;
    } else if (parray[p].session.num_observe > 0) {
      g1 = parray[p].session.observe_list[0];
    } else {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NEITHER_PLAYING_NOR_OBSERVING_A_GAME_);
      return COM_OK;
    }
    break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p2 = player_find_part_login(param[0].val.word);
    if (p2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
      return COM_OK;
    }
    g1 = parray[p2].session.gnum;
    break;
  case TYPE_INT:
  default:          /* Must be an integer */
    g1 = param[0].val.integer - 1;
    break;
  }

  if (param[1].type == TYPE_INT) {
     until = param[1].val.integer;
  }

  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    return COM_NOSUCHGAME;
  }

  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;
  if (garray[g1].Private && pb != p && pw != p) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_IS_A_PRIVATE_GAME_);
    return COM_OK;
  }

  mink_getcaps(garray[g1].minkg, &wc, &bc);
  mink_boardstatus(garray[g1].minkg, statstring);

  pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
		parray[pw].pname,
		parray[pw].srank,
		parray[pw].flags.is_rated ? "*" : " ",
		wc,
		TICS2SECS(garray[g1].white.ticksleft),
   		garray[g1].white.byostones,
		garray[g1].komi,
                garray[g1].minkg->handicap);
  pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
		parray[pb].pname,
		parray[pb].srank,
		parray[pb].flags.is_rated ? "*" : " ",
		bc,
		TICS2SECS(garray[g1].black.ticksleft),
   		garray[g1].black.byostones,
		garray[g1].komi,
                garray[g1].minkg->handicap);
  if (!until) until = garray[g1].minkg->height;
  if (until-1 > garray[g1].minkg->height) return COM_OKN;
  for(yy = 0; yy < until; yy++) {
    pcn_out(p, CODE_STATUS, FORMAT_d_sn, yy, statstring[yy]);
  }
  return COM_OKN;
}

int com_undo(int p, struct parameter * param)
{
  int gmove, pw, pb;
  int p2, g2;
  char mbuf[20];
  int num, x;
  int g1 = parray[p].session.gnum;

  if (param[0].type == TYPE_INT) {
    num = param[0].val.integer;
  }
  else num = 1;

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_ );
    return COM_OK;
  }
  player_decline_offers(p, -1, -1);

  if (parray[p].session.side != garray[g1].onMove
     && parray[p].session.protostate != STAT_SCORING
     && !garray[g1].teach) {
    pcn_out(p, CODE_ERROR, FORMAT_IT_IS_NOT_YOUR_MOVE_);
    return COM_OK;
  }

  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;
  gmove = mink_movenum(garray[g1].minkg);
#if WANT_PAIR
  g2 = garray[g1].pairwith;

/* pair1 = (W1,B1), pair2 = (W2,B2)
   movenum % 4      0   1   2   3   ( == movenum & 3)
   player to move   B1  W1  B2  W2
*/

  if (paired(g1) && gmove == mink_movenum(garray[g2].minkg)
      && ((gmove&3)/2) != (garray[g1].pairstate==PAIR2)) {
    pcn_out(p, CODE_ERROR, FORMAT_IT_IS_YOUR_PARTNER_S_MOVE_n);
    return COM_OK;
  }
#endif

  if (parray[p].session.protostate == STAT_SCORING) {
    parray[pw].session.protostate = STAT_PLAYING_GO;
    parray[pb].session.protostate = STAT_PLAYING_GO;
    garray[g1].num_pass = 1;
    mink_replay(garray[g1].minkg);
  }
  player_remove_requests(parray[p].session.opponent, p, PEND_DONE);
  
  for(x = 0; x < num; x++) {
    if (gmove == 0) {  /* At beginning of game. */
      garray[g1].onMove = PLAYER_BLACK; 
      return COM_OK; 
    }
    if (!garray[g1].teach && x == 0) x = num;
    mink_listmove(garray[g1].minkg, gmove, mbuf);
    pcn_out(pb, CODE_UNDO, FORMAT_s_UNDID_THE_LAST_MOVE_s_n,
              parray[p].pname,
              mbuf + 1);
    if (garray[g1].teach != 1) 
      pcn_out(pw, CODE_UNDO, FORMAT_s_UNDID_THE_LAST_MOVE_s_n,
              parray[p].pname,
              mbuf + 1);
    for (p2 = 0; p2 < parray_top; p2++) {
      if (!parray[p2].slotstat.is_online) continue;
      if (player_is_observe(p2, g1)) {
        pcn_out_prompt(p2, CODE_UNDO, FORMAT_UNDO_IN_GAME_d_s_VS_s_sn,
            g1 + 1,
            parray[pw].pname,
            parray[pb].pname,
            mbuf + 1);
      }
    }
    mink_back(garray[g1].minkg);
    garray[g1].lastMovetick = globclock.tick;
    garray[g1].onMove = PLAYER_BLACK+PLAYER_WHITE - garray[g1].onMove;
    send_go_boards(g1, 0);
    gmove = mink_movenum(garray[g1].minkg);
    if (gmove == 0) {
      garray[g1].minkg->handicap = 0;
    }
  }
#if WANT_PAIR
  if (paired(g1) && gmove == mink_movenum(garray[g2].minkg)) {
    com_undo(garray[g1].onMove == PLAYER_WHITE
       ? garray[g2].black.pnum : garray[g2].white.pnum,
                 param);
      Logit("DUPLICATING undo");
  }
#endif
  return COM_OKN;
}

int com_games(int p, struct parameter * param)
{
  int g0;
  int pw, pb;

  int selected = 0;  int count = 0;  int totalcount = 0;
  char *s = NULL;  int slen = 0;
  int mnum = 0;

  if (param[0].type == TYPE_WORD) {
    s = param[0].val.word;
    slen = strlen(s);
    selected = atoi(s);
    if (selected < 0) selected = 0;
  }

  pcn_out(p, CODE_GAMES, FORMAT_WHITE_NAME_RK_BLACK_NAME_RK_MOVE_SIZE_H_KOMI_BY_FR_);
  for (g0 = 0; g0 < garray_top; g0++) {
    if (!garray[g0].slotstat.is_playing || garray[g0].gstatus != GSTATUS_ACTIVE) continue;
    totalcount++;
    if (selected && selected != g0+1) continue;  /* not selected game number */
    pb = garray[g0].black.pnum;
    pw = garray[g0].white.pnum;
    if (!selected && s 
          && strncmp(s, parray[pw].login, slen)
          && strncmp(s, parray[pb].login, slen))
      continue;  /* player names did not match */
    count++;
    mnum = mink_movenum(garray[g0].minkg);
    pcn_out(p, CODE_CR1|CODE_GAMES, FORMAT_d_s_ss_VS_s_ss_d_d_d_f_d_cc_d_,
            g0 + 1,
	    parray[pw].pname,
            parray[pw].srank,
            parray[pw].flags.is_rated ? "*" : " ",
	    parray[pb].pname,
            parray[pb].srank,
            parray[pb].flags.is_rated ? "*" : " ",
	    mnum,
            garray[g0].minkg->width,
            garray[g0].minkg->handicap,
            garray[g0].komi,
            TICS2SECS(garray[g0].ts.byoticks) / 60,
	    (garray[g0].rated) ? ' ' : garray[g0].teach ? 'T' : 'F',
/*	    (garray[g0].gotype) ? 'I' : '*', */
	    (garray[g0].rules == RULES_NET) ? (parray[pw].match_type == GAMETYPE_TNETGO ? '*' : 'I') : 'G',
            game_get_num_ob(g0));
  }
  return COM_OK;
}

int com_gomoves(int p, struct parameter * param)
{
  int ii, p2, pb, pw;
  int wc, bc;

  int g1 = 0;  int count = 0;

  char mbuf[20], outbuff[1024];

  switch (param[0].type) {
  case TYPE_NULL:
    if (parray[p].session.gnum >= 0) {
      g1 = parray[p].session.gnum;
    } else {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NEITHER_PLAYING_NOR_OBSERVING_A_GAME_);
      return COM_OK;
    }
    break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p2 = player_find_part_login(param[0].val.word);
    if (p2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, 
                   param[0].val.word);
      return COM_OK;
    }
    g1 = parray[p2].session.gnum;
    break;
  case TYPE_INT:
  default:    /* Must be an integer */
    g1 = param[0].val.integer - 1;
    break;
  }
  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    return COM_NOSUCHGAME;
  }
  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;
  if (garray[g1].Private && pb != p && pw != p) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_IS_A_PRIVATE_GAME_);
    return COM_OK;
  }

  mink_getcaps(garray[g1].minkg, &wc, &bc);
  sprintf(outbuff, "Game %d %s: %s (%d %d %d) vs %s (%d %d %d)",
        g1 + 1, "I",
        parray[pw].pname, bc,
        TICS2SECS(garray[g1].white.ticksleft), garray[g1].white.byostones,

        parray[pb].pname, wc,
        TICS2SECS(garray[g1].black.ticksleft), garray[g1].black.byostones);

  count = mink_movenum(garray[g1].minkg);
  if (count == 0) {
    pcn_out_prompt(p, CODE_MOVE, FORMAT_sn, outbuff);  
    return COM_OKN;
  }
  pcn_out(p, CODE_MOVE, FORMAT_sn, outbuff);  
  for(ii = 0; ii < count; ii++) {
    mink_listmove(garray[g1].minkg, ii + 1, mbuf);
    pcn_out(p, CODE_MOVE, FORMAT_d_c_sn, ii, mbuf[0], mbuf + 1);
  }
  return COM_OKN;
}

static int do_observe(int p, int obgame)
{
  int wc, bc, g1, pb, pw;
  int gmove = 0;
  char buf[200];

  if (garray[obgame].Private && parray[p].adminLevel < ADMIN_ADMIN) {
    pcn_out(p, CODE_INFO, FORMAT_SORRY_GAME_d_IS_A_PRIVATE_GAME_, 
                obgame + 1);
    return COM_OK;
  }
  pb = garray[obgame].black.pnum;
  pw = garray[obgame].white.pnum;
  if (pb == p || pw == p) {
    pcn_out(p, CODE_INFO, FORMAT_YOU_CANNOT_OBSERVE_A_GAME_THAT_YOU_ARE_PLAYING_);
    return COM_OK;
  }
  if (player_is_observe(p, obgame)) {
    pcn_out(p, CODE_INFO, FORMAT_REMOVING_GAME_d_FROM_OBSERVATION_LIST_n, 
                obgame + 1);
    player_remove_observe(p, obgame);
  } else {
    if (!player_add_observe(p, obgame)) {
      pcn_out(p, CODE_INFO, FORMAT_ADDING_GAME_TO_OBSERVATION_LIST_n, 
                  obgame + 1);
      g1 = obgame;
      sprintf(buf, " %s started observation.", parray[p].pname);
      add_kib(&garray[g1], mink_movenum(garray[g1].minkg), buf);
      if (parray[p].flags.is_client) {
        mink_getcaps(garray[g1].minkg, &wc, &bc);
        gmove = mink_movenum(garray[obgame].minkg);
        pcn_out(p, CODE_MOVE, FORMAT_GAME_d_s_s_d_d_d_VS_s_d_d_d_n,
                    g1 + 1, "I",
                    parray[pw].pname, bc,
                    TICS2SECS(garray[g1].white.ticksleft), garray[g1].white.byostones,

                    parray[pb].pname, wc,
                    TICS2SECS(garray[g1].black.ticksleft),
                    garray[g1].black.byostones);

        if (gmove) {
          mink_listmove(garray[g1].minkg, gmove, buf);
          pcn_out(p, CODE_MOVE, FORMAT_d_c_s, 
                      gmove - 1, buf[0], buf + 1);
        }
      }
      else if (parray[p].i_verbose) {
        pcommand (p, "refresh %d", g1 + 1);
      }
      parray[p].session.protostate = STAT_OBSERVING;
    } else {
      pcn_out(p, CODE_INFO, FORMAT_YOU_ARE_ALREADY_OBSERVING_THE_MAXIMUM_NUMBER_OF_GAMES_n);
    }
  }
  return COM_OKN;
}

int com_observe(int p, struct parameter * param)
{
  int i;
  int p2 = -1, gx;

  switch (param[0].type) {
  case TYPE_NULL:
    for (i = 0; i < parray[p].session.num_observe; i++) {
      pcn_out(p, CODE_INFO, FORMAT_REMOVING_GAME_d_FROM_OBSERVATION_LIST_, 
                parray[p].session.observe_list[i] + 1);
    }
    parray[p].session.num_observe = 0;
    parray[p].session.protostate = STAT_WAITING;
    return COM_OK;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p2 = player_find_part_login(param[0].val.word);
    if (p2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, 
              param[0].val.word);
      return COM_OK;
    }
    gx = parray[p2].session.gnum;
    break;
  case TYPE_INT:
  default: /* Must be an integer */
    gx = param[0].val.integer - 1;
    break;
  }

  if (gx < 0 || gx >= garray_top || !garray[gx].slotstat.is_playing || garray[gx].gstatus != GSTATUS_ACTIVE) {
    return COM_NOSUCHGAME;
  }
  if (garray[gx].white.pnum == p || garray[gx].black.pnum == p) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CANNOT_OBSERVE_A_GAME_THAT_YOU_ARE_PLAYING_);
    return COM_OK;
  }
  do_observe(p, gx);
  return COM_OK;
}

#if WANT_PAIR 
int com_pair(int p, struct parameter * param)
{
  int gmove, p2;
  int ourgame, theirgame;
  int btim, ttim;

  theirgame = param[0].val.integer - 1;
  ourgame = parray[p].session.gnum;

  if (ourgame < 0 || ourgame >= garray_top || !garray[ourgame].slotstat.is_playing || garray[ourgame].gstatus != GSTATUS_ACTIVE) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (garray[ourgame].pairstate != NOTPAIRED) {
    pcn_out(p, CODE_ERROR, FORMAT_YOUR_GAME_IS_PAIRED_ALREADY_);
    return COM_OK;
  }
  if (theirgame < 0 || theirgame >= garray_top || !garray[theirgame].slotstat.is_playing || garray[theirgame].gstatus != GSTATUS_ACTIVE) {
    pcn_out(p, CODE_ERROR, FORMAT_NO_SUCH_GAME_);
    return COM_OK;
  }

  p2 = garray[theirgame].white.pnum;

  if (p != garray[ourgame].white.pnum) {
    pcn_out(p, CODE_ERROR, FORMAT_ONLY_THE_WHITE_PLAYER_MAY_REQUEST_A_PAIR_MATCH);
    return COM_OK;
  }
  
  if (p == garray[ourgame].black.pnum
    || p == garray[theirgame].white.pnum
    || p == garray[theirgame].black.pnum) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_ONE_OF_THE_OTHER_PLAYERS_CANNOT_PAIR_);
    return COM_OK;
  }

  gmove = mink_movenum(garray[ourgame].minkg) 
	- mink_movenum(garray[theirgame].minkg);
  if (gmove != 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_MUST_PAIR_BEFORE_YOUR_FIRST_MOVE_OR_AT_EQUAL_MOVES_IN_A_MATCH);
    return COM_OK;
  }

  if (garray[theirgame].minkg->width != garray[ourgame].minkg->width) {
    pcn_out(p, CODE_ERROR, FORMAT_THE_BOARDS_ARE_NOT_THE_SAME_SIZE_);
    return COM_OK;
  }
   
  /* Test if other side already requested pair with us */

  if (pending_find(p2, p, PEND_PAIR)) {
    player_remove_requests(p2, p, PEND_PAIR);
    player_decline_offers(p, -1, -1);
    
    /* Ok, in here we have a match for the pairs */

    /* First, get all the times the same */

    ttim = (garray[theirgame].white.ticksleft > garray[ourgame].white.ticksleft) ? 
            garray[theirgame].white.ticksleft : garray[ourgame].white.ticksleft;
    btim = (garray[theirgame].ts.byoticks > garray[ourgame].ts.byoticks)     ? 
            garray[theirgame].ts.byoticks : garray[ourgame].ts.byoticks;

    garray[theirgame].white.ticksleft = ttim;
    garray[ourgame].white.ticksleft   = ttim;
    garray[theirgame].black.ticksleft = ttim;
    garray[ourgame].black.ticksleft   = ttim;
    garray[theirgame].ts.byoticks   = btim;
    garray[ourgame].ts.byoticks     = btim;

    /* Match the games up in a pair fashion */

    garray[theirgame].pairstate = PAIR1;
    garray[ourgame].pairstate = PAIR2;
    garray[theirgame].pairwith = ourgame;
    garray[ourgame].pairwith = theirgame;

    /* Remove ladder problems (for now) */

    garray[theirgame].Ladder_Possible = 0; 
    garray[theirgame].Ladder9 = 0;
    garray[theirgame].Ladder19 = 0;
    garray[ourgame].Ladder_Possible = 0; 
    garray[ourgame].Ladder9 = 0;
    garray[ourgame].Ladder19 = 0;

    /* Make the game unrated by default */
    garray[theirgame].rated = 0;
    garray[ourgame].rated = 0;

    /* Tell the players */

    pcn_out_prompt(garray[theirgame].white.pnum, CODE_INFO, FORMAT_GAMES_d_AND_d_ARE_NOW_PAIRED_n,
      ourgame+1,theirgame+1);
    pcn_out_prompt(garray[theirgame].black.pnum, CODE_INFO, FORMAT_GAMES_d_AND_d_ARE_NOW_PAIRED_n,
      ourgame+1,theirgame+1);
    pcn_out(garray[ourgame].white.pnum, CODE_INFO, FORMAT_GAMES_d_AND_d_ARE_NOW_PAIRED_n,
      ourgame+1,theirgame+1);
    pcn_out_prompt(garray[ourgame].black.pnum, CODE_INFO, FORMAT_GAMES_d_AND_d_ARE_NOW_PAIRED_n,
      ourgame+1,theirgame+1);

    return COM_OK;
  }

    /* Send pair request to other game White player */

  player_add_request(p, p2, PEND_PAIR, ourgame);
  pcn_out(p2, CODE_INFO, FORMAT_s_AND_s_WOULD_LIKE_TO_PAIR_THEIR_GAME_WITH_YOURS_n,
          parray[p].pname, parray[parray[p].session.opponent].pname);
  pcn_out_prompt(p2, CODE_INFO, FORMAT_TO_DO_SO_PLEASE_TYPE_qPAIR_dqn,
                 ourgame + 1);
  return COM_OK;
}
#endif /* WANT_PAIR */

int com_ginfo(int p, struct parameter * param)
{
  int g0;

  if (param[0].type == TYPE_NULL) {
    if (parray[p].session.gnum >= 0) g0 = parray[p].session.gnum;
    else if (parray[p].session.num_observe > 0) g0 = parray[p].session.observe_list[0];
    else return COM_BADPARAMETERS;
  } else {			/* Must be an integer */
    g0 = param[0].val.integer - 1;
  }
  if (g0 >= garray_top) return COM_OK;
  if (g0 <= 0) g0 = 0;
  pcn_out(p, CODE_INFO, FORMAT_GAME_INFO_sn, 
            IFNULL(garray[g0].gtitle, "No info available") );
  pcn_out(p, CODE_INFO, FORMAT_EVENT_INFO_sn,
            IFNULL(garray[g0].gevent,"none") );
  return COM_OK;
}

int com_allob(int p, struct parameter * param)
{
  int p2;
  int g0;
  int count;

  count = 0;
  switch (param[0].type) {
  case TYPE_NULL:
    if (parray[p].session.gnum >= 0) g0 = parray[p].session.gnum;
    else if (parray[p].session.num_observe > 0) g0 = parray[p].session.observe_list[0];
    else {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_OR_OBSERVING_A_GAME);
      return COM_OK;
    }
    break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p2 = player_find_part_login(param[0].val.word);
    if (p2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, 
                  param[0].val.word);
      return COM_OK;
    }
    g0 = parray[p2].session.gnum;
    break;
  case TYPE_INT:
  default: 		/* Must be an integer */
    g0 = param[0].val.integer - 1;
    break;
  }

  if (g0 < 0 || g0 >= garray_top || !garray[g0].slotstat.is_playing || garray[g0].gstatus != GSTATUS_ACTIVE) return COM_OK;
  
  pcn_out(p, CODE_INFO, FORMAT_OBSERVING_GAME_d_s_VS_s_,
                g0 + 1,
	        parray[garray[g0].white.pnum].pname,
	        parray[garray[g0].black.pnum].pname);
  for (p2 = 0; p2 < parray_top; p2++) {
    if (!parray[p2].slotstat.is_online) continue;
    if (!player_is_observe(p2, g0)) continue;
    if ((count % 3) == 0) pcn_out(p, CODE_CR1|CODE_INFO, FORMAT_empty); 
    pprintf(p, "%15s %3.3s%s ", 
                parray[p2].pname, 
                parray[p2].srank,
                parray[p2].flags.is_rated ? "*" : " ");
    count++;
  }
  if ((count % 3) != 0) 
    pcn_out(p, CODE_CR1|CODE_INFO, FORMAT_FOUND_d_OBSERVERS_, count);
  else
    pcn_out(p, CODE_INFO, FORMAT_FOUND_d_OBSERVERS_, count);
  return COM_OK;
}

int com_moves(int p, struct parameter * param)
{
  int g0 = parray[p].session.gnum;
  int p2;

  switch (param[0].type) {
  case TYPE_NULL:
    if (g0 >= 0) {}
    else if (parray[p].session.num_observe > 0) {
      g0 = parray[p].session.observe_list[0];
    } else {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NEITHER_PLAYING_NOR_OBSERVING_A_GAME_);
      return COM_OK;
    }
  break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p2 = player_find_part_login(param[0].val.word);
    if (p2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, 
               param[0].val.word);
      return COM_OK;
    }
    g0 = parray[p2].session.gnum;
  break;
  case TYPE_INT:
  default: /* Must be an integer */
    g0 = param[0].val.integer - 1;
  }

  if (g0 < 0 || g0 >= garray_top || !garray[g0].slotstat.is_playing || garray[g0].gstatus != GSTATUS_ACTIVE) {
    return COM_NOSUCHGAME;
  }
  if (garray[g0].Private
    && garray[g0].white.pnum != p && garray[g0].black.pnum != p) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_IS_A_PRIVATE_GAME_);
    return COM_OK;
  }
  return COM_OK;
}

int com_touch(int p, struct parameter * param)
{
  int pw, pb;
  const char *bname, *wname;
  
  bname = file_bplayer(param[0].val.string);
  wname = file_wplayer(param[0].val.string);
  pw = player_fetch(wname);
  if (pw < 0) return COM_OK;
  pb = player_fetch(bname);
  if (pb < 0) {
    player_unfix(pw);
    return COM_OK;
    }
  
  if (pw != p && pb != p) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CANNOT_TOUCH_SOMEONE_ELSES_STORED_GAME_n);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }

  xytouch(FILENAME_GAMES_ws_s, wname, bname);
  /* these two names SHOULD refer to the same Inode
  xytouch(FILENAME_GAMES_bs_s, wname, bname); */
  pcn_out(p, CODE_INFO, FORMAT_THE_GAME_s_s_HAS_BEEN_TOUCHED_, wname, bname);
  player_unfix(pw);
  player_unfix(pb);
  return COM_OK;
}

#define LOAD_TIME_WARNING 30

int com_load(int p, struct parameter * param)
{
  int pw, pb, p2, px;
  int g0;
  int filesize;
  const struct ladderplayer *LadderPlayer;
  int bpos = -1, wpos = -1;
  struct pending *ptr;
  const char *bname, *wname;

  bname = file_bplayer(param[0].val.string);
  wname = file_wplayer(param[0].val.string);

  pb = player_find_part_login(bname);
  pw = player_find_part_login(wname);

  if (p == pb) p2 = pw;
  else if (p == pw) p2 = pb;
  else {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CANNOT_LOAD_SOMEONE_ELSE_S_GAME_TRY_qLOOKq_TO_SEE_THE_GAME_);
    return COM_OK;
  }

  if (pw < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_s_IS_NOT_LOGGED_IN_, 
               file_wplayer(param[0].val.string));
    return COM_OK;
  }
  if (parray[pw].session.gnum >= 0) {
    pcn_out(p, CODE_ERROR, FORMAT_s_IS_CURRENTLY_PLAYING_A_GAME_, 
                parray[pw].pname);
    return COM_OK;
  }
  if (pb < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_NO_PLAYER_NAMED_s_IS_LOGGED_IN_, 
            file_bplayer(param[0].val.string));
    return COM_OK;
  }
  if (parray[pb].session.gnum >= 0) {
    pcn_out(p, CODE_ERROR, FORMAT_s_IS_CURRENTLY_PLAYING_A_GAME_, 
                    parray[pb].pname);
    return COM_OK;
  }

  if (!parray[p2].flags.is_open) {
      pcn_out(p, CODE_ERROR, FORMAT_YOUR_OPPONENT_IS_NOT_OPEN_FOR_MATCHES_n);
      pcn_out(p, CODE_ERROR, FORMAT_GAME_FAILED_TO_LOAD_);
      return COM_OK;
  }
  filesize= xystat(NULL, FILENAME_GAMES_ws_s,parray[pw].login,parray[pb].login);
  if (filesize < 0) {
    Logit("Failed stat %s", filename() );
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_sn, parray[pw].pname,parray[pb].pname);
    return COM_OK;
  }
  Logit("Successfull stat %s, size=%d", filename(), filesize );
  if (filesize == 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_A_SERIOUS_PROBLEM_WITH_YOUR_GAME_RECORD_THIS_IS_SOMETIMESn);
    pcn_out(p, CODE_ERROR, FORMAT_CAUSED_BY_AN_NNGS_CRASH_DURING_YOUR_GAME_);
    pcn_out(p, CODE_ERROR, FORMAT_WE_APOLOGIZE_BUT_THE_GAME_IS_LOST_);
    return COM_OK;
  }
  g0 = game_new(GAMETYPE_GO,19);
  if (game_read(&garray[g0], pw, pb) < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_sn, parray[pw].pname,parray[pb].pname);
    return COM_OK;
  }
  while((ptr=pending_find(pb, -1, PEND_MATCH))) {
    px=ptr->whoto;
    if (!parray[px].slotstat.is_online) continue;
    player_pending_delete(ptr);
    if (px != pw) {
      pcn_out_prompt(px, CODE_SHOUT, FORMAT_s_IS_CONTINUING_A_GAME_WITH_sn,
		     parray[pw].pname, parray[pb].pname);
    }
  }
  while((ptr=pending_find(pw, -1, PEND_MATCH))) {
    px=ptr->whoto;
    if (!parray[px].slotstat.is_online) continue;
    player_pending_delete(ptr);
    if (px != pb) {
        pcn_out_prompt(px, CODE_SHOUT, FORMAT_s_IS_CONTINUING_A_GAME_WITH_sn,
		     parray[pw].pname, parray[pb].pname);
    }
  }
  garray[g0].gstatus = GSTATUS_ACTIVE;
  garray[g0].slotstat.is_playing = 1;
  garray[g0].starttick = globclock.tick;
  garray[g0].lastMovetick = garray[g0].starttick;
  garray[g0].lastDectick = garray[g0].starttick;

  pcn_out(p2, CODE_INFO, FORMAT_s_HAS_RESTORED_YOUR_OLD_GAME_n, 
          parray[p].pname);
  pcn_out(p2, CODE_INFO, FORMAT_s_HAS_RESTARTED_YOUR_GAME_n, 
          parray[p].pname);
  

  for (px = 0; px < parray_top; px++) {
    if (!parray[px].slotstat.is_online) continue;
    if (px == pw || px == pb) continue;
    if (!parray[px].flags.want_gshouts) continue;
    pcn_out_prompt(px, CODE_SHOUT, FORMAT_GAME_d_s_VS_s_MOVE_d_n, 
                   g0+1, parray[pw].pname, parray[pb].pname, 
                   mink_movenum(garray[g0].minkg));
  } 
  parray[pw].session.gnum = g0;
  parray[pw].session.opponent = pb;
  parray[pw].session.side = PLAYER_WHITE;
  parray[pb].session.gnum = g0;
  parray[pb].session.opponent = pw;
  parray[pb].session.side = PLAYER_BLACK;
  parray[pb].session.protostate = STAT_PLAYING_GO;
  parray[pw].session.protostate = STAT_PLAYING_GO;
  if (garray[g0].Ladder9 || garray[g0].Ladder19) {
    if (garray[g0].size == 19) {
      if ((LadderPlayer=ladder_player_named(Ladder19,parray[pb].pname)) ) {
        bpos = LadderPlayer->idx;
      }
      else bpos = -1;
      if ((LadderPlayer=ladder_player_named(Ladder19,parray[pw].pname)) ) {
        wpos = LadderPlayer->idx;
      }
      else wpos = -1;
    }
    else if (garray[g0].size == 9) {
      if ((LadderPlayer=ladder_player_named(Ladder9,parray[pb].pname)) ) {
        bpos = LadderPlayer->idx;
      }
      else bpos = -1;
      if ((LadderPlayer=ladder_player_named(Ladder9,parray[pw].pname)) ) {
        wpos = LadderPlayer->idx;
      }
      else wpos = -1;
    }
    if (wpos < 0 || bpos < 0 || wpos > bpos) {
      garray[g0].Ladder9 = 0;
      garray[g0].Ladder19 = 0;
    }
  }
  send_go_boards(g0, 0);

  /* [PEM]: Added some time checks and automatic pausing. */
  if (garray[g0].white.ticksleft < LOAD_TIME_WARNING &&
      (garray[g0].ts.byoticks == 0 || garray[g0].white.byoperiods))
  {
    if (garray[g0].black.ticksleft < LOAD_TIME_WARNING &&
	(garray[g0].ts.byoticks == 0 || garray[g0].black.byoperiods))
    {				/* both */
      pcn_out(pw, CODE_INFO, FORMAT_BOTH_PLAYERS_HAVE_LESS_THAN_d_SECONDS_LEFT_n,
	      LOAD_TIME_WARNING);
      pcn_out(pb, CODE_INFO, FORMAT_BOTH_PLAYERS_HAVE_LESS_THAN_d_SECONDS_LEFT_n,
	      LOAD_TIME_WARNING);
    }
    else
    {				/* white */
      pcn_out(pw, CODE_INFO, FORMAT_YOU_HAVE_ONLY_d_SECONDS_LEFT_n,
	      TICS2SECS(garray[g0].white.ticksleft));
      pcn_out(pb, CODE_INFO, FORMAT_WHITE_HAS_ONLY_d_SECONDS_LEFT_n,
	      TICS2SECS(garray[g0].white.ticksleft));
    }
    garray[g0].clockStopped = 1;
  }
  else if (TICS2SECS(garray[g0].black.ticksleft) < LOAD_TIME_WARNING &&
	   (garray[g0].ts.byoticks == 0 || garray[g0].black.byoperiods))
  {				/* black */
    pcn_out(pw, CODE_INFO, FORMAT_BLACK_HAS_ONLY_d_SECONDS_LEFT_n,
	    TICS2SECS(garray[g0].black.ticksleft));
    pcn_out(pb, CODE_INFO, FORMAT_YOU_HAVE_ONLY_d_SECONDS_LEFT_n,
	    TICS2SECS(garray[g0].black.ticksleft));
    garray[g0].clockStopped = 1;
  }
  if (garray[g0].clockStopped)
  {
    pcn_out(pw, CODE_INFO, FORMAT_GAME_CLOCK_PAUSED_USE_qUNPAUSEq_TO_RESUME_n);
    pcn_out(pb, CODE_INFO, FORMAT_GAME_CLOCK_PAUSED_USE_qUNPAUSEq_TO_RESUME_n);
  }

  return COM_OK;
}

int com_stored(int p, struct parameter * param)
{
  DIR *dirp;
  struct dirent *dp;
  int p1;
  int count;

  if (param[0].type == TYPE_WORD) {
    p1 = player_fetch(param[0].val.word);
    if (p1 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_n);
      pcn_out(p, CODE_INFO, FORMAT_FOUND_d_STORED_GAMES_n, 0);
      return COM_OK;
      }
  } else {
    p1 = p;
    player_fix(p1);
  }

  pcn_out(p, CODE_INFO, FORMAT_STORED_GAMES_FOR_s_n, parray[p1].pname);

  dirp = xyopendir(FILENAME_GAMES_c,parray[p1].login);
  if (!dirp) {
    pcn_out(p, CODE_CR1|CODE_INFO, FORMAT_FOUND_d_STORED_GAMES_n, 0);

    player_unfix(p1);
    return COM_OK;
  }
  for (count=0; (dp = readdir(dirp)); ) {
    if (!file_has_pname(dp->d_name, parray[p1].login)) continue;
    if (count % 3 == 0) {
      pcn_out(p, CODE_CR1|CODE_STORED, FORMAT_empty);
    }
    pprintf(p, " %25s", dp->d_name);
    count++;
  }

  closedir(dirp);
  if (count % 3) pprintf(p, "\n");
  pcn_out(p, CODE_INFO, FORMAT_FOUND_d_STORED_GAMES_n, count);

  player_unfix(p1);
  return COM_OK;
}


int com_sgf(int p, struct parameter * param)
{
  DIR *dirp;
  struct dirent *dp;
  char pname[sizeof parray[p].pname];
  int count;

  if (param[0].type == TYPE_WORD) {
    do_copy(pname, param[0].val.word, sizeof pname);
  } else {
    do_copy(pname, parray[p].pname, sizeof pname);
  }
    /* Always Lowercase name */
  stolower(pname);

  count = 0;
  dirp = xyopendir(FILENAME_CGAMES_c,pname);
  if (!dirp) {
    pcn_out(p, CODE_ERROR, FORMAT_PLAYER_s_HAS_NO_SGF_GAMES_, pname);
    return COM_OK;
  }
  pcn_out(p, CODE_INFO,FORMAT_COMPLETED_GAMES_FOR_s_n, pname);
  for (dp = readdir(dirp); dp ; dp = readdir(dirp)) {
    if (!(strstr(dp->d_name, pname))) continue;
    if (count % 2 == 0) pcn_out(p, CODE_INFO, FORMAT_empty);
    pprintf(p, "%30s", dp->d_name);
    if (count % 2) pprintf(p, "\n");
    count++;
  }

  closedir(dirp);
  if (count)  {
    if (count % 2) pprintf(p, "\n");
    pcn_out(p, CODE_INFO,FORMAT_FOUND_d_COMPLETED_GAMES_n, count);
  }

  return COM_OK;
}

int com_history(int p, struct parameter * param)
{
  int p1 ;
  FILE *fp = NULL;

  if (param[0].type == TYPE_WORD) {
    p1 = player_fetch(param[0].val.word);
    if (p1 < 0) {
      return COM_OK;
    }
  }
  else player_fix(p1=p);
  if (p1 != p) {
    if (!parray[p].slotstat.is_registered) {
      pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_);
      player_unfix(p1);
      return COM_OK;
    }
    fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "r", parray[p1].login);
  } else {
    fp = xyfopen(FILENAME_RESULTS, "r" );
  }
  if (fp) {
    pgames(p, fp);
  }
  player_unfix(p1);
  return COM_OKN;
}

int com_rhistory(int p, struct parameter * param)
{
  int p2 = p;
  FILE *fp = NULL;

  if (!parray[p].slotstat.is_registered) return COM_OK;
  if (param[0].type == TYPE_WORD) {
    p2 = player_fetch(param[0].val.word);
    if (p2 < 0) return COM_OK;
  } else {
    player_fix(p2=p);
  }
  if (parray[p2].slotstat.is_registered) {
    fp = xyfopen(FILENAME_PLAYER_cs_GAMES, "r", parray[p2].login);
    if (fp) {
      pgames(p, fp);
    }
  }
  player_unfix(p2);
  return COM_OKN;
}


int com_time(int p, struct parameter * param)
{
  int p1, g1, pb, pw;

  switch (param[0].type) {
  case TYPE_NULL:
    if (parray[p].session.gnum < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
      return COM_OK;
    }
    g1 = parray[p].session.gnum;
    break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    p1 = player_find_part_login(param[0].val.word);
    if (p1 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, 
                     param[0].val.word);
      return COM_OK;
    }
    g1 = parray[p1].session.gnum;
    break;
  case TYPE_INT:
    g1 = param[0].val.integer - 1;
    break;
  default:
    return COM_BADPARAMETERS;
  }

  if (g1 < 0 || g1 >= garray_top || !garray[g1].slotstat.is_playing || garray[g1].gstatus != GSTATUS_ACTIVE) {
    return COM_NOSUCHGAME;
  }

  game_update_time(g1);
  pb = garray[g1].black.pnum;
  pw = garray[g1].white.pnum;

  pcn_out(p, CODE_TIME, FORMAT_GAME_dn, g1 + 1);
  pcn_out(p, CODE_TIME, FORMAT_WHITE_s_d_dn,
	  parray[pw].pname,
	  TICS2SECS(garray[g1].white.ticksleft) / 60,
	  TICS2SECS((garray[g1].white.ticksleft) % 60));
  pcn_out(p, CODE_TIME, FORMAT_BLACK_s_d_d,
	  parray[pb].pname,
	  TICS2SECS(garray[g1].black.ticksleft) / 60,
	  TICS2SECS((garray[g1].black.ticksleft) % 60 ));
  return COM_OK;
}

/*
** [PEM]: Changed this to a non-toggling command. See com_unfree() below.
*/

int com_free(int p, struct parameter * param)
{
  int gmove;
  int g1 = parray[p].session.gnum;
  UNUSED(param);

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  gmove = mink_movenum(garray[g1].minkg);
  if (gmove > 1) {
    pcn_out(p, CODE_ERROR, FORMAT_ONLY_VALID_AS_YOUR_FIRST_MOVE_);
    return COM_OK;
  }
  if (garray[g1].rated == 0)
    pcn_out(p, CODE_INFO, FORMAT_GAME_IS_ALREADY_FREE_USE_qUNFREEq_TO_CHANGE_THIS_n);
  else
  {
    garray[g1].rated = 0;
    pcn_out(p, CODE_INFO, FORMAT_GAME_WILL_NOT_COUNT_TOWARD_RATINGS_n);
    pcn_out(parray[p].session.opponent, CODE_INFO, FORMAT_GAME_WILL_NOT_COUNT_TOWARD_RATINGS_n);
  }
  return COM_OK;
}
 
/*
** [PEM]: Added this after several incidents where confusion about a game's
**        status arised.
*/
int com_unfree(int p, struct parameter * param)
{
  int gmove;
  int g1 = parray[p].session.gnum;
  UNUSED(param);

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  gmove = mink_movenum(garray[g1].minkg);
  if (gmove > 1) {
    pcn_out(p, CODE_ERROR, FORMAT_ONLY_VALID_AS_YOUR_FIRST_MOVE_);
    return COM_OK;
  }
  if (garray[g1].rated == 1)
    pcn_out(p, CODE_INFO, FORMAT_GAME_ALREADY_COUNTS_TOWARD_RATINGS_n);
  else
  {
    garray[g1].rated = 1;
    pcn_out(p, CODE_INFO, FORMAT_GAME_WILL_COUNT_TOWARD_RATINGS_n);
    pcn_out(parray[p].session.opponent, CODE_INFO, FORMAT_GAME_WILL_COUNT_TOWARD_RATINGS_n);
  }
  return COM_OK;
}

#if NOUSED
int com_nocaps(int p, struct parameter * param)
{
  int gmove, nc, nocap;
  int g1 = parray[p].session.gnum;

  if (g1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }

  gmove = mink_movenum(garray[g1].minkg);
  if (gmove > 1) {
    pcn_out(p, CODE_ERROR, FORMAT_ONLY_VALID_AS_YOUR_FIRST_MOVE);
    return COM_OK;
  }
  nc = garray[g1].nocaps = !garray[g1].nocaps; /* Huh ? */
 

  if (handicap < 1) {
    pcn_out(p, CODE_ERROR, FORMAT_USE_HANDICAP_1_9_PLEASE_ );
    return COM_OK;
  }
  
  test = mink_sethcap(garray[g1].minkg, handicap);
  gmove = mink_movenum(garray[g1].minkg);

  garray[g1].onMove = PLAYER_WHITE;
  send_go_boards(g1, 0);
#if WANT_PAIR
  if (paired(g1) && !mink_movenum(garray[garray[g1].pairwith].minkg)) {
    com_handicap(garray[garray[g1].pairwith].black.pnum, param);
    Logit("DUPLICATING handicap");
  }
#endif
  /* [PEM]: Point out that we change komi when setting handicap. */
  if (garray[g1].komi != 0.5) {
    int p2 = parray[p].session.opponent;

    garray[g0].komi = 0.5;
    pcn_out(p, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g0].komi);
    pcn_out(p2, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g0].komi);
    for (p2 = 0; p2 < parray_top; p2++) {
      if (!parray[p2].slotstat.is_online) continue;
      if (player_is_observe(p2, g0))
	pcn_out(p2, CODE_INFO, FORMAT_KOMI_SET_TO_f_IN_MATCH_dn, garray[g0].komi, g+1);
    }
  }
  return COM_OK;
}
#endif /* NOUSED */


int com_handicap(int p, struct parameter * param)
{
  int gmove;
  int test, g0, handicap;

  handicap = param[0].val.integer;

  if (parray[p].session.gnum < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  g0 = parray[p].session.gnum;

  gmove = mink_movenum(garray[g0].minkg);
  if (gmove > 0) return COM_OK;
  
  if (handicap > 9) {
    pcn_out(p, CODE_ERROR, FORMAT_USE_HANDICAP_9_THEN_ASK_WHITE_TO_PASS_PLEASE_);
    return COM_OK;
  }

  if (handicap < 1) {
    pcn_out(p, CODE_ERROR, FORMAT_USE_HANDICAP_1_9_PLEASE_ );
    return COM_OK;
  }
  
  test = mink_sethcap(garray[g0].minkg, handicap);
  gmove = mink_movenum(garray[g0].minkg);

  garray[g0].onMove = PLAYER_WHITE;
  send_go_boards(g0, 0);
#if WANT_PAIR
  if (paired(g0) && !mink_movenum(garray[garray[g0].pairwith].minkg)) {
    com_handicap(garray[garray[g0].pairwith].black.pnum, param);
    Logit("DUPLICATING handicap");
  }
#endif
  /* [PEM]: Point out that we change komi when setting handicap. */
  if (garray[g0].komi != 0.5)
  {
    int p1 = parray[p].session.opponent;

    garray[g0].komi = 0.5;
    pcn_out(p, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g0].komi);
    pcn_out(p1, CODE_INFO, FORMAT_KOMI_IS_NOW_SET_TO_fn, garray[g0].komi);
    for (p1 = 0; p1 < parray_top; p1++)
    {
      if (!parray[p1].slotstat.is_online) continue;
      if (player_is_observe(p1, g0))
	pcn_out(p1, CODE_INFO, FORMAT_KOMI_SET_TO_f_IN_MATCH_dn, garray[g0].komi, g0+1);
    }
  }
  return COM_OK;
}


int com_save(int p, struct parameter * param)
{
  UNUSED(param);
  if (parray[p].session.gnum < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  
  game_save(parray[p].session.gnum);
  pcn_out(p, CODE_INFO, FORMAT_GAME_SAVED_);

  return COM_OK;
}


int com_moretime(int p, struct parameter * param)
{
  int g0,pb,pw,increment;
#if WANT_PAIR
  int g2,pb2,pw2;
#endif

  increment = param[0].val.integer;
  if (increment <= 0) {
    pcn_out(p, CODE_ERROR, FORMAT_ADDTIME_REQUIRES_AN_INTEGER_VALUE_GREATER_THAN_ZERO_);
    return COM_OK;
  }
  if (parray[p].session.gnum < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_A_GAME_);
    return COM_OK;
  }
  g0 = parray[p].session.gnum;
  pb= garray[g0].black.pnum;
  pw= garray[g0].white.pnum;
#if WANT_PAIR
  g2 = garray[g0].pairwith;
  pb2= garray[g2].black.pnum;
  pw2= garray[g2].white.pnum;
#endif
  if (increment > 60000) {
    pcn_out(p, CODE_ERROR, FORMAT_ADDTIME_HAS_A_MAXIMUM_LIMIT_OF_60000_MINUTES_);
    increment = 60000;
  }
  if (pw == p) {
    garray[g0].black.ticksleft += SECS2TICS(increment * 60);
#if WANT_PAIR
    if (paired(g0)) {
      garray[g2].black.ticksleft += SECS2TICS(increment * 60);
    }
#endif
  }
  if (pb == p) {
    garray[g0].white.ticksleft += SECS2TICS(increment * 60);
#if WANT_PAIR
    if (paired(g0)) {
      garray[g2].white.ticksleft += SECS2TICS(increment * 60);
    }
#endif
  }
  pcn_out(p, CODE_INFO, FORMAT_d_MINUTES_WERE_ADDED_TO_YOUR_OPPONENTS_CLOCK,
	    increment);
#if WANT_PAIR
  if (paired(g0)) {
    if (p == pb) {
      pcn_out_prompt(pb2, CODE_INFO, 
                     FORMAT_d_MINUTES_WERE_ADDED_TO_YOUR_OPPONENTS_CLOCKn,
                     increment);
    } else {
      pcn_out_prompt(pw2, CODE_INFO, 
                     FORMAT_d_MINUTES_WERE_ADDED_TO_YOUR_OPPONENTS_CLOCKn,
                     increment);
    }
  }
#endif /* WANT_PAIR */

  pcn_out_prompt(parray[p].session.opponent, CODE_INFO,
	   FORMAT_YOUR_OPPONENT_HAS_ADDED_d_MINUTES_TO_YOUR_CLOCK_n,
	   increment);

#if WANT_PAIR
  if (paired(g0)) {
    if (p == pb) {
      pcn_out_prompt(pw2, CODE_INFO, 
                     FORMAT_YOUR_OPPONENT_HAS_ADDED_d_MINUTES_TO_YOUR_CLOCK_n,
                     increment);
    } else {
      pcn_out_prompt(pb2, CODE_INFO, 
                     FORMAT_YOUR_OPPONENT_HAS_ADDED_d_MINUTES_TO_YOUR_CLOCK_n,
                     increment);
    }
  }
#endif /* WANT_PAIR */

  return COM_OK;
}

void game_update_time(int g0)
{
  unsigned now, jiffies;
  int pb,pw;
#if WANT_PAIR
  int g2,pb2,pw2;
#endif

  /* If players have "paused" the game */
  if (garray[g0].clockStopped) return;

  /* If players have time controls of 0 0 (untimed) */
  if (garray[g0].ts.time_type == TIMETYPE_UNTIMED) {
    garray[g0].ts.totalticks = SECS2TICS(480);
    garray[g0].black.ticksleft = SECS2TICS(480);
    garray[g0].white.ticksleft = SECS2TICS(480);
    garray[g0].black.byoperiods = 0;
    garray[g0].white.byoperiods = 0;
    return;
  }

  /* If a teaching game */
  if (garray[g0].teach) {
    garray[g0].black.ticksleft = SECS2TICS(600);
    garray[g0].white.ticksleft = SECS2TICS(600);
    garray[g0].black.byoperiods = 0;
    garray[g0].white.byoperiods = 0;
    return;
  }

  now = globclock.tick;
  jiffies = now - garray[g0].lastDectick;

  /* Courtesy to allow hcap setup, etc.... */
#if WANT_HANDICAP_COURTESY
  if (mink_movenum(garray[g0].minkg < 2) {
    garray[g0].lastDectick = garray[g0].lastMovetick = now;
    return;
    }
#endif /* WANT_HANDICAP_COURTESY */

  pb = garray[g0].black.pnum;
  pw = garray[g0].white.pnum;
#if WANT_PAIR
  g2 = garray[g0].pairwith;
  pb2 = garray[g2].black.pnum;
  pw2 = garray[g2].white.pnum;
#endif

  /* If players are scoring */
  if (parray[pw].session.protostate == STAT_SCORING
     || parray[pb].session.protostate == STAT_SCORING) {
    return;
  }
 
  /* Game over, ran out of time! */
  if (garray[g0].white.ticksleft < SECS2TICS(1)
      && garray[g0].white.byostones > 1) {
#if WANT_PAIR
    if (paired(g0)) {
      game_ended(g2, pb2, END_FLAG);
    }
#endif /* WANT_PAIR */
    game_ended(g0, pb, END_FLAG);
  }
  else if (garray[g0].black.ticksleft < SECS2TICS(1)
          && garray[g0].black.byostones > 1) {
#if WANT_PAIR
    if (paired(g0)) {
      game_ended(g2, pw2, END_FLAG);
    }
#endif /* WANT_PAIR */
    game_ended(g0, pw, END_FLAG);
  }
  if (garray[g0].onMove == PLAYER_WHITE) {
    garray[g0].white.ticksleft -= jiffies;
#if WANT_PAIR
    if (paired(g0)) {
      garray[g2].white.ticksleft = garray[g0].white.ticksleft;
    }
#endif /* WANT_PAIR */
    /* White's flag fell. If he has not entered byo-yomi yet,
    ** he will start his first b/y period.
    ** No stones will be used, but his time keeps ticking ...
    */
    if (garray[g0].white.ticksleft <= 0 && garray[g0].white.byoperiods == 0) {
      garray[g0].white.byoperiods = 1;
      garray[g0].white.byostones = garray[g0].ts.byostones;
      garray[g0].white.ticksleft = garray[g0].ts.byoticks;
      pcn_out(pw, CODE_INFO,
                FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
                parray[pw].pname);
      pcn_out_prompt(pw, CODE_INFO,
                FORMAT_YOU_HAVE_d_STONES_AND_d_MINUTESn,
                garray[g0].white.byostones,
                TICS2SECS(garray[g0].white.ticksleft)/60);
      pcn_out(pb, CODE_INFO,
                FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
                parray[pw].pname);
      pcn_out_prompt(pb, CODE_INFO,
               FORMAT_HE_HAS_d_STONES_AND_d_MINUTESn,
                garray[g0].white.byostones,
                TICS2SECS(garray[g0].white.ticksleft)/60);
    }
#if WANT_PAIR
    if (paired(g0)) {
      if (garray[g2].white.ticksleft <= 0 && garray[g2].white.byoperiods == 0) {
        garray[g2].white.byoperiods = 1;
        garray[g2].white.byostones = garray[g2].ts.byostones;
        garray[g2].white.ticksleft = garray[g2].ts.byoticks;
	  /* AvK split */
        pcn_out(pw2, CODE_INFO, 
                    FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
                    parray[garray[g2].white.pnum].pname);
        pcn_out_prompt(pw2, CODE_INFO, 
                    FORMAT_YOU_HAVE_d_STONES_AND_d_MINUTESn,
                    garray[g2].white.byostones,
                    TICS2SECS(garray[g2].white.ticksleft)/60);
        pcn_out(pb2, CODE_INFO, 
                    FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
                    parray[garray[g2].white.pnum].pname);
        pcn_out_prompt(pb2, CODE_INFO, 
                   FORMAT_HE_HAS_d_STONES_AND_d_MINUTESn,
                    garray[g2].white.byostones,
                    TICS2SECS(garray[g2].white.ticksleft)/60);
      }
    }
#endif /* WANT_PAIR */

  } else {  /* onMove == PLAYER_BLACK */
    garray[g0].black.ticksleft -= jiffies ;
#if WANT_PAIR
    if (paired(g0)) {
      garray[g2].black.ticksleft = garray[g0].black.ticksleft;
    }
#endif /* WANT_PAIR */
    if (garray[g0].black.ticksleft <= 0 && garray[g0].black.byoperiods == 0) {
      garray[g0].black.byoperiods = 1;
      garray[g0].black.byostones = garray[g0].ts.byostones;
      garray[g0].black.ticksleft = garray[g0].ts.byoticks;

      pcn_out(pw, CODE_INFO,
              FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
              parray[pb].pname);
      pcn_out_prompt(pw, CODE_INFO,
              FORMAT_HE_HAS_d_STONES_AND_d_MINUTESn,
              garray[g0].black.byostones,
              TICS2SECS(garray[g0].black.ticksleft)/60);
      pcn_out(pb, CODE_INFO,
              FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
              parray[pb].pname);
      pcn_out_prompt(pb, CODE_INFO,
              FORMAT_YOU_HAVE_d_STONES_AND_d_MINUTESn,
              garray[g0].black.byostones,
              TICS2SECS(garray[g0].black.ticksleft)/60);
    }

#if WANT_PAIR
    if (paired(g0)) {
      if (garray[g2].black.ticksleft <= 0 && garray[g2].black.byoperiods == 0) {
        garray[g2].black.byoperiods = 1;
        garray[g2].black.byostones = garray[g2].ts.byostones - 1;
        garray[g2].black.ticksleft = garray[g2].ts.byoticks;
        pcn_out(pw2, CODE_INFO, 
               FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
               parray[pb2].pname);
        pcn_out_prompt(pw2, CODE_INFO, 
               FORMAT_HE_HAS_d_STONES_AND_d_MINUTESn,
               garray[g2].black.byostones,
               TICS2SECS(garray[g2].black.ticksleft)/60);
        pcn_out(pb2, CODE_INFO, 
               FORMAT_THE_PLAYER_s_IS_NOW_IN_BYO_YOMI_n,
               parray[pb2].pname);
        pcn_out_prompt(pb2, CODE_INFO, 
               FORMAT_YOU_HAVE_d_STONES_AND_d_MINUTESn,
               garray[g2].black.byostones,
               TICS2SECS(garray[g2].black.ticksleft)/60);
      }

    }
#endif /* WANT_PAIR */
  }
  garray[g0].lastDectick = now;
#if WANT_PAIR
  if (paired(g0)) 
    garray[g2].lastDectick = now;
#endif /* WANT_PAIR */    
}

void game_update_times()
{
  int g0;

  for (g0 = 0; g0 < garray_top; g0++) {
    if (!garray[g0].slotstat.is_playing) continue;
    if (garray[g0].gstatus != GSTATUS_ACTIVE) continue;
    if (garray[g0].clockStopped) continue;
    game_update_time(g0);
  }
}

int com_sresign(int p, struct parameter * param)
{
  int pw, pb, g0, oldwstate, oldbstate;
  int old_w_game, old_b_game;
  const char *wname, *bname;

  bname = file_bplayer(param[0].val.string);
  wname = file_wplayer(param[0].val.string);

  if (strlen(bname) < 2) return COM_BADPARAMETERS;
  if (strlen(wname) < 2) return COM_BADPARAMETERS;

  pw = player_fetch(wname);
  if (pw < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_);
    return COM_OK;
    }
  pb = player_fetch(bname);
  if (pb < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_);
    player_unfix(pw);
    return COM_OK;
  }
  
  if (p != pw && p != pb) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_MUST_BE_ONE_OF_THE_TWO_PLAYERS_TO_SRESIGN_);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }

  if (xystat(NULL,FILENAME_GAMES_ws_s,wname, bname) < 0 ) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_sn, 
               parray[pw].pname,parray[pb].pname);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }
  oldbstate = parray[pb].session.protostate;
  oldwstate = parray[pw].session.protostate;
  old_b_game = parray[pb].session.gnum ;
  old_w_game = parray[pw].session.gnum ;

  g0 = game_new(GAMETYPE_GO,19);
  if (game_read(&garray[g0], pw, pb) < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_s_HMMMMM_n, 
           parray[pw].pname,parray[pb].pname);
    game_remove(g0);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }
  parray[pb].session.gnum = g0;
  parray[pw].session.gnum = g0;
  game_ended(g0, (p == pw) ? pb : pw, END_RESIGN);
  parray[pw].session.protostate = oldwstate;
  parray[pb].session.protostate = oldbstate;
  parray[pw].session.gnum = old_w_game;
  parray[pb].session.gnum = old_b_game;

  player_unfix(pb);
  player_unfix(pw);
  return COM_OKN;
}

int com_look(int p, struct parameter * param)
{
  int pw, pb, g0, x, until, wc, bc, oldwstate, oldbstate;
  twodstring statstring;
  const char *wname, *bname;

  until = 0;

  bname = file_bplayer(param[0].val.string);
  wname = file_wplayer(param[0].val.string);

  if (strlen(bname) < 2) return COM_BADPARAMETERS;
  if (strlen(wname) < 2) return COM_BADPARAMETERS;

  pw = player_fetch(wname);
  if (pw < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_);
      return COM_OK;
    }
  pb = player_fetch(bname);
  if (pb < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_BY_THAT_NAME_);
    player_unfix(pw);
    return COM_OK;
  }

  if (xystat(NULL,FILENAME_GAMES_ws_s, wname, bname) < 0 ) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_sn, 
               parray[pw].pname,parray[pb].pname);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }
  oldwstate = parray[pw].session.protostate;
  oldbstate = parray[pb].session.protostate;

  g0 = game_new(GAMETYPE_GO,19);
  if (game_read(&garray[g0], pw, pb) < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_STORED_GAME_s_VS_s_HMMMMM_n, 
           parray[pw].pname,parray[pb].pname);
    game_remove(g0);
    player_unfix(pw);
    player_unfix(pb);
    return COM_OK;
  }
  if (parray[p].i_verbose) send_go_board_to(g0, p);
  else {
    mink_getcaps(garray[g0].minkg, &wc, &bc);
    mink_boardstatus(garray[g0].minkg, statstring);
    pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
                parray[pw].pname,
                parray[pw].srank,
                parray[pw].flags.is_rated ? "*" : " ",
                wc,
                TICS2SECS(garray[g0].white.ticksleft),
                garray[g0].white.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);
    pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
                parray[pb].pname,
                parray[pb].srank,
                parray[pb].flags.is_rated ? "*" : " ",
                bc,
                TICS2SECS(garray[g0].black.ticksleft),
                garray[g0].black.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);
    if (!until) until = garray[g0].minkg->height;
    for(x = 0; x < until; x++) {
      pcn_out(p, CODE_STATUS, FORMAT_d_sn,
                  x,
                  statstring[x]);
    }
  }
  game_remove(g0);
  parray[pb].session.protostate = oldbstate;
  parray[pw].session.protostate = oldwstate;
  player_unfix(pw);
  player_unfix(pb);
  return COM_OKN;
}

int com_problem(int p, struct parameter * param)
{
  int g0, x, until, wc, bc;
  twodstring statstring;
  int problem = 0;
  FILE *fp;

  until = 0;

  if (param[0].type == TYPE_NULL) {
    problem = parray[p].last_problem + 1;
    parray[p].last_problem++;
  }
  else
    problem = param[0].val.integer;

  fp = xyfopen(FILENAME_PROBLEM_d, "r", problem);
  if (!fp) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_IS_NO_PROBLEM_NUMBER_d, problem);
    return COM_OK;
  }
  g0 = game_new(GAMETYPE_GO,19);
  garray[g0].size = 19;
  garray[g0].minkg = mink_initgame(19,19, RULES_NET);
  garray[g0].white.ticksleft = SECS2TICS(600);
  garray[g0].black.ticksleft = SECS2TICS(600);
  garray[g0].gtitle = mystrdup("Black to play");
  garray[g0].black.pnum = p;
  garray[g0].white.pnum = p;
  mink_loadpos(fp, garray[g0].minkg);
  fclose(fp);

  if (parray[p].i_verbose) send_go_board_to(g0, p);
  else {
    mink_getcaps(garray[g0].minkg, &wc, &bc);
    mink_boardstatus(garray[g0].minkg, statstring);
    pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
                parray[p].pname,
                parray[p].srank,
                parray[p].flags.is_rated ? "*" : " ",
                wc,
                TICS2SECS(garray[g0].white.ticksleft),
                garray[g0].white.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);
    pcn_out(p, CODE_STATUS, FORMAT_s_ss_d_d_d_T_f_dn,
                parray[p].pname,
                parray[p].srank,
                parray[p].flags.is_rated ? "*" : " ",
                bc,
                TICS2SECS(garray[g0].black.ticksleft),
                garray[g0].black.byostones,
                garray[g0].komi,
                garray[g0].minkg->handicap);
    if (!until) until = garray[g0].minkg->height;
    for(x = 0; x < until; x++) {
      pcn_out(p, CODE_STATUS, FORMAT_d_sn,
                  x,
                  statstring[x]);
    }
  }
  game_remove(g0);
  return COM_OKN;
}
