/* comproc.c
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997 Erik Van Riper (geek@nngs.cosmic.org)
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
#include <stdlib.h>

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "missing.h"
#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "command.h"
#include "common.h"
#include "servercodes.h"
#include "comproc.h"
#include "chkaddr.h"
#include "gameproc.h"
#include "bm.h"
#include "utils.h"
#include "playerdb.h"
#include "network.h"
#include "channel.h"
#include "variable.h"
#include "gamedb.h"
#include "multicol.h"
#include "mink.h"

#include "ladder.h"
#include "emote2.h"

#include "plan.h"
#include "censor.h"
#include "alias.h"
#include "pending.h"

#if WANT_NNGSRATED
#include "rdbm.h"
#endif

#define TELL_PLAYER_IN_TMATCH(From, To) \
    (( (parray[To].session.gnum >= 0) \
    && (parray[From].session.gnum != parray[To].session.gnum) \
    && (parray[To].match_type == GAMETYPE_TNETGO) \
    )||( (parray[From].match_type == GAMETYPE_TNETGO) \
    && (parray[From].session.gnum != parray[To].session.gnum) \
    ))

#define TELL_TELL 0
#define TELL_SAY 1
#define TELL_KIBITZ 3
#define TELL_CHANNEL 4
#define TELL_BEEP 5
	/* This declaration is here because we *absolutely* want
	** a prototype in scope. If it conflicts with a header file:
	** so be it ...
	*/

static int index_lookup(char* found , char * find);
static int com_xmatch(int p, struct parameter * param, int gametype);
static int do_tell(int p, int p1, const char *msg, int why, int ch);
static void a_who(int p, int cnt, int *plist);

int com_register(int p0, struct parameter * param)
{
  char text[2048];
  char *pname = param[0].val.word;
  char *email = param[1].val.word;
  const char *fullname = param[2].val.string;
  char passwd[sizeof parray[p0].passwd];
  char login[sizeof parray[p0].login];
  char tmp[200];
  char salt[4];
  int p1, p2;
  int idx, len;
  time_t shuttime = globclock.time;

  if (conffile.allow_registration < 1 && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_THAT_NAME_IS_NOT_PERMITTED);
    return COM_OK;
  }
  len = strlen(pname);
  if (len > MAX_NAME) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_NAME_IS_TOO_LONG);
    return COM_OK;
  }
  if (len < MIN_NAME) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_NAME_IS_TOO_SHORT);
    return COM_OK;
  }
  if (!strncasecmp(pname, "guest", 5)) {
    pcn_out(p0, CODE_ERROR, FORMAT_IT_WOULD_NOT_BE_NICE_TO_REGISTER_GUEST);
    return COM_OK;
  }
  if (invalid_pname(pname)) {
    pcn_out(p0, CODE_ERROR, FORMAT_ILLEGAL_CHARACTERS_IN_PLAYER_NAME_ONLY_A_Z_A_Z_0_9_ALLOWED_);
    return COM_OK;
  }
  if (in_list_match("badname", pname)) {
    pcn_out(p0, CODE_ERROR, FORMAT_THAT_NAME_IS_NOT_PERMITTED);
    return COM_OK;
  }
  if (param[2].type != TYPE_STRING) fullname = "Not Provided";
  strcpy(login, pname);
  stolower(login);
  p1 = player_fetch(login);
  if (p1>=0 && parray[p1].slotstat.is_registered) {
    pcn_out(p0, CODE_ERROR, FORMAT_A_PLAYER_BY_THE_NAME_s_IS_ALREADY_REGISTERED_,
		 login);
    player_unfix(p1);
    return COM_OK;
  }
  player_unfix(p1);
  switch (chkaddr(email)) {
  case 0: break;
  case 1: if (conffile.allow_mail_to_localhost > 0) break; else {;}
  default:
    pcn_out(p0, CODE_ERROR, FORMAT_INVALID_EMAIL_ADDRESSn);
    return COM_OK;
  }
  p1 = player_new();
  do_copy(parray[p1].login, login, sizeof parray[0].login);
  do_copy(parray[p1].pname, pname, sizeof parray[0].pname);
  do_copy(parray[p1].rank, "NR", sizeof parray[0].rank);
  do_copy(parray[p1].ranked, "NR", sizeof parray[0].ranked);
  do_copy(parray[p1].srank, "NR", sizeof parray[0].srank);
  do_copy(parray[p1].fullname, fullname, sizeof parray[0].fullname);
  do_copy(parray[p1].email, email, sizeof parray[0].email);
  do_copy(parray[p1].RegDate, time2str_local(&shuttime), sizeof parray[0].RegDate);
  for (idx = 0; idx < PASSLEN; idx++) {
    passwd[idx] = 'a' + rand() % 26;
  }
  passwd[idx] = '\0';
  salt[0] = 'a' + rand() % 26;
  salt[1] = 'a' + rand() % 26;
  salt[2] = '\0';
  do_copy(parray[p1].passwd, mycrypt(passwd, salt), sizeof parray[0].passwd);
  parray[p1].adminLevel = ADMIN_REGISTERED_USER;
  parray[p1].slotstat.is_registered = 1;
  parray[p1].slotstat.is_valid = 1;
  player_dirty(p1);
  player_unfix(p1);

  sprintf(text, "\nWelcome to %s!  Here is your account info:\n\n\
  Login Name: %s\n\
  Full Name: %s\n\
  Email Address: %s\n\
  Initial Password: %s\n\n\
  Host registered from: %s as %s \n\n\
If any of this information is incorrect, please contact the administrator\n\
to get it corrected at %s.\n\n\
Please write down your password, as it will be your initial password\n\
To access the server, telnet to %s port %s\n\
Please change your password after logging in.  See \"help password\"\n\
For additional help, type \"help welcome\" while on the server.\n\n\
On WWW, please try %s/\n\n\
Regards,\n\n\
NNGS admins\n\n--"
        , conffile.server_name
        , pname, fullname
        , email, passwd
        , dotQuad(parray[p0].thisHost)
        , parray[p0].pname
        , conffile.server_email
	, conffile.server_address, conffile.server_ports
	, conffile.server_http);
  sprintf(tmp, "%s Account Created (%s)"
	, conffile.server_name, pname);
  if ((idx = mail_ast(email, tmp, text)) < 0)
    Logit("mail_ast(\"%s\", ...) returned %d", email, idx);
  /* Mail a copy to geek for testing / verification.  */
  if (conffile.geek_email) {
    sprintf(text, "\n\
Login Name: %s\n\
Full Name: %s\n\
Email Address: %s\n\
Initial Password: %s\n\
Host registered from: %s \n\n--"
    , pname, fullname, email, passwd
    , dotQuad(parray[p0].thisHost));
    if ((idx = mail_ast(conffile.geek_email, tmp, text)) < 0)
      Logit("mail_ast(\"%s\", ...) returned %d"
      , conffile.geek_email, idx);
  }
  Logit("NewPlayer: %s [%s] %s (%s) by user %s"
  , pname, email, fullname, passwd, parray[p0].pname);
  pcn_out(p0, CODE_INFO, FORMAT_YOU_ARE_NOW_REGISTERED_CONFIRMATION_TOGETHER_WITH_PASSWORD_IS_SENT_TO_YOURn);
  pcn_out_prompt(p0, CODE_INFO, FORMAT_EMAIL_ADDRESS_sn, email);
  for (p2 = 0; p2 < parray_top; p2++) { /* Announce to all online admins */
    if (!parray[p2].slotstat.is_online) continue;
    if (parray[p2].adminLevel < ADMIN_ADMIN) continue;
    pcn_out_prompt(p2, CODE_SHOUT, FORMAT_NEW_PLAYER_s_s_BY_ss_n
    , pname, email
    , (parray[p0].slotstat.is_registered ? "" : "guest ")
    , parray[p0].pname);
  }
  new_players++;
  return COM_OK;
}

int com_review(int p0, struct parameter * param)
{
  UNUSED(param);
  pcn_out(p0,CODE_INFO, FORMAT_THE_CURRENTLY_LOADED_GAMES_n);
  pcn_out(p0,CODE_INFO, FORMAT_YOU_CANNOT_REVIEW_MULTIPLE_SGF_FILES_n);
  pcn_out(p0,CODE_INFO, FORMAT_YOU_NEED_REVIEW_A_GAME_REVIEW_LOADED_GAME_FILE_IS_A_LOADED_FILE_);

  return COM_OK;
}

int com_more(int p0, struct parameter * param)
{
  UNUSED(param);
  pmore_file( p0 );
  return COM_OK;
}

int com_ayt(int p0, struct parameter * param)
{
  UNUSED(param);
  pcn_out(p0,CODE_INFO, FORMAT_YES);
  return COM_OK;
}

int com_news(int p0, struct parameter * param)
{
  FILE *fp;
  char junk[MAX_LINE_SIZE];
  char *junkp;
  time_t crtime;
  char count[10];
  int flag, len;

  if (param[0].type==TYPE_NULL || !strcmp(param[0].val.word,"all")) {

/* no params - then just display index over news */

    pcn_out(p0,CODE_INFO, FORMAT_BULLETIN_BOARD_n);
    fp = xyfopen(FILENAME_NEWSINDEX, "r");
    if (!fp) {
      return COM_OK;
    }
    flag=0;
    while ((junkp=fgets(junk, sizeof junk, fp))) {
      if ((len = strlen(junk))<=1) continue;
      junk[len-1]='\0';
      sscanf(junkp, "%d %s", (int*) &crtime, count);
      junkp=nextword(junkp);
      junkp=nextword(junkp);
      if (param[0].type==TYPE_WORD && !strcmp(param[0].val.word,"all")) {
        pcn_out(p0,CODE_INFO, FORMAT_s_s_sn, count, time2str_local(&crtime), junkp);
        flag=1;
      } else {
        if (crtime - player_lastconnect(p0) > 0) {
          pcn_out(p0,CODE_INFO, FORMAT_s_s_sn, count, time2str_local(&crtime), junkp);
	flag=1;
	}
      }
    }
    fclose(fp);
    crtime=player_lastconnect(p0);
    if (!flag) {
      pcn_out(p0,CODE_INFO, FORMAT_THERE_IS_NO_NEWS_SINCE_YOUR_LAST_LOGIN_s_n, time2str_local(&crtime));
    } else {
      pcn_out(p0,CODE_INFO, FORMAT_n);
    }
  } else {

/* check if the specific news file exist in index */

    fp = xyfopen(FILENAME_NEWSINDEX, "r");
    if (!fp) {
      return COM_OK;
    }
    while ((junkp=fgets(junk, sizeof junk, fp))) {
      if ((len = strlen(junk))<=1) continue;
      junk[len-1]='\0';
      sscanf(junkp, "%d %s", (int*) &crtime, count);
      if (!strcmp(count,param[0].val.word)) {
        junkp=nextword(junkp);
        junkp=nextword(junkp);
        pcn_out(p0,CODE_INFO, FORMAT_NEWS_s_s_nn_sn, count, time2str_local(&crtime), junkp);
	break;
      }
    }
    fclose(fp);
    if (!junkp) {
      pcn_out(p0, CODE_ERROR, FORMAT_BAD_INDEX_NUMBER_n);
      return COM_OK;
    }

/* file exists - show it */

    fp = xyfopen(FILENAME_NEWS_s, "r", param[0].val.word);
    if (!fp) {
      pcn_out(p0, CODE_ERROR, FORMAT_NO_MORE_INFO_n);
      return COM_OK;
    }
    fclose(fp);
    if (pxysend_file(p0, FILENAME_NEWS_s, param[0].val.word) < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_INTERNAL_ERROR_COULDN_T_SEND_NEWS_FILE_n);
    }
  }
  return COM_OK;
}

/* Next function is for public bulletin */
/* add by Syncanph */

int com_note(int p0, struct parameter * param)
{
  FILE *fp;
  time_t tt = globclock.time;

  fp = xyfopen(FILENAME_NOTEFILE,"a");
  if (!fp) {
    pcn_out(p0, CODE_ERROR, FORMAT_ERROR_NOTE_NOT_SAVEDn);
    return COM_OK;
  }
  fputs(parray[p0].pname, fp);
  fputs(" on ",fp);
  fputs(time2str_utc(&tt),fp);
  fputs(": \n '",fp);
  fputs(param[0].val.string, fp);
  fputs("'\n\n",fp);
  fclose(fp);

  pcn_out(p0,CODE_INFO, FORMAT_YOUR_NOTE_SAVED_n);
  return COM_OK;
}


int com_shownote(int p0, struct parameter * param)
{
  UNUSED(param);
  pxysend_file (p0, FILENAME_NOTEFILE);
  return COM_OK;
}


/* see if a person is a member of a list or not */
int in_list(const char *listname, const char *member)
{
  FILE *fp;
  char listmember[20];
  char *cp;
  int len;

  fp = xyfopen(FILENAME_LIST_s, "r", listname);
  if (!fp) return 0;

  while ((cp=fgets(listmember,sizeof listmember,fp))) {
    if ((len =strlen(listmember)) <= 1) continue;
    listmember[len-1] = 0;
    if (!strcasecmp(member, listmember)) break;
    }
  fclose(fp);
  return cp ? 1: 0;
}


int in_list_match(const char *listname, const char *member)
{
  FILE *fp;
  char listmember[20];
  char *cp;
  int len;

  fp = xyfopen(FILENAME_LIST_s, "r",listname);
  if (!fp) return 0;
  while ((cp=fgets(listmember, sizeof listmember, fp))) {
    len=strlen(listmember);
    if (len < 1) continue;
    cp[len-1] = 0;
    if (!strncasecmp(listmember, member, len)) break;
  }
  fclose(fp);
  return cp ? 1: 0;
}


/* add a user to a certain list */
int com_addlist(int p0, struct parameter * param)
{
  FILE *fp;
  char *listname = param[0].val.word;
  char *member;
  char lmember[20];
  int rights;
  int p1;

/* Lookup exact name and rights in the index file */
  rights = index_lookup(lmember,listname);

  switch(rights) {
  case -1:
    pcn_out(p0, CODE_ERROR, FORMAT_NO_SUCH_LIST_n); 
    return COM_OK;
    break;
  case -2:
    return COM_OK;
    break;
  }

  if (rights < 2 && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_ALLOWED_TO_EDIT_THIS_LIST_n);
    return COM_OK;
    }
  listname=lmember;

/* get exact member name */
  p1 = player_fetch(param[1].val.word);
  if (p1<0) return COM_OK;
  member=parray[p1].pname;

/* listname is now the exact list name. --- Now check if member is already in
the list */

  if (in_list(listname, member)) {
    pcn_out(p0, CODE_ERROR, FORMAT_s_IS_ALREADY_IN_THE_s_LIST_n, member,listname);
    player_unfix(p1);
    return COM_OK;
  }

/* expand list with member */

  pcn_out(p0,CODE_INFO, FORMAT_s_ADDED_TO_THE_s_LIST_n, member,listname);
  fp = xyfopen(FILENAME_LIST_s, "a", listname);
  fprintf(fp, "%s\n", member);
  fclose(fp);
  player_unfix(p1);
  return COM_OK;
}


/* remove a user from a certain list */
int com_sublist(int p0, struct parameter * param)
{
  FILE *fp1;
  FILE *fp2;
  char *listname = param[0].val.word;
  char lmember[20];
  char *member;
  int rights;
  int p1;
  char *cp;
  int len;

/* Lookup exact name and rights in the index file */

  rights=index_lookup(lmember,listname);
  switch(rights) {
  case -1:
    pcn_out(p0, CODE_ERROR, FORMAT_NO_SUCH_LIST_n); 
    return COM_OK;
  case -2:
    return COM_OK;
  }
  if (rights < 2 && parray[p0].adminLevel < ADMIN_MASTER) {
     pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_ALLOWED_TO_EDIT_THIS_LIST_n);
	return COM_OK;
      }
  listname=lmember;

/* get exact member name */

  p1 = player_fetch(param[1].val.word);
  if (p1<0) return COM_OK;
  member=parray[p1].pname;

/* listname is now the exact list name. --- Now check if member is
already in the list */

  if (!in_list(listname, member)) {
    pcn_out(p0, CODE_ERROR, FORMAT_s_IS_NOT_IN_THE_s_LIST_n, member,listname);
    player_unfix(p1);
    return COM_OK;
  }

/* remove member from the list */

  pcn_out(p0,CODE_INFO, FORMAT_s_HAS_BEEN_REMOVED_FROM_THE_s_LIST_n, member,listname);
  xyrename(FILENAME_LIST_s, FILENAME_LIST_s_OLD, listname);
  fp1 = xyfopen(FILENAME_LIST_s, "w", listname);
  fp2 = xyfopen(FILENAME_LIST_s_OLD, "r", listname);
  while((cp=fgets(lmember, sizeof lmember, fp2))) {
    if ((len=strlen(cp)) < 1) {
      Logit("Emty line, not copied in \"%s\"", filename() );
      continue;
    }
    if (!strncasecmp(lmember, member,len-1)) continue;
    fputs(lmember,fp1);
  }
  fclose(fp1);
  fclose(fp2);
  player_unfix(p1);
  return COM_OK;
}


/* show all lists or show all members of any given list */
int com_showlist(int p0, struct parameter * param)
{
  FILE *fp;
  char *listname = param[0].val.word;
  char member[20];
  char buff[MAX_LINE_SIZE];
  int rights;
  int lines;

/* no args -> show all lists */
  if (param[0].type == 0) {
    pcn_out(p0,CODE_INFO, FORMAT_LISTS_n);

    fp = xyfopen(FILENAME_LISTINDEX, "r");
    if (!fp) return COM_OK;

    for(lines=1; fgets(buff, sizeof buff, fp); lines++) {
      if (sscanf(buff, "%s %d\n", member, &rights) != 2) {
        Logit("Bad format in \"%s\", line%d:%s", filename(), lines, buff );
#if 0
        fclose(fp);
        return COM_OK;
#else
        continue;
#endif
      }
      pcn_out(p0,CODE_INFO, FORMAT_s_IS_sn, member, (rights == 0) ? "SECRET" :
(rights == 1) ? "READ ONLY" : "READ/WRITE");
    }

    fclose(fp);
    return COM_OK;
  }

/* Lookup exact name and rights in the index file */
  memset(member,0,sizeof member);
  rights = index_lookup(member,listname);
  switch(rights) {
  case -1:
    pcn_out(p0, CODE_ERROR, FORMAT_NO_SUCH_LIST_n); 
    return COM_OK;
  case -2:
    return COM_OK;
  }
  if (rights < 1 && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_ALLOWED_TO_SEE_THIS_LIST_);
    return COM_OK;
    }
  listname=member;

/* display the file */

/*  pprintf(p0, "-- The %s list: --",listname); */
  {
    char linebuff[80];
    FILE *fp2;
    int count = 0;
    struct multicol *m = multicol_start(1000);
    char *cp;
    int len;
    /* if a list has >1000 names, we're in trouble! */

    fp2 = xyfopen(FILENAME_LIST_s, "r",listname);
    if (!fp2) {
      return COM_OK;
    }
    while((cp=fgets(linebuff, sizeof linebuff, fp2))) {
      if ((len = strlen(linebuff)) < 1) continue; 
      linebuff[len-1] = '\0';
      multicol_store_sorted( m, linebuff );
      count++;
    }
    fclose(fp2);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    pprintf( p0, "-- The %s list: %d names --", listname, count );
    multicol_pprint( m, p0, 78, 2 );
    multicol_end( m );
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
  }
  return COM_OK;
}


int com_quit(int p0, struct parameter * param)
{
  UNUSED(param);
  if (parray[p0].session.gnum >= 0) {
    pcn_out_prompt(parray[p0].session.opponent,CODE_ERROR,
    FORMAT_YOUR_OPPONENT_TYPED_QUIT_PERHAPS_THEY_HAD_AN_EMERGENCY_n);
    Logit("ESCAPE: %s quit from game with %s",
           parray[p0].pname, parray[parray[p0].session.opponent].pname);
  }
  pxysend_file(p0, FILENAME_MESS_LOGOUT);
  return COM_LOGOUT;
}


int com_best(int p0, struct parameter * param)
{
  const struct ladderplayer *lp;
  int i, type, low, high;
  const char *cp;

  i = low = high = 0;

  if (param[0].type == TYPE_WORD) {
    i = sscanf(param[0].val.word, "%d-%d", &low, &high);
    if (i == 1) high = low;
    type = 19;
  }
  else if (param[0].type == TYPE_INT) type = param[0].val.integer;
  else type = 0;

  if (param[1].type == TYPE_INT) low = high = param[1].val.integer;
  else if (param[1].type == TYPE_WORD) {
    i = sscanf(param[1].val.word, "%d-%d", &low, &high);
    if (i == 1) high = low;
  }
  else {
    if (type == 9 && high == 0) high = num_9;
    else if (type == 19 && high == 0) high = num_19;
  }

  if (low) low--;
  if (low < 0) low = 0;
  if (low > high) { i = high; high = low; low = i;}
  if (high <= 0) high = 1;
  if (type == 9) {
    if (high > num_9) high = num_9;
    if (low > high) low = 0;
  }
  else {
    if (high > num_19) high = num_19;
    if (low > high) low = 0;
  }

  pcn_out(p0,CODE_INFO, FORMAT_POSITION_NAME_W_L_DATE_LAST_PLAYEDn);
  pcn_out(p0,CODE_INFO, FORMAT_n);
  for(i = low; i < high; i++) {
    if (type == 9)
      lp = ladder_player_at(Ladder9, i);
    else
      lp = ladder_player_at(Ladder19, i);
    cp = lp->lasttime 
       ? time2str_utc((const time_t *) &lp->lasttime)
       : "---" ;
    pcn_out(p0, CODE_INFO, FORMAT_d_s_d_d_sn,
      i+1,
      lp->name,
      lp->nWins, lp->nLosses,
      cp);
  }
  return COM_OKN;
}


int com_join(int p0, struct parameter * param)
{
  FILE *fp;
  const struct ladderplayer *lp;
  time_t now;

  now = globclock.time;

  if (!parray[p0].slotstat.is_registered) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOU_MUST_REGISTER_TO_PLAY_ON_THE_LADDER_);
    return COM_OK;
  }
  if (parray[p0].session.gnum >= 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOU_CANNOT_JOIN_A_LADDER_WHILE_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (param[0].type == 0) {
  /* no args -> show all ladders */
    pcn_out(p0,CODE_INFO, FORMAT_LADDERS_n);
    pcn_out(p0,CODE_INFO, FORMAT_LADDER9_9X9_LADDERn);
    pcn_out(p0,CODE_INFO, FORMAT_LADDER19_19X19_LADDER);
    return COM_OK;
  }
  if (param[0].val.string) {
    if (!strcmp(param[0].val.string, "ladder9")) {
      if (ladder_player_named(Ladder9, parray[p0].pname)) {
        pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_ALREADY_A_MEMBER_OF_THE_9X9_LADDER_);
        return COM_OK;
      }
      ladder_new_p(Ladder9, parray[p0].pname);
      lp = ladder_player_named(Ladder9, parray[p0].pname);
      ladder_set_time(Ladder9, lp->idx, now);
      fp = xyfopen(FILENAME_LADDER9, "w");
      if (!fp) {
        pcn_out(p0, CODE_ERROR,FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_);
        return COM_OK;
      }
      num_9 = ladder_save(fp, Ladder9);
      fclose(fp);
      pcn_out(p0, CODE_INFO, FORMAT_YOU_ARE_AT_POSITION_d_IN_THE_9X9_LADDER_GOOD_LUCK_,
                  (lp->idx) + 1);
      player_resort();
      return COM_OK;
    }
    else if (!strcmp(param[0].val.string, "ladder19")) {
      if (ladder_player_named(Ladder19, parray[p0].pname) ) {
        pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_ALREADY_A_MEMBER_OF_THE_19X19_LADDER_);
        return COM_OK;
      }
      ladder_new_p(Ladder19, parray[p0].pname);
      lp = ladder_player_named(Ladder19, parray[p0].pname);
      ladder_set_time(Ladder19, lp->idx, now);
      fp = xyfopen(FILENAME_LADDER9, "w");
      if (!fp) {
        pcn_out(p0, CODE_ERROR, FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_);
        return COM_OK;
      }
      num_19 = ladder_save(fp, Ladder19);
      fclose(fp);
      pcn_out(p0, CODE_INFO, FORMAT_YOU_ARE_AT_POSITION_d_IN_THE_19X19_LADDER_GOOD_LUCK_, (lp->idx) + 1);
      player_resort();
      return COM_OK;
    }
    else return COM_BADPARAMETERS;
  }
  return COM_OK;
}

int com_drop(int p0, struct parameter * param)
{
  FILE *fp;
  const struct ladderplayer *lp;

  if (!parray[p0].slotstat.is_registered) {
    pcn_out(p0,CODE_ERROR,  FORMAT_SORRY_YOU_MUST_REGISTER_TO_PLAY_ON_THE_LADDER_);
    return COM_OK;
  }

  if (parray[p0].session.gnum >= 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOU_CANNOT_DROP_A_LADDER_WHILE_PLAYING_A_GAME_ );
    return COM_OK;
  }

  if (param[0].type == 0) {
  /* no args -> show all ladders */
    pcn_out(p0,CODE_INFO, FORMAT_YOU_ARE_IN_LADDERS_n);
    lp = ladder_player_named(Ladder9, parray[p0].pname);
    if (lp)
      pcn_out(p0, CODE_INFO, FORMAT_LADDER9_POSITION_dn, (lp->idx) + 1);
    lp = ladder_player_named(Ladder19, parray[p0].pname);
    if (lp)
      pcn_out(p0, CODE_INFO, FORMAT_LADDER19_POSITION_d, (lp->idx) + 1);
    return COM_OK;
  }
  if (param[0].val.string) {
    if (!strcmp(param[0].val.string, "ladder9")) {
      if (!(lp = ladder_player_named(Ladder9, parray[p0].pname))) {
        pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_A_MEMBER_OF_THE_9X9_LADDER_);
        return COM_OK;
      }
      ladder_remove_at(Ladder9, lp->idx);
      fp = xyfopen(FILENAME_LADDER9, "w");
      if (!fp) {
        pcn_out(p0, CODE_ERROR, FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        return COM_OK;
      }
      num_9 = ladder_save(fp, Ladder9);
      fclose(fp);
      pcn_out(p0, CODE_INFO, FORMAT_YOU_HAVE_BEEN_REMOVED_FROM_THE_9X9_LADDER_);
      player_resort();
      return COM_OK;
    }
    if (!strcmp(param[0].val.string, "ladder19")) {
      if (!(lp = ladder_player_named(Ladder19, parray[p0].pname))) {
        pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_A_MEMBER_OF_THE_19X19_LADDER_);
        return COM_OK;
      }
      ladder_remove_at(Ladder19, lp->idx);
      fp = xyfopen(FILENAME_LADDER19, "w");
      if (!fp) {
        Logit("Error opening \"%s\" for write!!!", filename() );
        pcn_out(p0, CODE_ERROR, FORMAT_THERE_WAS_AN_INTERNAL_ERROR_PLEASE_NOTIFY_AN_ADMIN_n);
        return COM_OK;
      }
      num_19 = ladder_save(fp, Ladder19);
      fclose(fp);
      pcn_out(p0, CODE_INFO, FORMAT_YOU_HAVE_BEEN_REMOVED_FROM_THE_19X19_LADDER_);
      player_resort();
      return COM_OK;
    }
  }
  return COM_OK;
}

int com_clntvrfy(int p0, struct parameter * param)
{
  char tmp[MAX_STRING_LENGTH];
  char *atmp;
  int len, ii;

  if (param[0].type == 0) { return COM_BADPARAMETERS; }
  if (strlen(param[0].val.string) < 2) { return COM_BADPARAMETERS; }

  atmp = getword(param[0].val.string);

  if (param[0].val.string) {
    /* AvK: %c ??
    sprintf(tmp, "%s%c\n", SendCode(p0, CODE_CLIVRFY), param[0].val.string[0]);
    */
/* AvK: FIXME: string[strlen] == always '\0' */
    pcn_out(p0, CODE_CLIVRFY, FORMAT_sn, atmp);
    len = strlen(atmp);
    atmp = param[0].val.string + len;
    for(ii=0;*atmp;ii++) {
      if (*atmp == '%') { tmp[ii++] = '%';}
      tmp[ii++] = *atmp;
      atmp++;
    }
    tmp[ii] = '\0';
    pcommand(p0, tmp);
  }
  return COM_OKN;
}

int com_choice(int p0, struct parameter * param)
{
  UNUSED(param);
  pcn_out(p0,CODE_INFO, FORMAT_GAME_SET_TO_GO_);
  return COM_OK;
}


int com_shout(int p0, struct parameter * param)
{
  int p1, i;

  if (!parray[p0].slotstat.is_registered && conffile.unregs_can_shout < 1) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_SHOUT_COMMAND_);
    return COM_OK;
  }
  if (parray[p0].muzzled) {
    return COM_OK;
  }

  if (carray[CHANNEL_SHOUT].is_locked && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_SHOUTS_ARE_TURNED_OFF_RIGHT_NOW_);
    return COM_OK;
  }

  for (i = 0; i < carray[CHANNEL_SHOUT].count; i++) {
    p1 = carray[CHANNEL_SHOUT].members[i];
    if (p1 == p0) continue;
    if (!parray[p1].slotstat.is_online) continue;
    if (player_censored(p1, p0)) continue;
    pcn_out_prompt(p1, CODE_SHOUT, FORMAT_s_arrow_sn,
                   parray[p0].pname, param[0].val.string);
  }
  return COM_OK;
}

int com_lashout(int p0, struct parameter * param)
{
  int i;
  UNUSED(param);

  for (i = 0; i < carray[CHANNEL_ASHOUT].Num_Yell; i++) {
    pcn_out(p0,CODE_INFO, FORMAT_s, carray[CHANNEL_ASHOUT].Yell_Stack[i]);
  }
  return COM_OK;
}

int com_lchan(int p0, struct parameter * param)
{
  int ch, i;

  if (param[0].type == TYPE_INT)
    ch = param[0].val.integer;
  else
  {
    ch = parray[p0].last_channel;
    if (ch < 0)
    {
      pcn_out(p0,CODE_INFO, FORMAT_NO_LAST_CHANNEL_);
      return COM_OK;
    }
  }

  if (ch < 0 || ch >= MAX_NCHANNEL)
    pcn_out(p0,CODE_INFO, FORMAT_NO_SUCH_CHANNEL_);
  else if (!on_channel(ch, p0))
    pcn_out(p0,CODE_INFO, FORMAT_YOU_ARE_NOT_IN_CHANNEL_d_, ch);
  else
  {
    pcn_out(p0,CODE_INFO, FORMAT_LAST_IN_CHANNEL_d_n, ch);
    for (i = 0; i < carray[ch].Num_Yell; i++)
      pcn_out(p0,CODE_INFO, FORMAT_s, carray[ch].Yell_Stack[i]);
  }
  return COM_OK;
}


int com_admins(int p0, struct parameter * param)
{
  int p1, count = 0;
  UNUSED(param);

  pcn_out(p0,CODE_INFO, FORMAT_ADMINS_AVAILABLE_TO_HELP_YOU_n);
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_online ) continue;
    if (parray[p1].adminLevel < ADMIN_ADMIN) continue;
    if (parray[p1].session.gnum >= 0) continue;
    if (((player_idle(p1)%3600)/60) > 45) continue;
    if (count %3 == 0)
      pcn_out(p0,CODE_CR1|CODE_INFO, FORMAT_empty);
    count++;
    pprintf(p0, " %13s", parray[p1].pname);
  }
  pcn_out(p0, CODE_CR1|CODE_INFO, FORMAT_nFOUND_d_ADMINs_TO_HELP_YOU_,
              count, (count > 1) ? "s" : "");
  return COM_OK;
}

int com_ashout(int p0, struct parameter * param)
{
  int p1, i;
  char text[MAX_STRING_LENGTH];

  if (parray[p0].adminLevel < ADMIN_ADMIN) {
    return COM_OK;
  }

  for (i = 0; i < carray[CHANNEL_ASHOUT].count; i++) {
    p1 = carray[CHANNEL_ASHOUT].members[i];
    if (p1 == p0) continue;
    if (player_censored(p1, p0)) continue;
    if (!parray[p1].slotstat.is_online) continue;
    pcn_out_prompt(p1, CODE_SHOUT, FORMAT_s_arrow_sn,
                   parray[p0].pname,
		   param[0].val.string);
  }
  sprintf(text, "<%s> %s\n", parray[p0].pname, param[0].val.string);
  add_to_yell_stack(CHANNEL_ASHOUT, text);

  return COM_OK;
}


int com_gshout(int p0, struct parameter * param)
{
  int p1;

  if (!parray[p0].slotstat.is_registered && conffile.unregs_can_shout < 1) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_GSHOUT_COMMAND_);
    return COM_OK;
  }

  if (parray[p0].gmuzzled) {
    return COM_OK;
  }
  if (carray[CHANNEL_SHOUT].is_locked && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_SHOUTS_ARE_TURNED_OFF_RIGHT_NOW_);
    return COM_OK;
  }

  for (p1 = 0; p1 < parray_top; p1++) {
    if (p1 == p0) continue;
    if (!parray[p1].slotstat.is_online) continue;
    if (!parray[p1].i_gshout) continue;
    if (player_censored(p1, p0)) continue;
    pcn_out_prompt(p1, CODE_SHOUT, FORMAT_s_arrow_sn,
                   parray[p0].pname, param[0].val.string);
  }
  return COM_OK;
}

int com_it(int p0, struct parameter * param)
{
  int p1;
  int why;

  if (!parray[p0].slotstat.is_registered && conffile.unregs_can_shout < 1) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_IT_COMMAND_);
    return COM_OK;
  }
  if (parray[p0].muzzled) return COM_OK;

  why=param[0].val.string[0];
  for (p1 = 0; p1 < parray_top; p1++) {
    if (p1 == p0) continue;
    if (!parray[p1].slotstat.is_online) continue;
    if (!parray[p1].i_shout) continue;
    if (player_censored(p1, p0)) continue;
    switch (why) {
    case '\'' :
    case ',' :
    case '.' :
      pcn_out_prompt(p1, CODE_SHOUT, FORMAT_ssn,
                   parray[p0].pname, param[0].val.string);
      break;
    default:
      pcn_out_prompt(p1, CODE_SHOUT, FORMAT_s_arrow_sn,
                   parray[p0].pname, param[0].val.string);
      break;
    }
  }
  return COM_OK;
}

int com_git(int p0, struct parameter * param)
{
  int p1;

  if (!parray[p0].slotstat.is_registered && conffile.unregs_can_shout < 1) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_IT_COMMAND_);
    return COM_OK;
  }
  if (parray[p0].muzzled) {
    return COM_OK;
  }
  for (p1 = 0; p1 < parray_top; p1++) {
    if (p1 == p0) continue;
    if (!parray[p1].slotstat.is_online) continue;
    if (!parray[p1].i_gshout) continue;
    if (player_censored(p1, p0)) continue;
    switch(param[0].val.string[0]) {
    case '\'' : case ',' : case '.' :
      pcn_out_prompt(p1, CODE_SHOUT, FORMAT_ssn,
                   parray[p0].pname, param[0].val.string);
      break;
    default:
       pcn_out_prompt(p1, CODE_SHOUT, FORMAT_s_arrow_sn,
                   parray[p0].pname, param[0].val.string);
       break;
    }
  }
  return COM_OK;
}

int com_emote(int p0, struct parameter * param)
{
  const char *tmp;
  char args[MAX_STRING_LENGTH];
  int p1;

  if (param[0].type != TYPE_WORD || param[1].type != TYPE_WORD)
    return COM_BADPARAMETERS;
  stolower(param[0].val.word);
  stolower(param[1].val.word);
  
  p1 = player_find_part_login(param[1].val.word);
  if (p1 < 0) {
     pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[1].val.word);
     return COM_OK;
  }

  if (player_censored(p1, p0)) {
    pcn_out(p0, CODE_INFO, FORMAT_PLAYER_qsq_IS_CENSORING_YOU_, parray[p1].pname);
    return COM_OK;
  }
  if (param[2].type != TYPE_NULL) {
    strcpy(args, param[2].val.string);
  } else {
    args[0] = 0;
  }

  if (!strcmp(param[0].val.word, "balloon")) {
    if (!parray[p0].water) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_OUT_OF_WATER_BALLOONS_GO_WIN_SOME_GAMES_);
      return COM_OK;
    } else {
      if (parray[p0].adminLevel < ADMIN_ADMIN) {
        parray[p0].water--;
      }
    }
  }

  tmp = EmoteMkStr(param[0].val.word, parray[p0].pname, args, parray[p1].flags.is_client);

  if (tmp) {
    pcn_out(p1, CODE_EMOTE, FORMAT_s, parray[p1].flags.is_client ? "" : "\n");
    pcn_out_prompt(p1, CODE_EMOTE, FORMAT_s_n, tmp);
    tmp = EmoteMkStr(param[0].val.word, parray[p0].pname, args, parray[p0].flags.is_client);
    pcn_out(p0, CODE_INFO, FORMAT_s_, tmp);
  }
  else
    pcn_out(p0, CODE_ERROR, FORMAT_NO_SUCH_EMOTE);

  return COM_OK;
}


int com_pme(int p0, struct parameter * param)
{
  char *tmp;
  int p1;

  if (param[0].type != TYPE_WORD) return COM_BADPARAMETERS;
  stolower(param[0].val.word);

  if (param[0].val.word[0] == '*') {
    if (parray[p0].forget.last_pzz < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_NO_ONE_TO_TELL_ANYTHING_TO_);
      return COM_OK;
    } else {
      p1 = parray[p0].forget.last_pzz;
      if (param[1].type == TYPE_NULL) {
	pcn_out(p0,CODE_INFO, FORMAT_YOUR_IS_s_, parray[p1].pname);
	return COM_OK;
      }
    }
  } else {
    p1 = player_find_part_login(param[0].val.word);
    if (p1 < 0) {
       pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_,
                   param[0].val.word);
       return COM_OK;
    }
  }

  if (param[1].type == TYPE_NULL) {
    parray[p0].forget.last_pzz = p1;
    pcn_out(p0, CODE_INFO, FORMAT_SETTING_YOUR_s_TO_s, param[0].val.word, parray[p1].pname);
    return COM_OK;
  }

  if (player_censored(p1, p0)) {
    pcn_out(p0, CODE_INFO, FORMAT_PLAYER_qsq_IS_CENSORING_YOU_, parray[p1].pname);
    return COM_OK;
  }
  tmp = eatwhite(param[1].val.string);

  pcn_out_prompt(p1, CODE_CR1|CODE_EMOTE, FORMAT_s_s_n, parray[p0].pname, tmp);
  if (parray[p0].forget.last_pzz != p1) {
    parray[p0].forget.last_pzz = p1;
    pcn_out(p0, CODE_INFO, FORMAT_SETTING_YOUR_s_TO_s, param[0].val.word, parray[p1].pname);
  } else {
    pcn_out(p0, CODE_EMOTETO, FORMAT_s, parray[p1].pname);
  }
  return COM_OK;
}

int com_invite(int p0, struct parameter * param)
{
  int ch, p1;

  if (param[0].type != TYPE_WORD) return COM_BADPARAMETERS;
  

  stolower(param[0].val.word);
  p1 = player_find_part_login(param[0].val.word);
  if (p1 < 0) {
     pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
     return COM_OK;
  }

  if (param[1].type == TYPE_NULL) {
    if (parray[p0].last_channel < 0) {
      return COM_BADPARAMETERS;
    } else {
      ch = parray[p0].last_channel;
    }
  } else {
    if (param[1].val.integer < 0 || param[1].val.integer > MAX_NCHANNEL) {
      return COM_BADPARAMETERS;
    } else {
      ch = param[1].val.integer;
    }
  }

  if (!on_channel(ch, p0)) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_ON_CHANNEL_dn, ch);
    return COM_OK;
  }
  if (on_channel(ch, p1)) {
    pcn_out(p0, CODE_ERROR, FORMAT_s_IS_ALREADY_ON_CHANNEL_dn, parray[p1].pname, ch);
    return COM_OK;
  }


  pprintf_prompt(p1, "%s invites you to channel %d\n", parray[p0].pname, ch);
  pcn_out(p0,CODE_INFO, FORMAT_INVITED_s_TO_CHANNEL_d, parray[p1].pname, ch);
  return COM_OK;
}

int com_me(int p0, struct parameter * param)
{
  int ch, i, p1;
  char buf[MAX_STRING_LENGTH];

  if (parray[p0].last_channel < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_PREVIOUS_CHANNEL_);
    return COM_OK;
  }

  i = my_snprintf(buf, sizeof buf, "-- %s %s --\n",
	   parray[p0].pname, param[0].val.string);
  if (i < 0) return COM_OK;

  ch = parray[p0].last_channel;

  add_to_yell_stack(ch, buf);

  for (i = 0; i < carray[ch].count; i++) {
    p1 = carray[ch].members[i];
    if (!parray[p1].slotstat.is_online) continue;
    if (player_censored(p1, p0)) continue;
    if (p1 == p0 || (parray[p1].last_channel != ch)) {
      pcn_out(p1, CODE_CR1|CODE_INFO, FORMAT_d_s, ch, buf);
    }
    else {
      pcn_out_prompt(p1, CODE_CR1|CODE_INFO, FORMAT_s, buf);
    }
  }
  return COM_OK;
}


static int do_tell(int p0, int p1, const char *msg, int why, int ch)
{
  char tmp[MAX_LINE_SIZE];

  switch(why) {
  case TELL_TELL:
  case TELL_KIBITZ:
    if (!parray[p1].flags.want_tells && !parray[p0].slotstat.is_registered) {
      pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_qsq_ISN_T_LISTENING_TO_UNREGISTERED_TELLS_,
        parray[p1].pname);
      return COM_OK;
    }

    if (parray[p1].i_robot && parray[p0].i_robot) {
      return COM_OK;
    }

    if (player_censored(p1, p0) && parray[p0].adminLevel < ADMIN_ADMIN) {
      pcn_out(p0,CODE_ERROR, FORMAT_PLAYER_qsq_IS_CENSORING_YOU_, parray[p1].pname);
      return COM_OK;
    }

    if (TELL_PLAYER_IN_TMATCH(p0, p1)) {
      pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_s_IS_CURRENTLY_INVOLVED_IN_A_TOURNEMENT_MATCH_n, parray[p1].pname);
      pcn_out(p0, CODE_ERROR, FORMAT_PLEASE_SEND_A_MESSAGE_INSTEAD_);
      return COM_OK;
    }

  case TELL_SAY: /* AvK: always listen to opponent */
  default:
    break;
  case TELL_BEEP:
    if (player_censored(p1, p0) && parray[p0].adminLevel < ADMIN_ADMIN) {
      pcn_out(p0, CODE_ERROR
        , FORMAT_PLAYER_s_IS_CENSORING_YOU_, parray[p1].pname);
      return COM_OK;
    }
    if (parray[p0].bmuzzled) {
      pcn_out(p0, CODE_ERROR, FORMAT_BEEP_OOPS_);
      return COM_OK;
    }
    if (!parray[p0].slotstat.is_registered) {
      pcn_out(p0, CODE_ERROR
        , FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_BEEP_COMMAND_);
      return COM_OK;
    }
    break;
  }

  switch (why) {
  case TELL_TELL:
  default:
    if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_a);
    pcn_out_prompt(p1, CODE_CR1|CODE_TELL, FORMATtell_s_sn, parray[p0].pname, msg);
    if (parray[p1].forget.last_tell != p0) parray[p1].forget.last_tell_from = p0;
    break;
  case TELL_SAY:
    pcn_out_prompt(p1, CODE_SAY, FORMATsay_s_sn, parray[p0].pname, msg);
    break;
  case TELL_KIBITZ:
    pcn_out_prompt(p1, CODE_KIBITZ, FORMATkibitz_sn, msg);
    break;
  case TELL_BEEP:
    if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_a);
    pcn_out_prompt(p1, CODE_CR1|CODE_INFO, FORMAT_s_IS_BEEPING_YOU_n, parray[p0].pname);
    break;
  case TELL_CHANNEL:
    if (parray[p1].flags.is_client) {
      pcn_out_prompt(p1, CODE_YELL, FORMAT_d_s_sn,
            ch, parray[p0].pname, msg);
    } else if (parray[p1].last_channel == ch) {
      pcn_out_prompt(p1, CODE_CR1|CODE_YELL, FORMATchannel_s_sn, parray[p0].pname, msg);
    } else {
      pcn_out_prompt(p1, CODE_CR1|CODE_YELL, FORMATchannel_sd_sn, parray[p0].pname, ch, msg);
    }
    break;
  }

  switch (why) {
  case TELL_BEEP:
  case TELL_TELL:
    if (parray[p1].busy[0]) {
      sprintf(tmp,"  [%s] (idle: %d minutes)",
                   parray[p1].busy, ((player_idle(p1)%3600)/60));
    } else if (((player_idle(p1)%3600)/60) > 2) {
      sprintf(tmp," who has been idle %d minutes", ((player_idle(p1)%3600)/60));
    } else *tmp = 0;

    if (parray[p1].session.gnum >= 0 && (parray[p1].session.gnum != parray[p0].session.gnum)) {
      strcpy(tmp, " (who is playing a game)");
    }
    break;
  }

  switch (why) {
  case TELL_TELL:
    if (parray[p0].forget.last_tell != p1) {
      pcn_out(p0, CODE_INFO, FORMAT_SETTING_YOUR_s_TO_s, ".", parray[p1].pname);
      parray[p0].forget.last_tell = p1;
    }
    if (*tmp) pcn_out(p0, CODE_DOT, FORMAT_ss, ",", tmp);
    break;
  case TELL_BEEP:
    pcn_out(p0, CODE_INFO, FORMAT_BEEPED_s_S_CONSOLEs_, parray[p1].pname, tmp);
    break;
  }
  return COM_OK;
}

#if 0
static int do_beep(int p0, int p1)
{
  char tmp[MAX_LINE_SIZE];

  if (player_censored(p1, p0) && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_s_IS_CENSORING_YOU_, parray[p1].pname);
    return COM_OK;
  }
  if (parray[p0].bmuzzled) {
    pcn_out(p0, CODE_ERROR, FORMAT_BEEP_OOPS_);
    return COM_OK;
  }
  if (!parray[p0].slotstat.is_registered) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONLY_REGISTERED_PLAYERS_CAN_USE_THE_BEEP_COMMAND_
    );
    return COM_OK;
  }

  if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_a);
  pcn_out_prompt(p1, CODE_CR1|CODE_INFO, FORMAT_s_IS_BEEPING_YOU_n, parray[p0].pname);
  if (parray[p1].busy[0]) {
    sprintf(tmp,"beeped %s's console who %s (idle: %s)", parray[p1].pname, parray[p1].busy, secs2hms_mask(player_idle(p1), 6));
  } else {
    if (((player_idle(p1)%3600)/60) > 2) {
      sprintf(tmp,"%s has been idle %s", parray[p1].pname, secs2hms_mask(player_idle(p1), 6));
    }
    else sprintf(tmp,"beeped %s's console.", parray[p1].pname);
  }
  pcn_out(p0, CODE_INFO, FORMAT_s, tmp);
  return COM_OK;
}
#endif

static int chtell(int p0, int ch, char *msg)
{
  int p1, ii;
  char text[MAX_STRING_LENGTH];

  if (ch == 0 && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_INVALID_CHANNEL_NUMBER_);
    return COM_OK;
  }
  if (ch < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_THE_LOWEST_CHANNEL_NUMBER_IS_1_);
    return COM_OK;
  }
  if (ch >= MAX_NCHANNEL) {
    pcn_out(p0, CODE_ERROR, FORMAT_THE_MAXIMUM_CHANNEL_NUMBER_IS_d_, MAX_NCHANNEL - 1);
    return COM_OK;
  }
  if (carray[ch].is_private && !on_channel(ch, p0)) {
    pcn_out(p0, CODE_ERROR, FORMAT_THE_USERS_OF_CHANNEL_d_WOULD_PREFER_YOU_TO_BE_IN_THAT_CHANNEL_BEFOREn, ch);
    pcn_out(p0, CODE_ERROR, FORMAT_SPEAKING_IN_IT_SEE_qHELP_CHANNELq );
    return COM_OK;
  }
  for (ii = 0; ii < carray[ch].count; ii++) {
    p1 = carray[ch].members[ii];
    if (!parray[p1].slotstat.is_online) continue;
    if (p1 == p0) continue;
    if (player_censored(p1, p0)) continue;
    if (!parray[p1].flags.want_tells) continue;
    do_tell(p0, p1, msg, TELL_CHANNEL, ch);
  }
  if (ch != parray[p0].last_channel) {
    pcn_out(p0,CODE_INFO, FORMAT_SETTING_YOUR_TO_CHANNEL_dn, ch);
    parray[p0].last_channel = ch;
  } else {
    pcn_out(p0,CODE_INFO, FORMAT_d_, ch);
  }
  sprintf(text, "<%s> %s\n", parray[p0].pname, msg);
  add_to_yell_stack(ch, text);
  return COM_OK;
}

int com_kibitz(int p0, struct parameter * param)
{
  int g0, ii, g2;
  int p1, bp, wp;
  int count = 0;
  char tmp2[MAX_LINE_SIZE];
  char tmp3[MAX_LINE_SIZE];

  if (parray[p0].muzzled) {
    return COM_OK;
  }
  if (parray[p0].session.num_observe == 0 && parray[p0].session.gnum < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_OR_OBSERVING_A_GAME_);
    return COM_OK;
  }
  if (param[0].type == TYPE_NULL) return COM_BADPARAMETERS;
  if (param[0].type == TYPE_INT) {
    if (param[0].val.integer == 0) return COM_BADPARAMETERS;
    g0 = param[0].val.integer - 1;
    for (ii = 0; ii < parray[p0].session.num_observe; ii++) {
      if (g0 == parray[p0].session.observe_list[ii]) {
        count = 1;
        continue;
      }
    }
    if (!count && g0 != parray[p0].session.gnum) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_PLAYING_OR_OBSERVING_THAT_GAME_);
      return COM_OK;
    }
  }

  else {
    if (parray[p0].session.gnum >= 0) g0 = parray[p0].session.gnum;
    else g0 = parray[p0].session.observe_list[0];
  }
  if (param[0].type == TYPE_INT) {
    sprintf(tmp2, "%s (%d)", param[1].type == TYPE_NULL
            ? "" : param[1].val.string, mink_movenum(garray[g0].minkg));
  } else {
    sprintf(tmp2, "%s %s (%d)", param[0].val.word,
            param[1].type == TYPE_NULL
            ? "" : param[1].val.string, mink_movenum(garray[g0].minkg));
  }

  bp = garray[g0].black.pnum;
  wp = garray[g0].white.pnum;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_online) continue;
    if (p1 == p0) continue;
    if (player_is_observe(p1, g0)) {
      if (player_censored(p1, p0)) continue;
      if (parray[p1].flags.is_client && parray[p1].flags.want_bell)
        pcn_out(p1, CODE_BEEP, FORMAT_nn);
      pcn_out(p1, CODE_KIBITZ, FORMAT_KIBITZ_s_ss_GAME_s_VS_s_d_n,
 		   parray[p0].pname,
 		   parray[p0].srank,
 		   parray[p0].flags.is_rated ? "*" : " ",
		   parray[wp].pname,
		   parray[bp].pname,
		   g0 + 1);
      do_tell(p0, p1, tmp2, TELL_KIBITZ, 0);
    }
#if WANT_PAIR
    if (paired(g0)) {
      g2 = garray[g0].pairwith;
      if (player_is_observe(p1, g2)) {
        if (player_censored(p1, p0)) continue;
        if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP,FORMAT_nn);
        pcn_out(p1, CODE_KIBITZ, FORMAT_KIBITZ_s_ss_GAME_s_VS_s_d_n,
 		   parray[p0].pname,
 		   parray[p0].srank,
 		   parray[p0].flags.is_rated ? "*" : " ",
		   parray[wp].pname,
		   parray[bp].pname,
		   g2 + 1);
        do_tell(p0, p1, tmp2, TELL_KIBITZ, 0);
      }
    }
#endif
  }
  if (garray[g0].teach == 2) {
    p1 = wp;
    pcn_out(p1, CODE_KIBITZ, FORMAT_KIBITZ_s_ss_GAME_s_VS_s_d_n,
                   parray[p0].pname,
                   parray[p0].srank,
                   parray[p0].flags.is_rated ? "*" : " ",
                   parray[wp].pname,
                   parray[bp].pname,
                   g0 + 1);
      do_tell(p0, p1, tmp2, TELL_KIBITZ, 0);
  }
  if (garray[g0].teach) {
    p1 = bp;
    pcn_out(p1, CODE_KIBITZ, FORMAT_KIBITZ_s_ss_GAME_s_VS_s_d_n,
                   parray[p0].pname,
                   parray[p0].srank,
                   parray[p0].flags.is_rated ? "*" : " ",
                   parray[wp].pname,
                   parray[bp].pname,
                   g0 + 1);
      do_tell(p0, p1, tmp2, TELL_KIBITZ, 0);
  }

  sprintf(tmp3, " %s %s%s: %s",
                parray[p0].pname,
                parray[p0].srank,
                parray[p0].flags.is_rated ? "*" : " ",
                tmp2);
  add_kib(&garray[g0], mink_movenum(garray[g0].minkg), tmp3);
#if WANT_PAIR
  if (paired(g0)) {
    g2 = garray[g0].pairwith;
    add_kib(&garray[g2], mink_movenum(garray[g2].minkg), tmp3);
  }
#endif
  return COM_OK;
}

int com_beep(int p0, struct parameter * param)
{
  int p1;

  if (param[0].type != TYPE_WORD) return COM_BADPARAMETERS;
   
  stolower(param[0].val.word);
  p1 = player_find_part_login(param[0].val.word);
  if (p1 < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
    return COM_OK;
  }
  return do_tell(p0, p1, "Dummy" , TELL_BEEP, 0);
}

int com_tell(int p0, struct parameter * param)
{
  int p1;
  int what;

  switch (param[0].type ) {
  default:
  case TYPE_NULL:
    return COM_BADPARAMETERS;
  case TYPE_INT:
      p1 = param[0].val.integer;
      what = ',';
      break;
  case TYPE_WORD:
    stolower(param[0].val.word);
    what=param[0].val.word[0];
    switch(what) {
    case '.':
      p1 = parray[p0].forget.last_tell;
      break;
    case '^':
      p1 = parray[p0].forget.last_tell_from;
      break;
    case ',':
      p1 =parray[p0].last_channel;
      break;
    default: what= 'p';
      stolower(param[0].val.word);
      p1 = player_find_part_login(param[0].val.word);
      break;
    }
    break;
  }

  if (p1 < 0) switch(what) {
  case '.':
  case '^':
    pcn_out(p0, CODE_ERROR, FORMAT_NO_ONE_TO_TELL_ANYTHING_TO_);
    return COM_OK;
  case ',':
    pcn_out(p0, CODE_ERROR, FORMAT_NO_PREVIOUS_CHANNEL_);
    return COM_OK;
  case 'p':
    pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
    return COM_OK;
  }

  switch(what) {
  case 'p':
  case '.':
  case '^':
    if ( parray[p0].forget.last_tell != p1) {
      pcn_out(p0, CODE_INFO, FORMAT_SETTING_YOUR_s_TO_s, ".", parray[p1].pname);
      parray[p0].forget.last_tell = p1;
    }
    break;
  case ',':
    if ( parray[p0].last_channel != p1) {
      pcn_out(p0, CODE_INFO, FORMAT_YOUR_IS_CHANNEL_d_, p1);
      parray[p0].last_channel = p1;
    }
    break;
  }

  if (param[1].type != TYPE_STRING) return COM_OK;

  switch(what) {
  case 'p':
  case '.':
  case '^':
    return do_tell(p0, p1, param[1].val.string, TELL_TELL, 0);
  case ',':
    return chtell(p0, p1, param[1].val.string);
  }
  return COM_OK;
}

int com_say(int p0, struct parameter * param)
{
  int g0 = parray[p0].session.gnum;
  char tmp[MAX_LINE_SIZE];

  if (parray[p0].session.opponent < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_ONE_TO_SAY_ANYTHING_TO_TRY_TELL_);
    return COM_OK;
  }
  sprintf(tmp, " %s %s%s: %s",
                 parray[p0].pname,
                 parray[p0].srank,
                 parray[p0].flags.is_rated ? "*" : " ",
                 param[0].val.string);
  add_kib(&garray[g0], mink_movenum(garray[g0].minkg), tmp);

  return do_tell(p0, parray[p0].session.opponent, param[0].val.string, TELL_SAY, 0);
}

int com_set(int p0, struct parameter * param)
{
  int result;
  int which;
  char *val;

  val= (param[1].type == TYPE_NULL) ? NULL : param[1].val.string;
  result = var_set(p0, param[0].val.word, val, &which);
  switch (result) {
  case VAR_OK:
    player_dirty(p0);
    break;
  case VAR_BADVAL:
    pcn_out(p0, CODE_ERROR, FORMAT_BAD_VALUE_GIVEN_FOR_VARIABLE_s_, param[0].val.word);
    break;
  case VAR_NOSUCH:
    pcn_out(p0, CODE_ERROR, FORMAT_UNKNOWN_VALUE_FOR_TOGGLING_, param[0].val.word);
    break;
  case VAR_AMBIGUOUS:
    pcn_out(p0, CODE_ERROR, FORMAT_AMBIGUOUS_VARIABLE_NAME_s_, param[0].val.word);
    break;
  }
  return COM_OK;
}

int com_stats(int p0, struct parameter * param)
{
  int p1;
  time_t tt;
  const struct ladderplayer *lp;

  if (param[0].type == TYPE_WORD) {
    p1 = player_find_sloppy(param[0].val.word);
    if (p1<0) {
      pcn_out(p0, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_MATCHING_THAT_NAME_n);
      return COM_OK;
    }
  } else {
    p1 = p0;
    player_fix(p1);
  }
  pcn_out(p0, CODE_INFO, FORMAT_PLAYER_sn , parray[p1].pname);
  pcn_out(p0, CODE_INFO, FORMAT_GAME_GO_1_n );
  pcn_out(p0, CODE_INFO, FORMAT_RATING_ss_dn,
          parray[p1].srank, parray[p1].flags.is_rated ? "*" : " ", parray[p1].rating);
  pcn_out(p0, CODE_INFO, FORMAT_RATED_GAMES_dn, parray[p1].numgam );
  pcn_out(p0, CODE_INFO, FORMAT_RANK_s_dn, parray[p1].ranked, parray[p1].orating);
  pcn_out(p0, CODE_INFO, FORMAT_WINS_dn, parray[p1].gowins);
  pcn_out(p0, CODE_INFO, FORMAT_LOSSES_dn, parray[p1].golose);
  if (!parray[p1].slotstat.is_online) {
    tt = player_lastdisconnect(p1);
    pcn_out(p0, CODE_INFO, FORMAT_LAST_ACCESS_GMT_NOT_ON_sn,
                tt ? time2str_utc(&tt) : "Never connected.");
    pcn_out(p0,CODE_INFO,  FORMAT_LAST_ACCESS_LOCAL_NOT_ON_sn,
                tt ? time2str_local(&tt) : "Never connected.");
  }
  else {
    pcn_out(p0,CODE_INFO,  FORMAT_IDLE_TIME_ON_SERVER_sn,
                secs2hms_mask(player_idle(p1), 7));
    if (parray[p1].session.gnum >= 0)
      pcn_out(p0,CODE_INFO,  FORMAT_PLAYING_IN_GAME_d_I_n,
                (parray[p1].session.gnum) + 1);
    else if (parray[p1].session.num_observe > 0)
      pcn_out(p0,CODE_INFO,  FORMAT_OBSERVING_GAME_dn,
                (parray[p1].session.observe_list[0]) + 1);
  }

  if ((parray[p0].adminLevel >= ADMIN_ADMIN
    || !parray[p1].slotstat.is_registered) &&
       parray[p1].slotstat.is_online) {
    pcn_out(p0,CODE_INFO,  FORMAT_ADDRESS_s_s_n,
          (parray[p1].email[0] ? parray[p1].email : "UNREGISTERED"),
	    dotQuad(parray[p1].slotstat.is_online ? parray[p1].thisHost : parray[p1].lastHost));
  }
  else if (parray[p0].adminLevel >= ADMIN_ADMIN && !parray[p1].slotstat.is_online) {
    pcn_out(p0,CODE_INFO,  FORMAT_ADDRESS_s_LAST_CON_FROM_s_n,
          parray[p1].email,
          dotQuad(parray[p1].lastHost));
  }
  else pcn_out(p0,CODE_INFO,  FORMAT_ADDRESS_sn,
          (parray[p1].email[0] ? parray[p1].email : "UNREGISTERED"));
  if (parray[p1].slotstat.is_registered)
  pcn_out(p0,CODE_INFO, FORMAT_REG_DATE_sn, parray[p1].RegDate);
  if ((lp = ladder_player_named(Ladder9, parray[p1].pname))) {
    if (lp->idx == 0) {
      pcn_out(p0,CODE_INFO, FORMAT_LADDER9_POSITION_NUMBER_ONEn);
    } else {
      pcn_out(p0,CODE_INFO,  FORMAT_LADDER9_POSITION_dn,
                  (lp->idx) +1);
    }
  }
  if ((lp = ladder_player_named(Ladder19, parray[p1].pname)) ) {
    if (lp->idx == 0) {
      pcn_out(p0,CODE_INFO, FORMAT_LADDER19_POSITION_NUMBER_ONEn);
    } else {
      pcn_out(p0,CODE_INFO,  FORMAT_LADDER19_POSITION_dn,
                  (lp->idx) +1);
    }
  }
  if (parray[p1].session.gnum >= 0) {
    int g0 = parray[p1].session.gnum;
    int bp = garray[g0].black.pnum;
    int wp = garray[g0].white.pnum;
    pcn_out(p0,CODE_INFO, FORMAT_PLAYING_GAME_d_s_VS_s_n, g0 + 1,
      parray[wp].pname, parray[bp].pname );
  }

  if (parray[p1].busy[0] && parray[p1].slotstat.is_online) {
    pcn_out( p0,CODE_INFO,  FORMAT_BUSY_s_n, parray[p1].busy );
  }

  if (!parray[p1].slotstat.is_registered) {
    pcn_out(p0,CODE_INFO,  FORMAT_UNREG_s_IS_NOT_A_REGISTERED_PLAYER_,
                parray[p1].pname);
  } else {
    if (parray[p1].rank) {
      pcn_out(p0,CODE_INFO, FORMAT_RANK_INFO_sn, parray[p1].rank);
    }
    pcn_out(p0,CODE_INFO,  FORMAT_GAMES_AS_B_d_GAMES_AS_W_dn,
                   parray[p1].gonum_black,
                   parray[p1].gonum_white);
  }
  if (parray[p1].adminLevel >= ADMIN_ADMIN
    && parray[p0].adminLevel >= ADMIN_ADMIN) {
    pcn_out(p0,CODE_INFO, FORMAT_ADMIN_LEVEL_ADMINISTRATOR_n);
  }

  if (parray[p1].slotstat.is_registered)
    pcn_out(p0,CODE_INFO, FORMAT_FULL_NAME_s, parray[p1].fullname);

  if (parray[p0].adminLevel >= ADMIN_ADMIN) {
    pcn_out(p0, CODE_INFO, FORMAT_n );
    pcn_out(p0, CODE_INFO, FORMAT_MUZZLED_s_GMUZZLED_s_BMUZZLED_s_TMUZZLED_s_KMUZZLED_s,
            parray[p1].muzzled ? "Yes" : "No",
            parray[p1].gmuzzled ? "Yes" : "No",
            parray[p1].bmuzzled ? "Yes" : "No",
            parray[p1].tmuzzled ? "Yes" : "No",
            parray[p1].kmuzzled ? "Yes" : "No");
  }

#if 1
  {
    char *pl;

    plan_start(parray[p1].plan_lines);
    while (plan_next(&pl, parray[p1].plan_lines)) {
      pcn_out(p0,CODE_CR1|CODE_INFO, FORMAT_INFO_s, pl);
    }
  }
#else
  {
    int i;

    if (parray[p1].num_plan) {
      for (i = 0; i < parray[p1].num_plan; i++) {
	pcn_out(p0,CODE_CR1|CODE_INFO, FORMAT_INFO_s,
		IFNULL(parray[p1].planLines[i], "") );
      }
    }
  }
#endif
  if (p0 == p1) {
    pcn_out(p0, CODE_CR1|CODE_INFO, FORMAT_SEE_ALSO_qVARIABLES_sq, parray[p1].pname);
  }
  player_unfix(p1);
  return COM_OK;
}

int com_variables(int p0, struct parameter * param)
{
  int p1;
  int i;
  int count = 0;

  if (param[0].type == TYPE_WORD) {
    p1 = player_find_sloppy(param[0].val.word);
    if (p1<0) {
      pcn_out(p0, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_MATCHING_THAT_NAME_n);
      return COM_OK;
      }
  } else {
    p1 = p0;
    player_fix(p1);
  }
  if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
  pprintf(p0, "Variable settings of %s:\n", parray[p1].pname);
  pprintf(p0, "\
go shouts (gshout)       = %-3.3s       kibitz (kibitz)              = %-3.3s\n\
shout (shout)            = %-3.3s       open (open)                  = %-3.3s\n\
bell (bell)              = %-3.3s       tell (tell)                  = %-3.3s\n\
robot (robot)            = %-3.3s       player inform (pin)          = %-3.3s\n\
looking (looking)        = %-3.3s       verbose (verbose)            = %-3.3s\n\
private (private)        = %-3.3s       ropen (ropen)                = %-3.3s\n\
automail (automail)      = %-3.3s       game inform (gin)            = %-3.3s\n\
client (client)          = %-3.3s       ladder shouts (lshout)       = %-3.3s\n\
time (time)              = %2.2d        byo-yomi stones (byo_stones) = %2d\n\
size (size)              = %2.2d        notifiedby (notified)        = %d\n\
width (width)            = %2.2d        height (height)              = %2d\n\
byo-yomi time (byo_time) = %2.2d        Problem number               = %d\n\
Water Balloons           = %2.2d        Extended Prompt (extprompt)  = %-3.3s\n\
\n",
	parray[p1].i_gshout ? "Yes" : "No",
        parray[p1].i_kibitz ? "Yes" : "No",
	parray[p1].i_shout ? "Yes" : "No",
        parray[p1].flags.is_open ? "Yes" : "No",
	parray[p1].flags.want_bell ? "Yes" : "No",
        parray[p1].flags.want_tells ? "Yes" : "No",
	parray[p1].i_robot ? "Yes" : "No",
        parray[p1].flags.want_logins ? "Yes" : "No",
        parray[p1].flags.is_looking ? "Yes" : "No",
        parray[p1].i_verbose ? "Yes" : "No",
        parray[p1].Private ? "Yes" : "No",
        parray[p1].ropen ? "Yes" : "No",
        parray[p1].automail ? "Yes" : "No",
        parray[p1].flags.want_gshouts ? "Yes" : "No",
        parray[p1].flags.is_client ? "Yes" : "No",
        parray[p1].i_lshout ? "Yes" : "No",
        parray[p1].def_time,
        parray[p1].def_byo_stones,
        parray[p1].def_size,
        parray[p1].forget.notifiedby,
	parray[p1].d_width,
        parray[p1].d_height,
        parray[p1].def_byo_time,
        parray[p1].last_problem,
        parray[p1].water,
        parray[p1].extprompt ? "Yes" : "No");

  pprintf(p0, "Prompt: %s\n", parray[p1].prompt);
  for (i=0; i < MAX_NCHANNEL; i++) {
    if (on_channel(i, p1)) {
      if (!count) pprintf( p0, "Channels:");
      pprintf( p0, " %d", i );
      count++;
    }
  }
  if (count) pprintf( p0, "\n" );
  if (parray[p1].last_channel >= 0) {
    pprintf(p0, "Last Channel: (;) %d\n",
                parray[p1].last_channel);
  }
  if (parray[p1].forget.last_tell >= 0
    && (p0 == p1 || parray[p0].adminLevel >= ADMIN_ADMIN)) {
    pprintf(p0, "Last Tell: (.) %s\n",
                parray[parray[p1].forget.last_tell].pname);
  }
  if (alias_count(parray[p1].alias_list) && p0 == p1) {
    char *c, *a;

    pprintf(p0, "Aliases:\n");
    alias_start(parray[p1].alias_list);
    while (alias_next(&c, &a, parray[p1].alias_list)) {
      pprintf(p0, "      %s %s\n", c, a);
    }
  }

  if (parray[p0].adminLevel >= ADMIN_ADMIN) {
    pprintf(p0, "Socket: %d p: %d\n", parray[p1].session.socket, p1);
  }

  if (parray[p1].adminLevel >= ADMIN_ADMIN) {
    pprintf(p0, "AdminLevel:       %d\n", parray[p1].adminLevel);
  }

  pcn_out(p0, CODE_NONE, FORMAT_LANGUAGE_dn,
    parray[p1].language);

  if (parray[p1].slotstat.is_registered) {
  pprintf(p0, "Client Type: ");
  switch(parray[p1].which_client) {
    case CLIENT_UNKNOWN: pprintf(p0, "Unknown\n"); break; 
    case CLIENT_TELNET:  pprintf(p0, "Telnet\n"); break; 
    case CLIENT_XIGC:    pprintf(p0, "Xigc\n"); break; 
    case CLIENT_WINIGC:  pprintf(p0, "WinIGC\n"); break; 
    case CLIENT_WIGC:    pprintf(p0, "WIGC\n"); break; 
    case CLIENT_CGOBAN:  pprintf(p0, "CGoban\n"); break; 
    case CLIENT_JAVA:    pprintf(p0, "Java\n"); break; 
    case CLIENT_TGIGC:   pprintf(p0, "Tgigc\n"); break; 
    case CLIENT_TGWIN:   pprintf(p0, "TgWin\n"); break; 
    case CLIENT_FIGC:    pprintf(p0, "Figc\n"); break; 
    case CLIENT_PCIGC:   pprintf(p0, "PCigc\n"); break; 
    case CLIENT_GOSERVANT: pprintf(p0, "GoServant\n"); break; 
    case CLIENT_MACGO:   pprintf(p0, "MacGo\n"); break; 
    case CLIENT_AMIGAIGC: pprintf(p0, "AmigaIgc\n"); break; 
    case CLIENT_HAICLIENT: pprintf(p0, "Hai Client\n"); break; 
    case CLIENT_IGC:     pprintf(p0, "IGC\n"); break; 
    case CLIENT_KGO:     pprintf(p0, "Kgo\n"); break; 
    case CLIENT_NEXTGO:  pprintf(p0, "NextGo\n"); break; 
    case CLIENT_OS2IGC:  pprintf(p0, "OS/2igc\n"); break; 
    case CLIENT_STIGC:   pprintf(p0, "StIgc\n"); break; 
    case CLIENT_XGOSPEL: pprintf(p0, "XGospel\n"); break; 
    case CLIENT_TKGC:    pprintf(p0, "TkGc\n"); break; 
    case CLIENT_JAGOCLIENT: pprintf(p0, "JagoClient\n"); break; 
    case CLIENT_GTKGO:   pprintf(p0, "gtkgo\n"); break; 
    default:             pprintf(p0, "Unknown\n"); break; }
  }

  if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);

  player_unfix(p1);
  return COM_OK;
}


int com_password(int p, struct parameter * param)
{
  char *oldpasswd = param[0].val.word;
  char *newpasswd = param[1].val.word;
  char salt[3];

  if (!parray[p].slotstat.is_registered) {
    pcn_out(p, CODE_ERROR, FORMAT_SETTING_A_PASSWORD_IS_ONLY_FOR_REGISTERED_PLAYERS_);
    return COM_OK;
  }
  if (parray[p].passwd) {
    memcpy(salt, parray[p].passwd, 2);
    salt[2]=0;

    if (strcmp(mycrypt(oldpasswd, salt), parray[p].passwd)) {
      pcn_out(p, CODE_ERROR, FORMAT_INCORRECT_PASSWORD_PASSWORD_NOT_CHANGED_);
      return COM_OK;
    }
    parray[p].passwd[0] = '\0';
  }
  salt[0] = 'a' + rand() % 26;
  salt[1] = 'a' + rand() % 26;
  salt[2] = '\0';
  do_copy(parray[p].passwd, mycrypt(newpasswd, salt), sizeof parray[0].passwd);
  pcn_out(p,CODE_INFO, FORMAT_PASSWORD_CHANGED_TO_qsq_, newpasswd);
  player_dirty(p);
  return COM_OK;
}

int com_uptime(int p, struct parameter * param)
{
  time_t uptime, now;
  const struct ladderplayer *lp;
  unsigned int count;
  UNUSED(param);

  now = globclock.time;
  uptime = now - startuptime;

  pcn_out(p, CODE_INFO, FORMAT_SERVER_NAME_sn, conffile.server_name);
  pcn_out(p, CODE_INFO, FORMAT_SERVER_ADDRESS_sn, conffile.server_address);
  pcn_out(p, CODE_INFO, FORMAT_SERVER_VERSION_sn, conffile.version_string);

  pcn_out(p, CODE_INFO, FORMAT_CURRENT_TIME_GMT_sn, time2str_utc(&now));
  pcn_out(p, CODE_INFO, FORMAT_CURRENT_LOCAL_TIME_sn, time2str_local(&now));
  pcn_out(p, CODE_INFO, FORMAT_THE_SERVER_HAS_BEEN_RUNNING_SINCE_s_GMT_n,
                          time2str_local(&startuptime));

  /* Does this break any clients? */
  if (uptime > 86400)
    pcn_out(p, CODE_INFO, FORMAT_SERVER_UPTIME_u_DAYS_sn, 
	    uptime/86400, secs2hms_long(uptime%86400));
  else
    pcn_out(p, CODE_INFO, FORMAT_SERVER_UPTIME_sn, secs2hms_long(uptime));

  pcn_out(p, CODE_INFO, FORMAT_PLAYER_LIMIT_dn, net_fd_count);
  pcn_out(p, CODE_INFO, FORMAT_MOVE_LIMIT_2_147_483_648n);
  pcn_out(p, CODE_INFO, FORMAT_GAMES_PLAYED_SINCE_RESTART_dn, completed_games);
  pcn_out(p, CODE_INFO, FORMAT_LOGINS_u_LOGOUTS_u_NEW_PLAYERS_un, 
              num_logins, num_logouts, new_players);
  count = player_count();
  pcn_out(p, CODE_INFO, FORMAT_THERE_ARE_CURRENTLY_u_PLAYERS_WITH_A_HIGH_OF_u_SINCE_LAST_RESTART_n, count, player_high);
  count = game_count();
  pcn_out(p, CODE_INFO, FORMAT_THERE_ARE_CURRENTLY_u_GAMES_WITH_A_HIGH_OF_u_SINCE_LAST_RESTART_n, count, game_high);

  pcn_out(p, CODE_INFO, FORMAT_CONNECTED_TO_sn, conffile.server_name);
  pcn_out(p, CODE_INFO, FORMAT_BYTES_SENT_un, bytes_sent);
  pcn_out(p, CODE_INFO, FORMAT_BYTES_RECEIVED_un, bytes_received);

  lp = ladder_player_at(Ladder9, 0);
  if (lp) pcn_out(p, CODE_INFO, FORMAT_u_PLAYERS_IN_9X9_LADDER_PLAYER_s_IS_1_n, 
              num_9, lp->name);
  else pcn_out(p, CODE_INFO, FORMAT_PROBLEM_WITH_9X9_LADDER_STRUCTURE_n);

  lp = ladder_player_at(Ladder19, 0);
  if (lp) pcn_out(p
                 , CODE_INFO, FORMAT_u_PLAYERS_IN_19X19_LADDER_PLAYER_s_IS_1_n
                 , num_19, lp->name);
  else pcn_out(p, CODE_INFO, FORMAT_PROBLEM_WITH_19X19_LADDER_STRUCTURE_n);

  if (conffile.server_http && *conffile.server_http)
    pcn_out(p, CODE_INFO, FORMAT_SEE_s_FOR_MORE_INFORMATION_n
           , conffile.server_http);
  return COM_OK;
}

int com_date(int p, struct parameter * param)
{
  time_t tt = globclock.time;
  UNUSED(param);

  pcn_out(p,CODE_INFO, FORMAT_LOCAL_TIME_sn, time2str_local(&tt));
  pcn_out(p,CODE_INFO, FORMAT_GREENWICH_TIME_sn, time2str_utc(&tt));
  return COM_OK;
}


int plogins(int p, FILE *fp)
{
  int inout, registered;
  time_t thetime;
  char loginName[MAX_LOGIN_NAME + 1];
  char ipstr[50];
  const char *inout_string[] = { "login", "logout" };
  char buff[MAX_LINE_SIZE];

  if (!fp) {
    pcn_out(p,CODE_ERROR, FORMAT_SORRY_NO_LOGIN_INFORMATION_AVAILABLE_);
    return COM_OK;
  }

  while (fgets(buff, sizeof buff, fp)) {
    if (sscanf(buff, "%d %s %d %d %s", &inout, loginName, (int*) &thetime,
                &registered, ipstr) != 5) {
      Logit("Error in login info format. \"%s\"", filename() );
      fclose(fp);
      return COM_OK;
    }

    pcn_out(p, CODE_INFO, FORMAT_s_s_s,
               time2str_local(&thetime), loginName, inout_string[inout]);

    if (parray[p].adminLevel >= ADMIN_ADMIN) {
      pprintf( p, " from %s\n", ipstr );
    } else  pprintf( p, "\n" );
  }
  fclose(fp);
  return COM_OKN;
}

int com_llogons(int p, struct parameter * param)
{
  FILE *fp;
  UNUSED(param);

  fp = xyfopen(FILENAME_LOGONS, "r");

  return plogins(p, fp);
}

int com_logons(int p, struct parameter * param)
{
  FILE *fp;

  if (param[0].type == TYPE_WORD) {
    fp = xyfopen(FILENAME_PLAYER_cs_LOGONS, "r", param[0].val.word);
  } else {
    fp = xyfopen(FILENAME_PLAYER_cs_LOGONS, "r", parray[p].login);
  }
  return plogins(p, fp);
}

#define WHO_OPEN 0x01
#define WHO_CLOSED 0x02
#define WHO_RATED 0x04
#define WHO_UNRATED 0x08
#define WHO_FREE 0x10
#define WHO_LOOKING 0x20
#define WHO_REGISTERED 0x40
#define WHO_UNREGISTERED 0x80

static int who_ok(int p, unsigned sel_bits, int from, int to)
{
  if (!parray[p].slotstat.is_inuse) return 0;
  if (!parray[p].slotstat.is_online) return 0;

  if (from < 1 && sel_bits == 0xff) {
    return 1;
  }

  if (sel_bits == 0xff && from > 0) {
    if (parray[p].orating >= from && parray[p].orating <= to) return 1;
  }

  if (from > 0) {
    if (parray[p].orating < from || parray[p].orating > to) return 0;
    if ((sel_bits & WHO_OPEN) && (!parray[p].flags.is_open || parray[p].session.gnum >= 0)) return 0;
    if ((sel_bits & WHO_CLOSED) && parray[p].flags.is_open) return 0;
    if ((sel_bits & WHO_RATED) && !parray[p].flags.is_rated) return 0;
    if ((sel_bits & WHO_UNRATED) && parray[p].flags.is_rated) return 0;
    if ((sel_bits & WHO_FREE) && parray[p].session.gnum >= 0) return 0;
    if ((sel_bits & WHO_LOOKING) && !parray[p].flags.is_looking) return 0;
    if ((sel_bits & WHO_REGISTERED) && !parray[p].slotstat.is_registered) return 0;
    if ((sel_bits & WHO_UNREGISTERED) && parray[p].slotstat.is_registered) return 0;
  }
  else {
    if ((sel_bits & WHO_OPEN) && (!parray[p].flags.is_open || parray[p].session.gnum >= 0)) return 0;
    if ((sel_bits & WHO_CLOSED) && parray[p].flags.is_open) return 0;
    if ((sel_bits & WHO_RATED) && !parray[p].flags.is_rated) return 0;
    if ((sel_bits & WHO_UNRATED) && parray[p].flags.is_rated) return 0;
    if ((sel_bits & WHO_FREE) && parray[p].session.gnum >= 0) return 0;
    if ((sel_bits & WHO_LOOKING) && !parray[p].flags.is_looking) return 0;
    if ((sel_bits & WHO_REGISTERED) && !parray[p].slotstat.is_registered) return 0;
    if ((sel_bits & WHO_UNREGISTERED) && parray[p].slotstat.is_registered) return 0;
  }
  return 1;
}

int com_awho(int p, struct parameter * param)
{
  int style = 0;
  int *sortarray = sort_ladder19;
  unsigned int sel_bits = 0;

  int plist[256];
  int i, len;

  char c;
  int p1, count, num_who;


  if (param[0].type == TYPE_WORD) {
    len = strlen(param[0].val.word);
    for (i = 0; i < len; i++) {
      c = param[0].val.word[i];
      switch (c) {
        case 'o':
            sel_bits |= WHO_OPEN;
          break;
        case 'r':
        case '*':
            sel_bits |= WHO_RATED;
          break;
        case 'l':
            sel_bits |= WHO_LOOKING;
          break;
        case 'f':
            sel_bits |= WHO_FREE;
          break;
        case 'R':
            sel_bits |= WHO_REGISTERED;
          break;
        case 'A':               /* Sort order */
          sortarray = sort_alpha;
          break;
        case '9':               /* Sort order */
          sortarray = sort_ladder9;
          break;
        case 't':               /* format */
          style = 0;
          break;
        case 'a':               /* format */
          style = 1;
          break;
        case 'U':
            sel_bits |= WHO_UNREGISTERED;
          break;
        default:
          return COM_BADPARAMETERS;
          break;
        }
    }
  }
  if (!sel_bits) sel_bits = 0xff;
  num_who = count = 0;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!who_ok(sortarray[p1], sel_bits, 0, 0)) continue;
    plist[num_who++] = sortarray[p1];
    count++;
  }

  if (!num_who) {
    pcn_out(p,CODE_ERROR, FORMAT_THERE_ARE_NO_PLAYERS_LOGGED_IN_THAT_MATCH_YOUR_FLAG_SET_ );
    return COM_OK;
  }
  switch (style) {
  case 0:                       /* terse */
  case 1:
    player_resort();
    a_who(p, num_who, plist);
    break;
  default:
    return COM_BADPARAMETERS;
    break;
  }
  return COM_OKN;
}

static void a_who(int p, int cnt, int *plist)
{
  int idx, g0, p1, pos9, pos19;
  const struct ladderplayer *lp9;
  const struct ladderplayer *lp19;
  char line[180];	
  char rtemp[8], otemp[4], gtemp[4], flags[4];

  pcn_out(p,CODE_INFO, FORMAT_INFO_NAME_RANK_19_9_IDLE_RANK_INFOn);
  pcn_out(p,CODE_INFO, FORMAT_n);
  for(idx = 0; idx < cnt; idx++) {
    p1 = plist[idx];
    if (!parray[p1].slotstat.is_inuse) continue;
    if (!parray[p1].slotstat.is_online) continue;
    lp19 = ladder_player_named(Ladder19, parray[p1].pname);
    lp9 = ladder_player_named(Ladder9, parray[p1].pname);
    pos9 = (lp9) ? lp9->idx + 1 : 0;
    pos19 = (lp19) ? lp19->idx + 1 : 0;
    flags[0] = parray[p1].i_shout ? ' ' : 'S';
    flags[1] = (parray[p1].flags.want_logins && parray[p1].flags.want_gshouts) ? ' ' : 'Q';
    g0=parray[p1].session.gnum;
    if (g0 >= 0
      && (garray[g0].Ladder19 || garray[g0].Ladder9)) {
      flags[2] = '*';
#if WANT_PAIR
    } else if (g0 >= 0 && paired(g0)) {
      flags[2] = '@';
#endif
    } else if (!parray[p1].flags.is_open && g0 < 0) {
      flags[2] = 'X';
    } else if (parray[p1].flags.is_looking && g0 < 0) {
      flags[2] = '!';
    } else {
      flags[2] = ' ';
    }
    flags[3] = 0;
    if (parray[p1].session.num_observe > 0)
      sprintf(otemp, "%2d", parray[p1].session.observe_list[0] + 1);
    else strcpy(otemp, "--" );
    if (g0 >= 0) sprintf(gtemp, "%2d", g0 + 1);
    else strcpy(gtemp, "--" );
    if (parray[p1].rank && parray[p1].rank[0])
      strcpy(rtemp, parray[p1].rank);
    else strcpy(rtemp, "None");
    sprintf(line, "%s %s %s %-10s %3.3s%s %3d %3d %3s  %-38.38s",
              flags, otemp, gtemp,
              parray[p1].pname,
              parray[p1].srank,
              parray[p1].flags.is_rated ? "*" : parray[p1].rating > 1 ? "?" : " ",
              pos19,
              pos9,
              secs2str_short(player_idle(p1)),
              rtemp);
    pcn_out(p,CODE_INFO, FORMATkibitz_sn, line);
  }
}

static void who_terse(int p, int num, int *plist, int type)
{
  int i, p1, left;
  char flags[4], otemp[3], gtemp[3];
  UNUSED(type);

  left = 1;

  pcn_out(p, CODE_WHO, FORMAT_INFO_NAME_IDLE_RANK_INFO_NAME_IDLE_RANKn);
  for (i = 0; i < num; i++) {
    p1 = plist[i];
/*    if (parray[p1].invisable && parray[p].adminLevel < ADMIN_ADMIN) continue; */
    flags[0] = ' ';
    flags[1] = (parray[p1].i_shout) ? (parray[p1].flags.want_logins) ? ' ' : 'Q' : 'S';
    if (!parray[p1].flags.is_open) {
      flags[2] = 'X';
    } else if (parray[p1].flags.is_looking && parray[p1].session.gnum < 0) {
      flags[2] = '!';
    } else {
      flags[2] = ' ';
    }
    flags[3] = 0;
    if (parray[p1].session.num_observe > 0)
      sprintf(otemp, "%2d", parray[p1].session.observe_list[0] + 1);
    else strcpy(otemp, "--" );
    if (parray[p1].session.gnum >= 0)
      sprintf(gtemp, "%2d", parray[p1].session.gnum + 1);
    else strcpy(gtemp, "--" );

    pcn_out(p, (left? CODE_WHO: CODE_NONE), FORMAT_s_s_s_s_s_s_s_s,
              flags, otemp, gtemp,
              parray[p1].pname,
	      secs2str_short(player_idle(p1)),
              parray[p1].srank,
              parray[p1].flags.is_rated ? "*" : " ",
#if MIMIC_NNGS_1205
	      left ? "| " : " \n"); /* AvK: extra space before \n */
#else
	      left ? "| " : "\n");
#endif
    left=(left)?0:1;
    }
  if (!left) pcn_out(p,CODE_NONE,FORMAT_sn,"                                  ");
  pcn_out(p, CODE_WHO, FORMAT_u_PLAYERS_u_TOTAL_GAMES_, player_count(), game_count());

}

/* This is one of the most compliclicated commands in terms of parameters */
int com_who(int p, struct parameter * param)
{
  int style = 0;
  int *sortarray = sort_alpha;
/*  float stop_perc = 1.0;
  float start_perc = 0;
*/
  unsigned sel_bits = 0;

  unsigned int to, from;
  char kord1, kord2;
  char options[MAX_STRING_LENGTH];

  int plist[256];
  int tmpint;

  char *cp;
  int p1, count, num_who;
  int sort_type =  0;

  int all = 0;
  /* AvK: -1 not possible for an unsigned type, Changed it to zero.
  ** this is equivalent to rank==31k. Hope that won't hurt.
  */
  from = to = 0;  /* (show all) */

  if (param[0].type != TYPE_STRING) {
    all = 1;  /* Assume that they want everyone */
    style = 1;
    sortarray = sort_alpha;
    cp=NULL;
  }
  else {
    strcpy(options, param[0].val.string);
    cp = strtok(options, " ");
  }
  for( ;cp; cp=strtok(NULL," ") ) {
    tmpint = sscanf(cp, "%u%c-%u%c", &from, &kord1, &to, &kord2);
    switch(tmpint) {
      case 4: to=parse_rank(to, kord2);
      case 2: from=parse_rank(from, kord1);
        continue;
      default: break;
    }
    if (!strcasecmp(cp, "ALL")) {
      all = 1;
      sortarray = sort_alpha;
      continue;
    }
    switch(cp[0]) {
      case 'k': case 'K': from = 0; to = 30; continue;
      case 'd': case 'D': from = 31; to = 39; continue;
      case 'p': case 'P': from = 40; to = 50; continue;
      case 'o': sel_bits |= WHO_OPEN; all = 0; continue;
      case 'r': sel_bits |= WHO_RATED; all = 0; continue;
      case 'l': sel_bits |= WHO_LOOKING; all = 0; continue;
      case 'f': sel_bits |= WHO_FREE; all = 0; continue;
      case 'R': sel_bits |= WHO_REGISTERED; all = 0; continue;
      case 'A': case 'a': style = all = 1; sortarray = sort_alpha; continue;/*ALL*/
      case 'U': sel_bits |= WHO_UNREGISTERED; all = 0; continue;
      default: return COM_BADPARAMETERS; continue;
    }
  }
  if (to == 0) {
    to = from;          /* Not interval, make to == from. */
  }
  if (to < from) { /* wrong order, swap them */
    tmpint = from; from = to; to = tmpint;
  }

  from = (from * 100);
  to = (to * 100);

  if (from == to) to += 99;
  num_who = count = 0;
  if (!sel_bits) sel_bits = 0xff;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!who_ok(sortarray[p1], sel_bits, from, to)) continue;
    plist[num_who++] = sortarray[p1];
    count++;
  }

#if 0
  startpoint = floor((float) count * start_perc);
  stoppoint = ceil((float) count * stop_perc) - 1;
  num_who = 0;
  count = 0;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!who_ok(sortarray[p1], sel_bits, (from * 100) - 100, (to * 100) - 100))
      continue;
    if (count >= startpoint && count <= stoppoint) {
      plist[num_who++] = sortarray[p1];
    }
    count++;
  }
#endif

  if (!num_who) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_ARE_NO_PLAYERS_LOGGED_IN_THAT_MATCH_YOUR_FLAG_SET_n);
    return COM_OK;
  }
  switch (style) {
  case 0:			/* terse */
    who_terse(p, num_who, plist, sort_type);
    break;
  case 1:
    who_terse(p, num_who, plist, sort_type);
    break;
  default:
    return COM_BADPARAMETERS;
    break;
  }
  return COM_OK;
}


int com_censor(int p, struct parameter * param)
{
  int p1;

  if (param[0].type != TYPE_WORD) {
    if (censor_count(parray[p].censor_list) == 0) {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_NO_ONE_CENSORED_);
      return COM_OK;
    } else {
      char *c;

      pcn_out(p,CODE_INFO, FORMAT_YOU_HAVE_CENSORED_);
      censor_start(parray[p].censor_list);
      while (censor_next(&c, parray[p].censor_list))
	pprintf(p, " %s", c);
    }
    pprintf(p, ".\n");
    return COM_OKN;
  }
  if (censor_count(parray[p].censor_list) >= MAX_CENSOR) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_ALREADY_CENSORING_THE_MAXIMUM_NUMBER_OF_PLAYERS_);
    return COM_OK;
  }
  stolower(param[0].val.word);
  if (censor_lookup(param[0].val.word, parray[p].censor_list)) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_ARE_ALREADY_CENSORING_s_, param[0].val.word);
    return COM_OK;
  }
  p1 = player_find_part_login(param[0].val.word);
  if (p1 == p) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_CENSOR_YOURSELF_);
    return COM_OK;
  }
  censor_add(param[0].val.word, parray[p].censor_list);
  pcn_out(p,CODE_INFO, FORMAT_s_CENSORED_n, param[0].val.word);
  return COM_OK;
}


int com_uncensor(int p, struct parameter * param)
{
  char *pname;

  pname = (param[0].type == TYPE_WORD) ? param[0].val.word: NULL;

  if (!pname) {
    char *cp;

    pcn_out(p,CODE_INFO, FORMAT_UNCENSORING_ALL_);
    censor_start(parray[p].censor_list);
    while (censor_next(&cp, parray[p].censor_list))
      pprintf(p, " %s", cp);
    pprintf(p, ".\n");
    censor_clear(parray[p].censor_list);
  } else {
    stolower(pname);
    if (censor_rem(pname, parray[p].censor_list))
      pcn_out(p,CODE_INFO, FORMAT_s_UNCENSORED_, pname);
    else
      pcn_out(p, CODE_ERROR, FORMAT_NO_ONE_WAS_UNCENSORED_);
  }
  return COM_OK;
}


int com_channel(int p, struct parameter * param)
{
  int i, j, err;

  if (param[0].type == TYPE_NULL) {	/* Turn off all channels */
    pcommand(p, "inchannel");
  } else {
    i = param[0].val.integer;
    if (i == 0 && parray[p].adminLevel < ADMIN_ADMIN) {
      pcn_out(p, CODE_ERROR, FORMAT_INVALID_CHANNEL_NUMBER_);
      return COM_OK;
    }
    if (i < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_THE_LOWEST_CHANNEL_NUMBER_IS_1_);
      return COM_OK;
    }
    if (i >= MAX_NCHANNEL) {
      pcn_out(p, CODE_ERROR, FORMAT_THE_MAXIMUM_CHANNEL_NUMBER_IS_d_, MAX_NCHANNEL - 1);
      return COM_OK;
    }
    if (on_channel(i, p)) {
      if (!channel_remove(i, p))
	pcn_out(p,CODE_INFO, FORMAT_CHANNEL_d_TURNED_OFF_n, i);
      for (j = 0; j < carray[i].count; j++) {
	if (carray[i].members[j] == p) continue;
	pcn_out_prompt(carray[i].members[j], CODE_INFO, FORMAT_s_HAS_LEFT_CHANNEL_d_n,
		       parray[p].pname, i);
      }
      /* [PEM]: Only zap last_channel if it's the one we just left. */
      if (parray[p].last_channel == i)
	parray[p].last_channel = -1;
    } else {
      err=channel_add(i,p);
      switch (err) {
      case 0:
	pcn_out(p,CODE_INFO, FORMAT_CHANNEL_d_TURNED_ON_n, i);
        pcn_out(p,CODE_INFO, FORMAT_CHANNEL_d_TITLE_n, i);
        pcn_out(p,CODE_INFO, FORMAT_sn, carray[i].ctitle);
        parray[p].last_channel = i;
        pcn_out(p,CODE_INFO, FORMAT_SETTING_YOUR_TO_CHANNEL_dn, i);
        for (j = 0; j < carray[i].count; j++) {
          if (carray[i].members[j] == p) continue;
          pcn_out_prompt(carray[i].members[j], CODE_INFO, FORMAT_s_HAS_JOINED_CHANNEL_d_n,
                     parray[p].pname, i);
        }
        break;
      case 1:
	pcn_out(p, CODE_ERROR, FORMAT_CHANNEL_d_IS_ALREADY_FULL_n, i);
        break;
      case 2:
        pcn_out(p, CODE_ERROR, FORMAT_MAXIMUM_CHANNEL_NUMBER_EXCEEDED_n);
        break;
      case 3:
        pcn_out(p, CODE_ERROR, FORMAT_INVALID_CHANNEL_n);
        break;
      case 4:
        pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_CHANNEL_IS_LOCKED_n);
        break;
      case 5:
        pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_CHANNEL_IS_CLOSED_n);
        break;
      }
    }
  }
  return COM_OKN;
}


int com_unlock(int p, struct parameter * param)
{
  int i, j;

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  if (on_channel(i, p) || parray[p].adminLevel >= ADMIN_ADMIN) {
    for (j = 0; j < carray[i].count; j++) {
      pprintf_prompt(carray[i].members[j], "%s has unis_locked channel %d\n", parray[p].pname, i);
    }
    carray[i].is_locked = 0;
  }
  return COM_OK;
}


int com_lock(int p, struct parameter * param)
{
  int i, j;

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  if (on_channel(i, p) || parray[p].adminLevel >= ADMIN_ADMIN) {
    for (j = 0; j < carray[i].count; j++) {
      pcn_out_prompt(carray[i].members[j], CODE_INFO, FORMAT_s_HAS_LOCKED_CHANNEL_dn,
      parray[p].pname, i);
    }
    carray[i].is_locked = 1;
  }
  return COM_OK;
}


int com_dnd(int p, struct parameter * param)
{
  int i, j;

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  if (on_channel(i, p) || parray[p].adminLevel >= ADMIN_ADMIN) {
    if (!carray[i].is_private) {
      for (j = 0; j < carray[i].count; j++) {
        pcn_out_prompt(carray[i].members[j], CODE_INFO, FORMAT_s_HAS_DO_NOT_DISTURBED_CHANNEL_dn,
        parray[p].pname, i);
      }
      carray[i].is_private = 1;
    }
    else if (carray[i].is_private) {
      for (j = 0; j < carray[i].count; j++) {
        pcn_out_prompt(carray[i].members[j], CODE_INFO, FORMAT_s_HAS_REMOVED_THE_DO_NOT_DISTURB_ON_CHANNEL_dn,
        parray[p].pname, i);
      }
      carray[i].is_private = 0;
    }
  }
  return COM_OK;
}


int com_ctitle(int p, struct parameter * param)
{
  int i, j;
  char szBuf[1024];

  if (param[0].val.integer >= MAX_NCHANNEL
    || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  if (strlen(param[1].val.string) > 200) return COM_OK;

  sprintf(szBuf, "[%s] %s", parray[p].pname, param[1].val.string);
  if (on_channel(i, p)) {
    free(carray[i].ctitle);
    carray[i].ctitle = mystrdup(szBuf);
    for (j = 0; j < carray[i].count; j++) {
      pcn_out_prompt(carray[i].members[j], CODE_INFO,
                     FORMAT_s_HAS_CHANGED_THE_TITLE_OF_CHANNEL_d_TO_sn,
                      parray[p].pname, i, carray[i].ctitle);
    }
  }
  return COM_OK;
}


int com_inchannel(int p, struct parameter * param)
{
  int c1, c2;
  int i, j, count = 0;

  if (param[0].type == TYPE_NULL) {  /* List everyone on every channel */
    c1 = -1;
    c2 = -1;
  } else if (param[1].type == TYPE_NULL) {	/* One parameter */
    c1 = param[0].val.integer;
    if (c1 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_THE_LOWEST_CHANNEL_NUMBER_IS_1_);
      return COM_OK;
    }
    c2 = -1;
  } else {			/* Two parameters */
    c1 = param[0].val.integer;
    c2 = param[2].val.integer;
    if (c1 < 0 || c2 < 0) {
      pcn_out(p, CODE_ERROR, FORMAT_THE_LOWEST_CHANNEL_NUMBER_IS_1_);
      return COM_OK;
    }
    pcn_out(p, CODE_ERROR, FORMAT_TWO_PARAMETER_INCHANNEL_IS_NOT_IMPLEMENTED_ );
    return COM_OK;
  }
  if (c1 >= MAX_NCHANNEL || c2 >= MAX_NCHANNEL) {
    pcn_out(p, CODE_ERROR, FORMAT_THE_MAXIMUM_CHANNEL_NUMBER_IS_d_, MAX_NCHANNEL - 1);
    return COM_OK;
  }
  for (i = 0; i < MAX_NCHANNEL; i++) {
    if (carray[i].is_hidden && parray[p].adminLevel < ADMIN_ADMIN) continue;
    if (carray[i].count && (c1 < 0 || i == c1)) {
      pcn_out(p,CODE_INFO, FORMAT_CHANNEL_d_sss_sn, i,
                  carray[i].is_locked ? "L" : "-",
                  carray[i].is_hidden ? "H" : "",
                  carray[i].is_private ? "D" : "",
                  carray[i].ctitle );
      pcn_out(p,CODE_INFO, FORMAT_empty );
      for (j = 0; j < carray[i].count; j++) {
	pprintf(p, " %s", parray[carray[i].members[j]].pname);
      }
      count++;
      pprintf(p, "\n");
    }
  }
  if (!count) {
    if (c1 < 0)
      pcn_out(p, CODE_ERROR, FORMAT_NO_CHANNELS_IN_USE_n);
    else
      pcn_out(p, CODE_ERROR, FORMAT_CHANNEL_NOT_IN_USE_n);
  }
  return COM_OKN;
}

/* For GO matches */

int com_tmatch(int p, struct parameter * param)
{
return com_xmatch(p, param, GAMETYPE_TNETGO);
}

/* For GO matches */

int com_gmatch(int p, struct parameter * param)
{
return com_xmatch(p, param, GAMETYPE_GO);
}

/* For GOE matches */

int com_goematch(int p, struct parameter * param)
{
return com_xmatch(p, param, GAMETYPE_GOEGO);
}

static int com_xmatch(int p0, struct parameter * param, int gametype)
{
  int p1;
  struct pending * hers, * mine;

  int start_time = -1;		/* start time */
  int byo_time = -1;		/* byo time */
  int size = -1;                /* size of board */
  int wp, bp;
  char *val;
  int pendtype = -1,rulestype = -1, formattype=0, mode=0;


  /* Things we need:
  g:   player_to_challenge challenger_color size_of_board time byo-yomi_time
  t:   player_to_challenge challenger_color size_of_board time byo-yomi_time
  e:   player_to_challenge challenger_color size_of_board time

     looks like:

 GAMETYPE
 GO        g:   match daveg b 19 30 10
 TNETGO    t:   tmatch daveg b 19 30 10
 GOEGO     e:   goematch daveg b 19 30
  */

  if (parray[p0].session.gnum >= 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_CAN_T_CHALLENGE_WHILE_YOU_ARE_PLAYING_A_GAME_);
    return COM_OK;
  }
  if (!parray[p0].flags.is_open) {
    parray[p0].flags.is_open = 1;
    pcn_out(p0,CODE_INFO, FORMAT_SETTING_YOU_OPEN_FOR_MATCHES_n);
  }
  stolower(param[0].val.word);
  stolower(param[1].val.word);

  p1 = player_find_part_login(param[0].val.word);
  if (p1 < 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
    return COM_OK;
  }
  if (p1 == p0) {
    pcn_out(p0, CODE_ERROR, FORMAT_USE_qTEACHq_FOR_TEACHING_GAMES_);
    return COM_OK;
  }
  if (player_censored(p1, p0)) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_qsq_IS_CENSORING_YOU_, parray[p1].pname);
    return COM_OK;
  }
  if (player_censored(p0, p1)) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_CENSORING_qsq_, parray[p1].pname);
    return COM_OK;
  }
  if (!parray[p1].flags.is_open) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_qsq_IS_NOT_OPEN_TO_MATCH_REQUESTS_, parray[p1].pname);
    return COM_OK;
  }
  if (parray[p1].session.gnum >= 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_qsq_IS_INVOLVED_IN_ANOTHER_GAME_, parray[p1].pname);
    return COM_OK;
  }

  switch(gametype) {
    default:
    case GAMETYPE_GO: rulestype=RULES_NET; pendtype=PEND_GMATCH;
      formattype = FORMAT_USE_MATCH_s_s_d_d_d_OR_DECLINE_s_TO_RESPOND_n;
      break;
    case GAMETYPE_TNETGO: rulestype=RULES_NET; pendtype=PEND_TMATCH;
      formattype = FORMAT_USE_TMATCH_s_s_d_d_d_OR_DECLINE_s_TO_RESPOND_n;
      break;
    case GAMETYPE_GOEGO: rulestype=RULES_ING; pendtype=PEND_GOEMATCH;
      formattype = FORMAT_USE_GOEMATCH_s_s_d_d_OR_DECLINE_s_TO_RESPOND_n;
      break;
  }

  mine = pending_find(p0, p1, pendtype);
  hers = pending_find(p1, p0, pendtype);
  if (mine) mode |= 1;
  if (hers) mode |= 2;
  val = param[1].val.string;

  switch (*val) {
    case 'B': case 'b': bp = p0 ; break;
    case 'W': case 'w': bp = p1; break;
    case '?': bp =  (rand()&1) ? p0 : p1 ; break; /* Nigiri! */
    default: return COM_BADPARAMETERS;
  }
#if (MIMIC_NNGS_1205)
  if (hers) bp=hers->param1;
#endif
  wp = (bp == p0) ? p1 : p0;

  if ((size = param[2].val.integer) < 2)
  {
    pcn_out(p0, CODE_ERROR, FORMAT_BOARDSIZE_MUST_BE_2n);
    return COM_OK;
  }
  if ((start_time = param[3].val.integer) < 0)
  {
    pcn_out(p0, CODE_ERROR, FORMAT_START_TIME_MUST_BE_0n);
    return COM_OK;
  }
  switch(gametype) {
  case GAMETYPE_GO:
  case GAMETYPE_TNETGO:
    if ((byo_time = param[4].val.integer) < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_BYO_YOMI_TIME_MUST_BE_0n);
      return COM_OK;
    }
    break;
  default:
    byo_time = -1; break;
  }

  if (!mine) {
    mine = player_pending_new(p0, p1, pendtype);
    if (!mine) {
      pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOU_CAN_T_HAVE_ANY_MORE_PENDING_MATCHES_);
      return COM_OK;
    }
  }
  mine->param1 = bp;
  mine->param2 = size;
  mine->param3 = start_time;
  mine->param4 = byo_time;


  if (hers /* Identical match, should accept! */
      && mine->param1 == hers->param1
      && mine->param2 == hers->param2
      && mine->param3 == hers->param3
      && mine->param4 == hers->param4) {

    if (size == 0) return COM_BADPARAMETERS;
    if (create_new_gomatch(wp, bp,
        size, start_time, byo_time, 0, rulestype, gametype) == COM_FAILED) {
      pcn_out(p0, CODE_ERROR, FORMAT_THERE_WAS_A_PROBLEM_CREATING_THE_NEW_MATCH_);
      pcn_out_prompt(p1, CODE_ERROR, FORMAT_THERE_WAS_A_PROBLEM_CREATING_THE_NEW_MATCH_n);
    }
    /* Logit("Set Game_TYPE to %d(p=%d,r=%d)", gametype,pendtype, rulestype); */
    parray[p0].match_type = gametype;
    parray[p1].match_type = gametype;
    player_remove_requests(p0, p1, pendtype);
    player_remove_requests(p1, p0, pendtype);
    return COM_OK;
  }
 
  	/* when we get here, there is a match offer to make:
        ** (0) a new offer
        ** (1) an update to an existing offer
        ** (2) a response to an offer from her
        ** (3) a reresponse to an offer from her
        */
  switch(mode) {
  case 0:
    break;
  case 1:
    pcn_out(p0,CODE_INFO, FORMAT_UPDATING_OFFER_ALREADY_MADE_TO_qsq_n,
            parray[p1].pname);
    pcn_out(p1, CODE_INFO, FORMAT_s_UPDATES_THE_MATCH_REQUEST_n,
            parray[p0].pname);
    break;
  case 2:
  case 3:
    pcn_out(p0,CODE_INFO, FORMAT_DECLINING_OFFER_FROM_s_AND_OFFERING_NEW_MATCH_PARAMETERS_n, parray[p1].pname);
    pcn_out_prompt(p1, CODE_NONE, FORMAT_s_DECLINES_YOUR_REQUEST_FOR_A_MATCH_n, parray[p0].pname);
    break;
  }

  pcn_out(p0, CODE_INFO, FORMAT_REQUESTING_MATCH_IN_d_MIN_WITH_s_AS_s_n,
		start_time,
                parray[p1].pname,
                p1 == wp ? "White" : "Black");
  pcn_out(p1, CODE_INFO, FORMAT_MATCH_dXd_IN_d_MINUTES_REQUESTED_WITH_s_AS_s_n,
                 size, size,
                 start_time,
                 parray[p0].pname,
                 p0 == bp ? "Black" : "White");
  pcn_out_prompt(p1, CODE_INFO, formattype,
                 parray[p0].pname,
                 p0 == bp ? "W" : "B",
                 size,
                 start_time,
                 byo_time,
                 parray[p0].pname);
  return COM_OK;
}

/* [PEM]: Set *p to player p's rating. Returns 1 on success, 0 otherwise. */
static int
get_nrating(int p0, double *ratp)
{
#if WANT_NNGSRATED
  rdbm_t db;
  rdbm_player_t rp;
  char fname[MAX_FILENAME_SIZE];

  xyfilename(fname, FILENAME_NRATINGS );
  if (!(db = rdbm_open(fname,0)))
    return 0;
  if (rdbm_fetch(db, parray[p0].pname, &rp))
  {
    *ratp = rp.rating;
    rdbm_close(db);
    return 1;
  }
  rdbm_close(db);
#else
  UNUSED(p0);
  UNUSED(ratp);
#endif /* WANT_NNGSRATED */
  return 0;
}

static void
aux_suggest(int p0, int ratdiff, int stronger)
{
  int stones;
  float komi;

  /* PEM: Ugly attempt to NaN-bug workaround. */
  AutoMatch(ratdiff, 19, &stones, &komi);
  switch (stones) {
  case 0: case 1:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_19X19_PLAY_AN_EVEN_GAME_AND_SET_KOMI_TO_f_n, komi);
    break;

  case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_19X19_s_d_HANDICAP_STONES_AND_SET_KOMI_TO_f_n,
	    stronger ? "Give" : "Take", stones, komi);
    break;

  default:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_19X19_s_9_HANDICAP_STONES_AND_SET_KOMI_TO_0_5n,
	    stronger ? "Give" : "Take");
    break;
  }

  /* PEM: Ugly attempt to NaN-bug workaround. */
  AutoMatch(ratdiff, 13, &stones, &komi);
  switch (stones)
  {
  case 0: case 1:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_13X13_PLAY_AN_EVEN_GAME_AND_SET_KOMI_TO_f_n, komi);
    break;

  case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_13X13_s_d_HANDICAP_STONES_AND_SET_KOMI_TO_f_n,
	    stronger ? "Give" : "Take", stones, komi);
    break;

  default:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_13X13_s_9_HANDICAP_STONES_AND_SET_KOMI_TO_0_5n,
	    stronger ? "Give" : "Take");
    break;
  }

  /* PEM: Ugly attempt to NaN-bug workaround. */
  AutoMatch(ratdiff, 9, &stones, &komi);
  switch (stones)
  {
  case 0: case 1:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_9X9_PLAY_AN_EVEN_GAME_AND_SET_KOMI_TO_f_n, komi);
    break;

  case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_9X9_s_d_HANDICAP_STONES_AND_SET_KOMI_TO_f_n,
	    stronger ? "Give" : "Take", stones, komi);
    break;

  default:
    pcn_out(p0, CODE_INFO, FORMAT_FOR_9X9_s_9_HANDICAP_STONES_AND_SET_KOMI_TO_0_5n,
	    stronger ? "Give" : "Take");
    break;
  }
}

#if WANT_NNGSRATED
int com_nsuggest(int p0, struct parameter * param)
{
  int p1, p2;
  int stronger;
  double rat1, rat2;

  /* Have at least one argument, maybe two. If only one,
  ** we use p1 as the caller and p2 as the only argument.
  ** AvK: What have you been smoking, boys ?
  */
  if (param[1].type == TYPE_NULL) { /* One argument. */
    p1 = p0;
    p2 = player_fetch(param[0].val.word);
    if (p2<0) return COM_OK;
    player_fix(p1);
  } else {				/* Two arguments. */
    p1 = player_fetch(param[0].val.word);
    if (p1<0) return COM_OK;
    p2 = player_fetch(param[1].val.word);
    if (p2<0) {
      player_unfix(p1);
      return COM_OK;
    }
  }

  if (p1 == p2) {
    pcn_out(p0,CODE_INFO, FORMAT_PLEASE_TYPE_qHELP_TEACHqn);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }

  if (!get_nrating(p1, &rat1)) {
    if (p0 == p1)
      pcn_out(p0, CODE_INFO, FORMAT_YOU_ARE_NOT_RATED_SO_I_CANNOT_SUGGEST_n );
    else
      pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATED_SO_I_CANNOT_SUGGEST_n, parray[p1].pname);
    pcn_out(p0, CODE_INFO, FORMAT_PLEASE_DISCUSS_HANDICAPS_WITH_YOUR_OPPONENT_n);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }
  if (!get_nrating(p2, &rat2)) {
    pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATED_SO_I_CANNOT_SUGGEST_n, parray[p2].pname);
    pcn_out(p0, CODE_INFO, FORMAT_PLEASE_DISCUSS_HANDICAPS_WITH_YOUR_OPPONENT_n );
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }

  if (rat1 > rat2)
    stronger = 1;
  else
    stronger = 0;

  pcn_out(p0, CODE_INFO, FORMAT_I_SUGGEST_THAT_s_PLAY_s_AGAINST_s_n,
	  p0 == p1 ? "you" : parray[p1].pname,
	  stronger ? "White" : "Black",
	  parray[p2].pname);

  if (rat1 > rat2)
    aux_suggest(p0, abs((int)(100*(rat1 - rat2 + 0.005))), stronger);
  else
    aux_suggest(p0, abs((int)(100*(rat2 - rat1 + 0.005))), stronger);

  player_unfix(p1);
  player_unfix(p2);

  return COM_OK;
}
#endif /* WANT_NNGSRATED */


int com_osuggest(int p0, struct parameter * param)
{
  int p1, p2;
  int stronger;

  /* Have at least one argument, maybe two. If only one,
  ** we use p1 as the caller and p2 as the only argument.
  */
  if (param[1].type == TYPE_NULL) { /* One argument. */
    p1 = p0;
    p2 = player_fetch(param[0].val.word);
    if (p2<0) return COM_OK;
    player_fix(p1);
    }
  else {			/* Two arguments. */
    p1 = player_fetch(param[0].val.word);
    if (p1<0) return COM_OK;
    p2 = player_fetch(param[1].val.word);
    if (p2<0) {
      player_unfix(p1);
      return COM_OK;
    }
  }

  if (p1 == p2) {
    pcn_out(p0,CODE_INFO, FORMAT_PLEASE_TYPE_qHELP_TEACHqn);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }

  if (parray[p1].rating == 0)
  {
    if (p1 == p0)
      pcn_out(p0, CODE_INFO, FORMAT_YOU_ARE_NOT_RATED_SO_I_CANNOT_SUGGEST_n );
    else
      pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATED_SO_I_CANNOT_SUGGEST_n,
	      parray[p1].pname);
    pcn_out(p0, CODE_INFO, FORMAT_PLEASE_DISCUSS_HANDICAPS_WITH_YOUR_OPPONENT_n );
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }
  if (parray[p2].rating == 0) {
    if (p2 == p0)
      pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATED_SO_I_CANNOT_SUGGEST_n, parray[p2].pname);
    else
      pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATED_SO_I_CANNOT_SUGGEST_n, parray[p2].pname);
    pcn_out(p0, CODE_INFO, FORMAT_PLEASE_DISCUSS_HANDICAPS_WITH_YOUR_OPPONENT_n);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }

  if (parray[p1].rating > parray[p2].rating)
    stronger = 1;
  else
    stronger = 0;

  pcn_out(p0, CODE_INFO, FORMAT_I_SUGGEST_THAT_s_PLAY_s_AGAINST_s_n,
	  p0 == p1 ? "you" : parray[p1].pname,
	  stronger ? "White" : "Black",
	  parray[p2].pname);

  aux_suggest(p0, abs(parray[p1].rating - parray[p2].rating), stronger);

  player_unfix(p1);
  player_unfix(p2);
  return COM_OK;
}


int com_teach(int p0, struct parameter * param)
{
  int t, g0;

  if (parray[p0].session.gnum >= 0) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_ALREADY_PLAYING_A_GAME_);
    return COM_OK;
  }

  if (param[0].val.integer <= 0) t = COM_FAILED;
  else t = create_new_gomatch(p0, p0, param[0].val.integer, 1, 1, 1, RULES_NET, GAMETYPE_GO);
  if (t == COM_FAILED) return COM_FAILED;
  g0 = parray[p0].session.gnum;
  garray[g0].teach = 1;
  garray[g0].rated = 0;
  return COM_OK;
}


int create_new_gomatch(int wp, int bp,
    int size,
    int start_time, int byo_time,
    int teaching, int rules, int type)
{
  int g0, px;
  char outStr[1024];

  g0 = game_new(GAMETYPE_GO,size);
  const struct ladderplayer *lp;
  int bpos, wpos;

  if (g0 < 0) return COM_FAILED;
  if (size >= NUMLINES) return COM_FAILED;

  garray[g0].minkg = mink_initgame(size,size, rules);
  garray[g0].slotstat.is_playing = 1;
  garray[g0].gstatus = GSTATUS_ACTIVE;
  garray[g0].white.pnum = wp;
  garray[g0].black.pnum = bp;

  switch(type) {
    case GAMETYPE_GO:
    player_remove_requests(wp, bp, PEND_GMATCH);
    player_remove_requests(bp, wp, PEND_GMATCH);
    player_decline_offers(wp, -1, PEND_GMATCH);
    player_withdraw_offers(wp, -1, PEND_GMATCH);
    player_decline_offers(bp, -1, PEND_GMATCH);
    player_withdraw_offers(bp, -1, PEND_GMATCH);
    garray[g0].gotype = GAMETYPE_GO;
    break;

    case GAMETYPE_GOEGO:
    player_remove_requests(wp, bp, PEND_GOEMATCH);
    player_remove_requests(bp, wp, PEND_GOEMATCH);
    player_decline_offers(wp, -1, PEND_GOEMATCH);
    player_withdraw_offers(wp, -1, PEND_GOEMATCH);
    player_decline_offers(bp, -1, PEND_GOEMATCH);
    player_withdraw_offers(bp, -1, PEND_GOEMATCH);
    garray[g0].gotype = GAMETYPE_GOEGO;
    break;

    case GAMETYPE_TNETGO:
    player_remove_requests(wp, bp, PEND_TMATCH);
    player_remove_requests(bp, wp, PEND_TMATCH);
    player_decline_offers(wp, -1, PEND_TMATCH);
    player_withdraw_offers(wp, -1, PEND_TMATCH);
    player_decline_offers(bp, -1, PEND_TMATCH);
    player_withdraw_offers(bp, -1, PEND_TMATCH);
    garray[g0].gotype = GAMETYPE_TNETGO;
    break;
  }

  if (!strcmp(parray[wp].srank, parray[bp].srank)) garray[g0].komi = conffile.default_komi19;
  if (rules == RULES_ING) garray[g0].komi = 8.0;
  if (start_time == 0) {
    garray[g0].ts.time_type = TIMETYPE_UNTIMED;
    start_time = 480;
    byo_time = 480;
  }
  else {
    start_time = start_time * 60;			/* To Seconds */
    if (rules == RULES_NET || type == GAMETYPE_TNETGO) byo_time = byo_time * 60;
    else byo_time = (start_time) / 6;
    garray[g0].ts.time_type = TIMETYPE_TIMED;
  }
  garray[g0].teach = teaching?1:0;
  if (rules == RULES_NET || type == GAMETYPE_TNETGO) garray[g0].type = GAMETYPE_GO;
  else garray[g0].type = GAMETYPE_GOEGO;
#if USING_PRIVATE_GAMES
  garray[g0].Private = parray[wp].Private || parray[bp].Private;
#endif
  garray[g0].ts.totalticks = SECS2TICS(start_time);
  garray[g0].ts.byoticks = SECS2TICS(byo_time);
  garray[g0].ts.byostones = 25;    /* Making an assumption here */
  garray[g0].black.ticksleft = SECS2TICS(start_time);
  garray[g0].black.byoperiods = 0;
  garray[g0].black.penalty = 0;
  garray[g0].white.ticksleft = SECS2TICS(start_time);
  garray[g0].white.byoperiods = 0;
  garray[g0].white.penalty = 0;
  if (parray[wp].slotstat.is_registered && parray[bp].slotstat.is_registered) garray[g0].rated = 1;
  else garray[g0].rated = 0;
  if (!strcmp(parray[wp].srank, "NR")) garray[g0].rated = 0;
  if (!strcmp(parray[bp].srank, "NR")) garray[g0].rated = 0;
  garray[g0].onMove = PLAYER_BLACK;
  garray[g0].timeOfStart = globclock.time;
  garray[g0].starttick = globclock.tick;
  garray[g0].lastMovetick = garray[g0].starttick;
  garray[g0].lastDectick = garray[g0].starttick;
  garray[g0].clockStopped = 0;
  garray[g0].rules = rules;
  if (!garray[g0].teach) {
    pcn_out_prompt(wp, CODE_INFO, FORMAT_MATCH_d_WITH_s_IN_d_ACCEPTED_n,
          g0 + 1,
          parray[bp].pname,
          start_time / 60);
    pcn_out_prompt(wp, CODE_INFO, FORMAT_CREATING_MATCH_d_WITH_s_n,
          g0 + 1,
          parray[bp].pname);
  }
  pcn_out_prompt(bp, CODE_INFO, FORMAT_MATCH_d_WITH_s_IN_d_ACCEPTED_n,
          g0 + 1,
          parray[wp].pname,
          start_time / 60);
  pcn_out_prompt(bp, CODE_INFO, FORMAT_CREATING_MATCH_d_WITH_s_n,
          g0 + 1,
          parray[wp].pname);
  sprintf(outStr, "Game %d %s: %s (0 %d %d) vs %s (0 %d %d)",
        g0 + 1, "I",
        parray[wp].pname,
        TICS2SECS(garray[g0].white.ticksleft),
        garray[g0].white.byostones,
        parray[bp].pname,
	TICS2SECS(garray[g0].black.ticksleft),
	garray[g0].black.byostones);
  if (!garray[g0].teach) {
    if (parray[wp].flags.is_client) pcn_out(wp, CODE_MOVE, FORMAT_sn, outStr);
    if (parray[bp].flags.is_client) pcn_out(bp, CODE_MOVE, FORMAT_sn, outStr);
    }
  Logit("%s", outStr);
  sprintf(outStr, "{Match %d: %s [%3.3s%s] vs. %s [%3.3s%s] }\n",
	  g0 + 1, parray[wp].pname,
          parray[wp].srank,
          parray[wp].flags.is_rated ? "*" : " ",
	  parray[bp].pname,
          parray[bp].srank,
          parray[bp].flags.is_rated ? "*" : " ");
  for (px = 0; px < parray_top; px++) {
    if (!parray[px].slotstat.is_online) continue;
    if (!parray[px].flags.want_gshouts) continue;
    pcn_out_prompt(px, CODE_SHOUT, FORMAT_s, outStr);
  }
  parray[wp].session.protostate = STAT_PLAYING_GO;
  parray[bp].session.protostate = STAT_PLAYING_GO;
  parray[wp].session.gnum = g0;
  parray[wp].session.opponent = bp;
  parray[wp].session.side = PLAYER_WHITE;
  parray[bp].session.gnum = g0;
  parray[bp].session.opponent = wp;
  parray[bp].session.side = PLAYER_BLACK;
  send_go_boards(g0, 0);
  if (garray[g0].teach) return COM_OKN;
  if (size == 19) {
    if ((lp=ladder_player_named(Ladder19,parray[bp].pname))) {
      bpos = lp->idx;
    }
    else return COM_OKN;
    if ((lp=ladder_player_named(Ladder19,parray[wp].pname))) {
      wpos = lp->idx;
    }
    else return COM_OKN;
    if (wpos < bpos) {
      garray[g0].Ladder_Possible = 1;
      pcn_out(wp, CODE_INFO, FORMAT_THIS_CAN_BE_A_LADDER19_RATED_GAME_n);
      pcn_out(wp, CODE_INFO, FORMAT_TYPE_LADDER_BEFORE_YOUR_FIRST_MOVE_TO_MAKE_IT_LADDER_RATED_n);
      pcn_out(bp, CODE_INFO, FORMAT_THIS_CAN_BE_A_LADDER19_RATED_GAME_n);
      pcn_out(bp, CODE_INFO, FORMAT_TYPE_LADDER_BEFORE_YOUR_FIRST_MOVE_TO_MAKE_IT_LADDER_RATED_n);
    }
  }
  else if (size == 9) {
    if ((lp=ladder_player_named(Ladder9,parray[bp].pname))) {
      bpos = lp->idx;
    }
    else return COM_OKN;
    if ((lp=ladder_player_named(Ladder9,parray[wp].pname))) {
      wpos = lp->idx;
    }
    else return COM_OKN;
    if (wpos < bpos) {
      garray[g0].Ladder_Possible = 1;
      pcn_out(wp, CODE_INFO, FORMAT_THIS_CAN_BE_A_LADDER9_RATED_GAME_n );
      pcn_out(wp, CODE_INFO, FORMAT_TYPE_LADDER_BEFORE_YOUR_FIRST_MOVE_TO_MAKE_IT_RATED_n );
      pcn_out(bp, CODE_INFO, FORMAT_THIS_CAN_BE_A_LADDER9_RATED_GAME_n );
      pcn_out(bp, CODE_INFO, FORMAT_TYPE_LADDER_BEFORE_YOUR_FIRST_MOVE_TO_MAKE_IT_RATED_n );
    }
  }
  return COM_OKN;
}

int com_accept(int p, struct parameter * param)
{
  int seq = -1;
  int type = -1;
  int p1 = -1;
  int mode = 0, cnt = 0, idx = 0;
  struct pending *ptr=NULL;
  const char *stype = "unknown";

  switch(param[0].type) {
  case TYPE_NULL:
    seq = 0;
    mode = 0;
    break;
  case TYPE_INT:
    seq = param[0].val.integer - 1;
    mode = 1;
    break;
  case TYPE_WORD:
    if (!strcmp(param[0].val.word, "pause")) {
      stype = "pause"; type = PEND_PAUSE; mode = 2;
    } else if (!strcmp(param[0].val.word, "adjourn")) {
      stype = "adjourn"; type = PEND_ADJOURN; mode = 2;
    } else if (!strcmp(param[0].val.word, "all")) {
      type = -1; p1 = -1; mode = 4;
    } else {
    
      p1 = player_find_part_login(param[0].val.word);
      if (p1 < 0) {
        pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_n, param[0].val.word);
        return COM_OK;
      }
      mode = 3;
    }
  }

  cnt = pending_count(p1, p, type);
  if (cnt <= 0) {
    switch(mode) {
    case 0: /* seq absent */
    case 1: /* seq nr */
    case 4: /* all */
      pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_NO_OFFERS_TO_ACCEPT_);
      break;
    case 2: /* type */
      pcn_out(p, CODE_ERROR, FORMAT_THERE_ARE_NO_PENDING_s_OFFERS_, stype);
      break;
    case 3: /* player name */
      pcn_out(p, CODE_ERROR, FORMAT_THERE_ARE_NO_PENDING_OFFERS_FROM_s_,
              parray[p1].pname);
      break;
    }
    return COM_OK;
  }

  if (cnt> 1) {
    switch(mode) {
    case 0: /* seq absent */
      pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_MORE_THAN_ONE_OFFER_TO_ACCEPT_n );
      pcn_out(p, CODE_ERROR, FORMAT_USE_qPENDINGq_TO_SEE_THEM_AND_qACCEPT_Nq_TO_CHOOSE_WHICH_ONE_ );

      return COM_OK;
    case 1: /* seq nr */
      if ( seq < cnt) break;
      pcn_out(p, CODE_ERROR, FORMAT_OUT_OF_RANGE_USE_qPENDINGq_TO_SEE_THE_LIST_OF_OFFERS_);
      return COM_OK;
    }
  }

  for(idx=0,ptr=pending_find(p1,p,type);ptr;idx++,ptr=pending_next(ptr,p1,p,type)) {
    if (mode == 1 && idx != seq) continue;
    switch (ptr->type) {
    case PEND_TMATCH:
    case PEND_MATCH:
    case PEND_GMATCH:
      pcommand(p, "match %s", parray[ptr->whofrom].pname);
      break;
    case PEND_PAUSE:
      pcommand(p, "pause");
      break;
    case PEND_ADJOURN:
      pcommand(p, "adjourn");
      break;
    }
  }
  return COM_OK_NOPROMPT;
}


int com_decline(int p, struct parameter * param)
{
  int seq=0;
  int p1 = -1, type = -1;
  int mode=0,idx, cnt;
  struct pending *ptr;

  switch(param[0].type ) {
  case TYPE_NULL:
    mode=0; seq=0;
    break;
  case TYPE_INT:
  default:			/* Must be an integer */
    seq = param[0].val.integer -1 ; mode = 1;
    break;
  case TYPE_WORD: /* Draw adjourn match takeback abort or <name> */
    if (!strcmp(param[0].val.word, "match")) {
      type = PEND_MATCH; mode = 2;
    } else if (!strcmp(param[0].val.word, "pause")) {
      type = PEND_PAUSE; mode = 2;
    } else if (!strcmp(param[0].val.word, "adjourn")) {
      type = PEND_ADJOURN; mode = 2;
    } else if (!strcmp(param[0].val.word, "all")) {
      type = -1; mode = 4;
    } else {
      p1 = player_find_part_login(param[0].val.word);
      if (p1 < 0) {
        pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_,
                 param[0].val.word);
        return COM_OK;
      }
    mode = 3;
    }
  }

  cnt = pending_count(p1, p, type);
  if (cnt <= 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_NO_PENDING_OFFERS_FROM_OTHER_PLAYERS_ );
    return COM_OK;
    }

  switch(mode) {
  case 0: /* implicit */
    if (cnt > 1) {
      pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_MORE_THAN_ONE_PENDING_OFFER_n );
      pcn_out(p, CODE_ERROR, FORMAT_PLEASE_SPECIFY_WHICH_ONE_YOU_WISH_TO_DECLINE_n );
      pcn_out(p, CODE_ERROR, FORMAT_PENDING_WILL_GIVE_YOU_THE_LIST_ );
        return COM_OK;
      }
    break;
  case 1: /* seq */
      if (seq < 0 || seq >= cnt) {
        pcn_out(p, CODE_ERROR, FORMAT_INVALID_OFFER_NUMBER_MUST_BE_BETWEEN_1_AND_d_,
        cnt);
        return COM_OK;
    }
    break;
  }

  switch(mode) {
  case 0: /* implicit */
  case 1: /* seq */
    for(idx=0,ptr=pending_find(p1,p,type);ptr;idx++,ptr=pending_next(ptr,p1,p,type)) {
      if (idx != seq) continue;
      player_pending_delete(ptr); break;
    }
    cnt=1;
    break;
  case 2: /* type */
  case 3: /* name */
  case 4: /* all */
    cnt = player_decline_offers(p, p1, type);
  }
  if (cnt != 1)
    pcn_out(p, CODE_ERROR, FORMAT_d_OFFERS_DECLINED, cnt);
  return COM_OK;
}

int com_withdraw(int p, struct parameter * param)
{
  int seq=-1;
  int p1 = -1, type = -1;
  int mode=0,cnt=0,idx;
  struct pending * ptr;

  switch(param[0].type) {
  case TYPE_NULL:
    mode=0; seq=0;
    break;
  case TYPE_INT:
  default:	/* Must be an integer */
    seq = param[0].val.integer - 1;
    mode = 1;
    break;
  case TYPE_WORD: /* Draw adjourn match takeback abort or <name> */
    if (!strcmp(param[0].val.word, "match")) {
      mode=2; type = PEND_MATCH;
    } else if (!strcmp(param[0].val.word, "pause")) {
      mode=2; type = PEND_PAUSE;
    } else if (!strcmp(param[0].val.word, "adjourn")) {
      mode=2; type = PEND_ADJOURN;
    } else if (!strcmp(param[0].val.word, "all")) {
      mode=4; type = -1;
    } else {
      p1 = player_find_part_login(param[0].val.word);
      if (p1 < 0) {
        pcn_out(p, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_, param[0].val.word);
	return COM_OK;
      }
    mode =3;
    break;
    }
  }

  cnt = pending_count(p, p1, type);
  if (cnt <= 0) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_NO_PENDING_OFFERS_TO_OTHER_PLAYERS_ );
    return COM_OK;
  }

  if (cnt > 1) switch (mode) {
  case 0:
    pcn_out(p, CODE_ERROR, FORMAT_YOU_HAVE_MORE_THAN_ONE_PENDING_OFFER_n );
    pcn_out(p, CODE_ERROR, FORMAT_PLEASE_SPECIFY_WHICH_ONE_YOU_WISH_TO_WITHDRAW_n );
    pcn_out(p, CODE_ERROR, FORMAT_PENDING_WILL_GIVE_YOU_THE_LIST_ );
    return COM_OK;
  case 1:
    if (seq < 0 || seq >= cnt) {
      pcn_out(p, CODE_ERROR, FORMAT_INVALID_OFFER_NUMBER_MUST_BE_BETWEEN_1_AND_d_, cnt);
      return COM_OK;
    }
  }

  switch (mode) {
  case 0: /* implicit */
  case 1: /* seq */
    for(idx=0,ptr=pending_find(p,p1,type);ptr;idx++,ptr=pending_next(ptr,p,p1,type)) {
      if (idx != seq) continue;
      player_pending_delete(ptr); break;
    }
    cnt=1;
    break;
  case 2:
  case 3:
  case 4:
    cnt = player_withdraw_offers(p, p1, type);
    break;
  }
  if (cnt != 1)
    pcn_out(p, CODE_ERROR, FORMAT_d_OFFERS_WITHDRAWN, cnt);
  return COM_OK;
}

int com_pending(int p, struct parameter * param)
{
  int cnt=0;
  struct pending * ptr;
  UNUSED(param);

  if (parray[p].outgoing)
    pcn_out(p,CODE_INFO, FORMAT_OFFERS_TO_OTHER_PLAYERS_n);
  for(cnt=0,ptr=pending_find(p,-1,-1);ptr;ptr=pending_next(ptr,p,-1,-1)) {
    cnt++;
    pprintf(p, "   ");
    player_pend_print(p, ptr);
  }
  if (cnt==0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_ARE_NO_OFFERS_PENDING_TO_OTHER_PLAYERS_n );
    parray[p].outgoing = 0;
  }


  if (parray[p].incoming)
    pcn_out(p,CODE_INFO, FORMAT_OFFERS_FROM_OTHER_PLAYERS_n);
  for(cnt=0,ptr=pending_find(-1,p,-1);ptr;ptr=pending_next(ptr,-1,p,-1)) {
    cnt++;
    pprintf(p, "%2d: ", cnt);
    player_pend_print(p, ptr);
  }
  if (cnt==0) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_ARE_NO_OFFERS_PENDING_FROM_OTHER_PLAYERS_n );
    parray[p].incoming = 0;
  }
  else {
    pcn_out(p, CODE_INFO, FORMAT_IF_YOU_WISH_TO_ACCEPT_ANY_OF_THESE_OFFERS_TYPE_ACCEPT_N_nOR_JUST_ACCEPT_IF_THERE_IS_ONLY_ONE_OFFER_n );
  }
  return COM_OKN;
}

int com_watching(int p0, struct parameter * param)
{
  int idx, first;
  first = 1;
  UNUSED(param);

  if (parray[p0].session.num_observe) {
    pcn_out(p0,CODE_INFO, FORMAT_GAMES_CURRENTLY_BEING_OBSERVED_);
    for (idx = 0; idx < parray[p0].session.num_observe; idx++) {
      if (first) {
        pprintf(p0, "%3d", 1 + (parray[p0].session.observe_list[idx]));
        first = 0;
      } else {
        pprintf(p0, ",%3d", 1 + (parray[p0].session.observe_list[idx]));
      }
    }
    pprintf(p0, "\n");
  }
  else {
    pcn_out(p0, CODE_INFO, FORMAT_GAMES_CURRENTLY_BEING_OBSERVED_NONE_n);
  }
  return COM_OKN;
}

int com_score(int p0, struct parameter * param)
{
  int g0, wterr, bterr, wocc, bocc, bc, wc;
  twodstring statstring;
  float wscore, bscore;

  bterr = 0; /* B territory */
  wterr = 0; /* W territory */
  bocc = 0; /* B Occupied */
  wocc = 0; /* W Occupied */
  bc = 0; /* B Captured */
  wc = 0; /* W Captured */

  if (param[0].type == TYPE_NULL) {
    if (parray[p0].session.gnum >= 0) {
      g0 = parray[p0].session.gnum;
    } else {			/* Do observing in here */
      if (parray[p0].session.num_observe) {
        g0 = parray[p0].session.observe_list[0];
      } else {
        pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NEITHER_PLAYING_NOR_OBSERVING_A_GAME_);
        return COM_OK;
      }
    }

    mink_countscore(garray[g0].minkg, statstring, &wterr, &bterr, &wocc, &bocc);
    mink_getcaps(garray[g0].minkg, &wc, &bc);
    if (garray[g0].komi > 0) {
      wscore = wterr + wocc + garray[g0].komi;
      bscore = bterr + bocc;
    } else {
      wscore = wterr + wocc;
      bscore = bterr + bocc - garray[g0].komi; /* PEM: komi <= 0 */
    }
    pcn_out(p0, CODE_INFO, FORMAT_CHINESE_WHITE_f_BLACK_fn, wscore, bscore);

    if (garray[g0].komi > 0) {
      wscore = wterr + bc + garray[g0].komi;
      bscore = bterr + wc;
    } else {
      wscore = wterr + bc;
      bscore = bterr + wc - garray[g0].komi; /* PEM: komi <= 0 */
    }
    pcn_out(p0, CODE_INFO, FORMAT_JAPANESE_WHITE_f_BLACK_fn, wscore, bscore);

  } else if (param[0].type == TYPE_INT) {
    g0 = param[0].val.integer - 1;
    if (g0 < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_GAME_NUMBERS_MUST_BE_1_n);
      return COM_OK;
    }

    if (g0 >= garray_top || !garray[g0].slotstat.is_playing  || garray[g0].gstatus != GSTATUS_ACTIVE) {
      return COM_NOSUCHGAME;
    }

    mink_countscore(garray[g0].minkg, statstring, &wterr, &bterr, &wocc, &bocc);
    mink_getcaps(garray[g0].minkg, &wc, &bc);
    if (garray[g0].komi > 0) {
      wscore = wterr + wocc + garray[g0].komi;
      bscore = bterr + bocc;
    } else {
      wscore = wterr + wocc;
      bscore = bterr + bocc - garray[g0].komi; /* PEM: komi <= 0 */
    }
    pcn_out(p0, CODE_INFO, FORMAT_CHINESE_WHITE_f_BLACK_fn, wscore, bscore);

    if (garray[g0].komi > 0) {
      wscore = wterr + bc + garray[g0].komi;
      bscore = bterr + wc;
    } else {
      wscore = wterr + bc;
      bscore = bterr + wc - garray[g0].komi; /* PEM: komi <= 0 */
    }
    pcn_out(p0, CODE_INFO, FORMAT_JAPANESE_WHITE_f_BLACK_fn, wscore, bscore);
  } else {
    return COM_BADPARAMETERS;
  }
  return COM_OKN;
}

int com_refresh(int p0, struct parameter * param)
{
  int idx, gnum;
  if (param[0].type == TYPE_NULL) {
    gnum = parray[p0].session.gnum;
    if (gnum >= 0) {
      if (garray[gnum].gotype >= GAMETYPE_GO) {
        if (!parray[p0].flags.is_client || parray[p0].i_verbose)
          send_go_board_to(gnum, p0);
        if (parray[p0].flags.is_client) pcommand(p0, "moves %d", gnum+1);
      }
    } else {			/* Do observing in here */
      if (parray[p0].session.num_observe) {
	for (idx = 0; idx < parray[p0].session.num_observe; idx++) {
          gnum = parray[p0].session.observe_list[idx];
          if (garray[gnum].gotype >= GAMETYPE_GO) {
            if (!parray[p0].flags.is_client || parray[p0].i_verbose)
              send_go_board_to(gnum, p0);
	    if (parray[p0].flags.is_client)
              pcommand(p0, "moves %d", gnum + 1);
          }
	}
      } else
	pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NEITHER_PLAYING_NOR_OBSERVING_A_GAME_n );
    }
  } else if (param[0].type == TYPE_INT) {
    gnum = param[0].val.integer - 1;
    if (gnum < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_GAME_NUMBERS_MUST_BE_1_n);
    } else if (gnum >= garray_top || !garray[gnum].slotstat.is_playing || garray[gnum].gstatus != GSTATUS_ACTIVE) {
      pcn_out(p0, CODE_ERROR, FORMAT_NO_SUCH_GAME_n);
    } else {
        if (!parray[p0].flags.is_client || parray[p0].i_verbose) send_go_board_to(gnum, p0);
	if (parray[p0].flags.is_client) pcommand(p0, "moves %d", gnum +1);
    }
  } else {
    return COM_BADPARAMETERS;
  }
  return COM_OKN;
}

	/* FIXME: could these be replaced by global aliases ? AvK */
int com_open(int p0, struct parameter * param)
{
  int retval;
  UNUSED(param);

  if ((retval = pcommand(p0, "set open")) != COM_OK)
    return retval;
  else
    return COM_OK_NOPROMPT;
}

int com_rank(int p0, struct parameter * param)
{
  int retval;

  if ((retval = pcommand(p0, "set rank %s", param[0].val.string)) != COM_OK)
    return retval;
  else
    return COM_OK_NOPROMPT;
}

int com_ranked(int p0, struct parameter * param)
{
  int len, level, rat;
  char temp[10], trnk;

  len = strlen(param[0].val.string);
  if (len > 3 || len < 2) {
    pcn_out(p0, CODE_ERROR, FORMAT_INVALID_RANK_VALID_RANKS_ARE_30K_1K_1D_6D_);
    return COM_OK;
  }
  do_copy(temp, param[0].val.string, sizeof temp);
  stolower(temp);

  switch(temp[1]) {
    case '?' :
      do_copy(parray[p0].ranked, "???", sizeof parray[0].ranked);
      do_copy(parray[p0].srank, "???", sizeof parray[0].srank);
      parray[p0].rating = 1;
      parray[p0].orating = 1;
      return COM_OK;
    case 'r' :
      do_copy(parray[p0].ranked, "NR", sizeof parray[0].ranked);
      do_copy(parray[p0].srank, "NR", sizeof parray[0].srank);
      parray[p0].rating = 0;
      parray[p0].orating = 0;
      parray[p0].flags.is_rated = 0;
      return COM_OK;
    default: break;
  }
  len=sscanf(temp, "%d%c", &level, &trnk);
  if (len==2) {
    switch(trnk) {
    case 'p':
      if (level < 1 || level > 20) len = -1; /* :-) */
      break;
    case 'd':
      if (level < 1 || level > 6) len = -1; 
      break;
    case 'k':
      if (level > 30 || level < 1) len = -1;
      break;
    default: len = -1;
    }
  }
  if (len!=2) {
    pcn_out(p0, CODE_ERROR, FORMAT_INVALID_RANK_VALID_RANKS_ARE_30K_1K_1D_6D_);
    return COM_OK;
  }
  if (!strcmp(parray[p0].ranked, "NR")) {
    parray[p0].orating = 0;
  } else {
    rat=parse_rank(level, trnk);
    sprintf(parray[p0].ranked, "%d%c", level, trnk);
    parray[p0].orating = (rat * 100) - 100;
  }
  pcn_out(p0,CODE_INFO, FORMAT_RANK_SET_TO_sn, parray[p0].ranked);
  pcn_out(p0, CODE_INFO, FORMAT_PLEASE_EXPLAIN_YOUR_RANK_BY_USING_qSET_RANKq_ );

  return COM_OK;
}

int com_info(int p0, struct parameter * param)
{
  int retval;

  if ((retval = pcommand(p0, "set 0 %s", param[0].val.string)) != COM_OK)
    return retval;
  else
    return COM_OK_NOPROMPT;
}

int com_bell(int p0, struct parameter * param)
{
  int retval;
  UNUSED(param);

  if ((retval = pcommand(p0, "set bell")) != COM_OK)
    return retval;
  else
    return COM_OK_NOPROMPT;
}

int com_alias(int p0, struct parameter * param)
{
  char *c, *a;

  if (param[0].type == TYPE_NULL) {
    pcn_out(p0, CODE_INFO, FORMAT_YOUR_ALIASESn );
    pcn_out(p0, CODE_INFO, FORMAT_CODE_COMMANDn );
    pcn_out(p0, CODE_INFO, FORMAT_n );
    alias_start(parray[p0].alias_list);
    while (alias_next(&c, &a, parray[p0].alias_list)) {
      pcn_out(p0,CODE_INFO, FORMAT_s_arrow_sn, c, a);
    }
    return COM_OKN;
  }
  a = alias_lookup(param[0].val.word, parray[p0].alias_list);
  if (param[1].type == TYPE_NULL) {
    if (!a) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOU_HAVE_NO_ALIAS_NAMED_s_n, param[0].val.word);
    } else {
      pcn_out(p0, CODE_INFO, FORMAT_s_sn, param[0].val.word, a);
    }
  } else {
    if (alias_count(parray[p0].alias_list) >= MAX_ALIASES - 1) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOU_HAVE_YOUR_MAXIMUM_OF_d_ALIASES_n, MAX_ALIASES - 1);
    } else {

      if (!strcmp(param[0].val.string, "quit")
     || !strcmp(param[0].val.string, "unalias")) {	/* making sure they cant
							   alias some commands */
	pcn_out(p0, CODE_ERROR, FORMAT_YOU_CAN_T_ALIAS_THIS_COMMAND_n);
      } else {
	if (alias_add(param[0].val.word, param[1].val.string,
		      parray[p0].alias_list))
	  pcn_out(p0,CODE_INFO, FORMAT_ALIAS_REPLACED_n);
	else
	  pcn_out(p0,CODE_INFO, FORMAT_ALIAS_SET_n);
      }
    }
  }
  return COM_OKN;
}

int com_unalias(int p0, struct parameter * param)
{
  int x;

  x = alias_rem(param[0].val.word, parray[p0].alias_list);
  if (!x) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_HAVE_NO_ALIAS_NAMED_s_, param[0].val.word);
  } else {
    pcn_out(p0,CODE_INFO, FORMAT_ALIAS_REMOVED_);
  }
  return COM_OK;
}


int com_sendmessage(int p0, struct parameter * param)
{
  int p1;

  if (!parray[p0].slotstat.is_registered) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_ARE_NOT_REGISTERED_AND_CANNOT_SEND_MESSAGES_ );
    return COM_OK;
  }
  if (param[0].type == TYPE_NULL || param[1].type == TYPE_NULL) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_MESSAGE_SENT_);
    return COM_OK;
  }
  p1 = player_find_sloppy(param[0].val.word);
  if (p1<0 || !parray[p1].slotstat.is_registered) {
    pcn_out(p0, CODE_ERROR, FORMAT_THERE_IS_NO_PLAYER_MATCHING_THAT_NAME_n);
    player_unfix(p1);
    return COM_OK;
    }
  if (player_censored(p1, p0) && parray[p0].adminLevel < ADMIN_ADMIN) {
    pcn_out(p0, CODE_ERROR, FORMAT_PLAYER_qsq_IS_CENSORING_YOU_, parray[p1].pname);
    player_unfix(p1);
    return COM_OK;
  }
  if (player_add_message(p1, p0, param[1].val.string)) {
    pcn_out(p0, CODE_ERROR, FORMAT_COULDN_T_SEND_MESSAGE_TO_s_MESSAGE_BUFFER_FULL_,
	    parray[p1].pname);
  } else {
    pcn_out(p0,CODE_INFO, FORMAT_MESSAGE_SENT_TO_s_, parray[p1].pname);
    if (parray[p1].slotstat.is_online)
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_JUST_SENT_YOU_A_MESSAGE_n, parray[p0].pname);
  }
  player_unfix(p1);
  return COM_OK;
}

int com_messages(int p0, struct parameter * param)
{
  if (param[0].type != TYPE_NULL) {
    return com_sendmessage(p0, param);
  }
  if (player_num_messages(p0) <= 0) {
    pcn_out(p0,CODE_INFO, FORMAT_YOU_HAVE_NO_MESSAGES_);
    return COM_OK;
  }
  pcn_out(p0,CODE_INFO, FORMAT_MESSAGES_n);
  player_show_messages(p0);
  return COM_OKN;
}


int com_clearmess(int p0, struct parameter * param)
{
  UNUSED(param);
  if (player_num_messages(p0) <= 0) {
    pcn_out(p0,CODE_INFO, FORMAT_YOU_HAVE_NO_MESSAGES_);
    return COM_OK;
  }
  pcn_out(p0,CODE_INFO, FORMAT_MESSAGES_CLEARED_);
  player_clear_messages(p0);
  return COM_OK;
}


int com_adhelp(int p0, struct parameter * param)
{
#define ADHELP_BUFFER_SIZE 8000
  char filenames[ADHELP_BUFFER_SIZE];	/* enough for all helpfile names */

  char line[90];
  int count;
  char *cp=filenames ;
  int ii;
  size_t pos,len;

  if (param[0].type == TYPE_NULL) {
    count = search_index(filenames, sizeof filenames, NULL, FILENAME_AHELP_l_index, parray[p0].language);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    for (pos=0,ii = 0; ii < count; ii++) {
      if (pos) do { line[pos++] = ' '; } while(pos % 8) ;
      len = strlen(cp);
      memcpy(line+pos,cp, len+1); /* includes \0 */
      cp += len + 1;
      pos += len;
      if (pos >64) {
	pprintf(p0, "%s\n", line);
	pos=0; line[pos] = 0;
      }
    }
    if (pos) pprintf(p0, "%s\n", line);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    return COM_OK;
  }
  if (!safestring(param[0].val.word)) {
    pcn_out(p0, CODE_ERROR, FORMAT_ILLEGAL_CHARACTER_IN_COMMAND_s_n, param[0].val.word);
    return COM_OKN;
  }
  count = search_index(filenames, sizeof filenames, param[0].val.word
      , FILENAME_AHELP_l_index, parray[p0].language);
  if (count == 0) { /* no file was found */
    pcn_out(p0, CODE_ERROR, FORMAT_NO_HELP_AVAILABLE_ON_s_n, param[0].val.word);
    return COM_OKN;
  }
  if (count == 1) { /* Only one file: send it */
    if (pxysend_file(p0, FILENAME_AHELP_q_s, filenames)) {
      pcn_out(p0, CODE_ERROR, FORMAT_HELPFILE_s_COULD_NOT_BE_FOUND_, filenames);
      pcn_out(p0, CODE_ERROR, FORMAT_PLEASE_INFORM_AN_ADMIN_OF_THIS_THANK_YOU_n );
      return COM_OKN;
    }
    return COM_OK;
  }
  /* if we get here (count > 1) :: show list */
  pcn_out(p0,CODE_INFO, FORMAT_MATCHES_s, cp);
  cp = filenames;
  for (ii = 1; ii < count; ii++) {
    cp += strlen(cp) + 1;
    pprintf(p0, ", %s", cp);
  }
  pprintf(p0, ".\n");
  return COM_OK;
}


int com_help(int p0, struct parameter * param)
{
#define HELP_BUFFER_SIZE 8000
  char filenames[HELP_BUFFER_SIZE];   /* enough for all helpfile names */
  char line[90];
  int count;
  int ii;
  size_t pos;
  size_t len;
  char *cp=filenames ;

  /* if no help file is requested (i.e. 'help') then show a list of all
   * help files in the help dir */
  if (param[0].type == TYPE_NULL) {
    count = search_index(filenames, sizeof filenames, NULL, FILENAME_HELP_l_index, parray[p0].language);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    for (pos=0,ii = 0; ii < count; ii++) {
      if (pos) do { line[pos++] = ' '; } while(pos % 8) ;
      len = strlen(cp);
      memcpy(line+pos,cp, len+1); /* includes \0 */
      cp += len + 1;
      pos += len;
      if (pos >64) {
	pprintf(p0, "%s\n", line);
	pos=0; line[pos] = 0;
      }
    }
    if (pos) pprintf(p0, "%s\n", line);

    pprintf(p0, "[Type \"help overview\" for a list of %s general information files.]\n", conffile.server_name);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    return COM_OK;
  }
  if (safestring(param[0].val.word)) {

    count = search_index(filenames, sizeof filenames, param[0].val.word, FILENAME_HELP_l_index, parray[p0].language);
    if (count > 0) {
      if (count > 1 && strcmp(filenames, param[0].val.word)) {
	cp=filenames;
	pcn_out(p0,CODE_INFO, FORMAT_MATCHES_s, cp);
	for (ii = 1; ii < count; ii++) {
	  cp += strlen(cp) + 1;
	  pprintf(p0, ", %s", cp);
	}
	pprintf(p0, ".\n");
	return COM_OK;
      }
      if (pxysend_file(p0, FILENAME_HELP_q_s, filenames)) {
	/* we should never reach this unless the file was just deleted */
	pcn_out(p0, CODE_ERROR, FORMAT_HELPFILE_s_COULD_NOT_BE_FOUND_, filenames);
	pcn_out(p0, CODE_ERROR, FORMAT_PLEASE_INFORM_AN_ADMIN_OF_THIS_THANK_YOU_n);
      }
    } else {
      /* if we get here no file was found */
      pcn_out(p0, CODE_ERROR, FORMAT_NO_HELP_AVAILABLE_ON_s_n, param[0].val.word);
    }
  } else {
    pcn_out(p0, CODE_ERROR, FORMAT_ILLEGAL_CHARACTER_IN_COMMAND_s_n, param[0].val.word);
  }
  return COM_OKN;
}


int com_infor(int p0, struct parameter * param)
{
  UNUSED(param);

  if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
  xpsend_dir(p0, FILENAME_INFO);
  if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
  return COM_OK;
}


int com_reset(int p0, struct parameter * param)
{
  UNUSED(param);
  parray[p0].gowins = 0;
  parray[p0].golose = 0;
  parray[p0].gonum_white = 0;
  parray[p0].gonum_black = 0;
  pcn_out(p0,CODE_INFO, FORMAT_RESET_YOUR_STATS_TO_0);
  return COM_OK;
}



int com_mailmess(int p0, struct parameter * param)
{
  char fname[MAX_FILENAME_SIZE];
  UNUSED(param);

  if (!player_num_messages(p0)) {
    pcn_out(p0,CODE_INFO, FORMAT_YOU_HAVE_NO_MESSAGES_TO_MAIL_n);
    return COM_OK;
  }
  xyfilename(fname, FILENAME_PLAYER_cs_MESSAGES, parray[p0].login );

  mail_asn(parray[p0].email, "NNGS Messages", fname);
  pcn_out(p0, CODE_INFO, FORMAT_YOUR_MESSAGES_WERE_SENT_TO_sn, parray[p0].email);
  return COM_OKN;
}


int com_mailhelp(int p0, struct parameter *param)
{
  char buffer[10000];
  char subj[MAX_FILENAME_SIZE+10];
  char fname[MAX_FILENAME_SIZE];
  int count;
  UNUSED(param);

  if (param[0].type == TYPE_NULL) {
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    xpsend_dir(p0, FILENAME_HELP_p);
    if (parray[p0].flags.is_client) pcn_out(p0, CODE_HELP, FORMAT_FILEn);
    return COM_OK;
  }

  count = search_directory(buffer, sizeof buffer, param[0].val.word, FILENAME_HELP_p);

  if (count != 1) {
    pcn_out(p0, CODE_ERROR, FORMAT_FOUND_d_FILES_MATCHING_THAT_n, count);
    return COM_OKN;
  }
#if 0
  sprintf(command, "/bin/mail -s \"NNGS helpfile: %s\" %s < %s%s&"
  , param[0].val.word, parray[p0].email, filename() , param[0].val.word);
  system(command);
#else
  {
  sprintf(fname, "%s/%s", filename() , buffer);
  sprintf(subj, "NNGS helpfile: %s", buffer);
  mail_asn(parray[p0].email, subj, fname);
  }
#endif
  pcn_out(p0, CODE_INFO, FORMAT_THE_FILE_s_WAS_SENT_TO_sn
  , param[0].val.word, parray[p0].email);
  return COM_OKN;

}

int com_mailme(int p0, struct parameter * param)
{
  int ii;
  char *arg;
  char fname[MAX_FILENAME_SIZE];

  if (strcmp(param[0].val.word, "me"))
    return COM_BADPARAMETERS;

  arg = getword(eatwhite(param[1].val.string));

  if (!safefilename(arg)) {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_YOU_DON_T_);
    return COM_OK;
  }
  ii = xyfilename(fname, FILENAME_CGAMES_cs, arg );

  ii = mail_asn(parray[p0].email, arg, fname);
  if (ii>=0) {
    pcn_out(p0,CODE_INFO, FORMAT_s_MAILED_TO_s_, arg, parray[p0].email);
  }
  else {
    pcn_out(p0, CODE_ERROR, FORMAT_EITHER_THE_FILE_WAS_NOT_FOUND_OR_YOUR_EMAIL_ADDRESS_IS_INVALID_);
  }
  return COM_OK;
}

int com_handles(int p0, struct parameter * param)
{
  char buffer[8000];
  int count;

  count = search_directory(buffer, sizeof buffer,param[0].val.word, FILENAME_PLAYER);
  pcn_out(p0,CODE_INFO, FORMAT_FOUND_d_NAMES_, count);
  if (count > 0) {
    display_directory(p0, buffer, count);
  }
  if (count == 0) pprintf(p0, "\n");
  return COM_OKN;
}

int com_which_client(int p0, struct parameter * param)
{
  parray[p0].which_client = param[0].val.integer;

  pcn_out(p0,CODE_INFO, FORMAT_CLIENT_TYPE_CHANGED_TO_);
  switch(parray[p0].which_client) {
    case CLIENT_UNKNOWN: pprintf(p0, "Unknown\n"); break;
    case CLIENT_TELNET: pprintf(p0, "Telnet\n"); break;
    case CLIENT_XIGC: pprintf(p0, "Xigc\n"); break;
    case CLIENT_WINIGC: pprintf(p0, "WinIGC\n"); break;
    case CLIENT_WIGC: pprintf(p0, "WIGC\n"); break;
    case CLIENT_CGOBAN: pprintf(p0, "CGoban\n"); break;
    case CLIENT_JAVA: pprintf(p0, "Java\n"); break;
    case CLIENT_TGIGC: pprintf(p0, "Tgigc\n"); break;
    case CLIENT_TGWIN: pprintf(p0, "TgWin\n"); break;
    case CLIENT_FIGC: pprintf(p0, "Figc\n"); break;
    case CLIENT_PCIGC: pprintf(p0, "PCigc\n"); break;
    case CLIENT_GOSERVANT: pprintf(p0, "GoServant\n"); break;
    case CLIENT_MACGO: pprintf(p0, "MacGo\n"); break;
    case CLIENT_AMIGAIGC: pprintf(p0, "AmigaIgc\n"); break;
    case CLIENT_HAICLIENT: pprintf(p0, "Hai Client\n"); break;
    case CLIENT_IGC: pprintf(p0, "IGC\n"); break;
    case CLIENT_KGO: pprintf(p0, "Kgo\n"); break;
    case CLIENT_NEXTGO: pprintf(p0, "NextGo\n"); break;
    case CLIENT_OS2IGC: pprintf(p0, "OS/2igc\n"); break;
    case CLIENT_STIGC: pprintf(p0, "StIgc\n"); break;
    case CLIENT_XGOSPEL: pprintf(p0, "XGospel\n"); break;
    case CLIENT_JAGOCLIENT: pprintf(p0, "Jago Client\n"); break;
    case CLIENT_GTKGO: pprintf(p0, "gtkgo\n"); break;
    default: pprintf(p0, "Unknown\n"); break;
  }
  return COM_OK;
}

int com_qtell(int p0, struct parameter * param)
{
  int p1;
  char tmp[MAX_STRING_LENGTH];
  char *src,*dst;

  stolower(param[0].val.word);
  p1 =  player_find_login(param[0].val.word);
  if (p1 < 0) {
    pcn_out(p0,CODE_INFO, FORMAT_QTELL_s_1_, param[0].val.word);
    return COM_OK;
  }

  /* Exchange "\n" in string to a '\n' char */

  sprintf(tmp, "%s", param[1].val.string);
  for (src=dst=tmp; (*dst = *src++); dst++) {
    if (*dst=='\\' && *src == 'n') { *dst = '\n';  src++; }
  }
  pcn_out_prompt(p1, CODE_INFO, FORMAT_s_s, parray[p0].pname, tmp);
  pcn_out(p0,CODE_INFO, FORMAT_QTELL_s_0_, parray[p1].pname);
  return COM_OK;
}

int com_spair(int p0, struct parameter * param)
{
  int pp4[4], p1, idx;

  for(idx=0; idx < 4; idx++) {
    stolower(param[idx].val.word);
    pp4[idx] = p1= player_find_part_login(param[idx].val.word);
    if (p1 < 0) {
      pcn_out(p0, CODE_ERROR, FORMAT_NO_USER_NAMED_qsq_IS_LOGGED_IN_,
       param[idx].val.word);
      return COM_OK;
    }
  }

  for(idx=0; idx < 3; idx++) {
    for(p1=idx+1; p1 < 4; p1++) {
      if (pp4[idx] == pp4[p1]) break; }}

  if (p1 < 4) {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_MUST_LIST_4_UNIQUE_PLAYER_NAMES_TO_PLAY_PAIR_GO_);
    return COM_OK;
  }

  for(idx=0; idx < 4; idx++) {
    p1=pp4[idx];
    if (!strcmp(parray[p0].pname, parray[p1].pname)) break;
  }
  if (idx < 4)  {
    pcn_out(p0, CODE_ERROR, FORMAT_YOU_MUST_BE_ONE_OF_THE_FOUR_PLAYERS_LISTED_IN_THE_SPAIR_COMMAND);
    return COM_OK;
  }

  for(idx=0; idx < 4; idx++) {
    if (parray[pp4[idx]].session.gnum >= 0) break;
  }
  if (idx < 4) {
    pcn_out(p0, CODE_ERROR, FORMAT_ONE_OF_THE_4_LISTED_PLAYERS_IS_ALREADY_PLAYING_A_GAME_);
    return COM_OK;
  }
  for(idx=0; idx < 4; idx++) {
    int pa,pp,pc; /* player, buddy, opponent */
    pa=pp4[idx];
    pp=pp4[idx^1];
    pc=pp4[idx^2];
    pcn_out(pa,CODE_INFO, FORMAT_WELCOME_TO_NNGS_PAIR_GO_n);
    if (idx < 2) {	/* White players */
      pcn_out(pa, CODE_INFO, FORMAT_YOU_AND_s_ARE_THE_WHITE_TEAM_n, parray[pp].pname);
      pcn_out(pa, CODE_INFO, FORMAT_FIRST_CREATE_A_MATCH_AGAINST_YOUR_OPPONENT_s_n, parray[pc].pname);
      pcn_out(pa, CODE_INFO, FORMAT_TYPE_MATCH_s_W_SIZE_TIME_BYO_TIMEn, parray[pc].pname);
      pcn_out(pa,CODE_INFO, FORMAT_ONCE_YOUR_MATCH_IS_CREATED_CHECK_TO_SEE_WHAT_s_S_GAME_NUMBER_IS_n, parray[pp].pname);
      pcn_out(pa, CODE_INFO, FORMAT_THEN_TYPE_qPAIR_q_WHERE_IS_s_S_GAME_NUMBER_n, parray[pp].pname);
    } else {	/* Black players */
      pcn_out(pa, CODE_INFO, FORMAT_YOU_AND_s_ARE_THE_BLACK_TEAM_n, parray[pp].pname);
      pcn_out(pa,CODE_INFO, FORMAT_IMPORTANT_TO_REMEMBER_DO_NOT_MOVE_UNTIL_AFTER_THE_GAMES_ARE_PAIRED_n);
      pcn_out(pa, CODE_INFO, FORMAT_YOUR_OPPONENT_s_WILL_FIRST_REQUEST_A_MATCH_WITH_YOU_n, parray[pc].pname);
      pcn_out(pa,CODE_INFO, FORMAT_SIT_BACK_AND_WAIT_FOR_THE_MESSAGE_qGAME_HAS_BEEN_PAIREDq_THEN_MOVE_n);
  
    }
  pcn_out_prompt(pa, CODE_INFO, FORMAT_GOOD_LUCK_n);
  }
  return COM_OKN;
}

int com_nrating(int p0, struct parameter * param)
{
#if WANT_NNGSRATED
  char name[sizeof parray[0].pname];
  rdbm_t db;
  rdbm_player_t rp;
  char ratstr[32];
  char fname[MAX_FILENAME_SIZE];

  strcpy(ratstr, "No Confidence");

  if (param[0].type == TYPE_NULL)
    do_copy(name, parray[p0].pname, sizeof name);
  else
    do_copy(name, param[0].val.word, sizeof name);

  xyfilename(fname, FILENAME_NRATINGS );
  if (!(db = rdbm_open(fname,0)))
  {
    pcn_out(p0, CODE_ERROR, FORMAT_AN_UNLUCKY_CHANCE_OCCUREDn);
    return COM_OK;
  }
  if (!rdbm_fetch(db, name, &rp))
    pcn_out(p0, CODE_ERROR, FORMAT_NO_RATING_INFORMATION_FOR_sn, name);
  else {
    if ((rp.high-rp.low) < 0.25f) {
      strcpy(ratstr, "Highest confidence");
    } else if ((rp.high-rp.low) < 0.50f) {
      strcpy(ratstr, "Very high confidence");
    } else if ((rp.high-rp.low) < 0.75f) {
      strcpy(ratstr, "High confidence");
    } else if ((rp.high-rp.low) < 1.00f) {
      strcpy(ratstr, "Low confidence");
    } else {
      strcpy(ratstr, "Very low confidence");
    }
    pcn_out(p0, CODE_INFO, FORMAT_s_sc_f_n , rp.name, rp.rank, (rp.star ? '*' : ' '), rp.rating);
    pcn_out(p0, CODE_INFO, FORMAT_RANGE_f_STONES_f_f_sn, rp.high-rp.low, rp.low, rp.high, ratstr);
    pcn_out(p0, CODE_INFO, FORMAT_RATED_GAMES_u_u_WINS_u_LOSSES_n, rp.wins+rp.losses, rp.wins, rp.losses);
  }
  rdbm_close(db);
#else
  UNUSED(p0);
  UNUSED(param);
#endif /* WANT_NNGSRATED */
  return COM_OK;
}

int com_orating(int p0, struct parameter * param)
{
  char line[180];
  char neeldle[sizeof parray[0].pname];
  FILE *fp;
  char name[11], rank[10];
  char junk1[5], junk2[5];
  int rating, numgam;
  float numeric_rank, confidence, range_high, range_low;
  int all_rated_wins, all_rated_losses,
      rated_wins_vs_rated, rated_losses_vs_rated,
      high_confidence_wins, high_confidence_losses;
  int found = 0;

  if (param[0].type == TYPE_NULL) {
    do_copy(neeldle, parray[p0].pname, sizeof neeldle);
  }
  else do_copy(neeldle, param[0].val.word, sizeof neeldle);

  fp = xyfopen(FILENAME_RATINGS, "r");
  if (!fp)
  {
    pcn_out(p0, CODE_ERROR, FORMAT_AN_UNKNOWN_ERROR_OCCURED_OPENING_THE_RATINGS_FILE_n);
    fprintf(stderr, "An unknown error occured opening the ratings file '%s'.\n"
	, filename() );
    return COM_OK;
  }

  while((fscanf(fp, "%s %s %d %d", name, rank, &rating, &numgam)) == 4)
  {
    if (!strcasecmp(name, neeldle))
    {
      pcn_out(p0,CODE_INFO,FORMAT_s_s_d_RATED_GAMES_dn,
                name, rank, rating, numgam);
      break;
    }
  }
  fclose(fp);
  fp = xyfopen(FILENAME_RATINGS, "r");
  if (!fp) {
    pcn_out(p0, CODE_ERROR, FORMAT_AN_UNKNOWN_ERROR_OCCUREDn);
    return COM_OK;
  }

  while (fgets(line, sizeof line, fp))
  {
    if (sscanf(line, "%s %f %s %f %f/%f %d-%d %d-%d %d-%d",
                     name, &numeric_rank,
                     rank, &confidence,
                     &range_high, &range_low,
                     &all_rated_wins, &all_rated_losses,
                     &rated_wins_vs_rated, &rated_losses_vs_rated,
                     &high_confidence_wins, &high_confidence_losses) == 12)
    {
      if (!strcasecmp(name, neeldle))
      {
        found = 1;
        pcn_out(p0, CODE_INFO, FORMAT_CONFIDENCE_f_PERCENTn, confidence * 100);
        pcn_out(p0, CODE_INFO, FORMAT_ALL_RATED_WINS_dn, all_rated_wins);
        pcn_out(p0, CODE_INFO, FORMAT_ALL_RATED_LOSSES_dn, all_rated_losses);
        pcn_out(p0, CODE_INFO, FORMAT_RATED_WINS_VERSUS_RATED_PLAYERS_dn, rated_wins_vs_rated);
        pcn_out(p0, CODE_INFO, FORMAT_RATED_LOSSES_VERSUS_RATED_PLAYERS_dn, rated_losses_vs_rated);
        pcn_out(p0, CODE_INFO, FORMAT_HIGH_CONFIDENCE_WINS_dn, high_confidence_wins);
        pcn_out(p0, CODE_INFO, FORMAT_HIGH_CONFIDENCE_LOSSES_dn, high_confidence_losses);
        break;
      }
    }
    else if (sscanf(line, "%s %s %s %f %s %d-%d %d-%d %d-%d",
                     name, junk1,
                     rank, &confidence,
                     junk2,
                     &all_rated_wins, &all_rated_losses,
                     &rated_wins_vs_rated, &rated_losses_vs_rated,
                     &high_confidence_wins, &high_confidence_losses) == 11)
    {
      if (!strcasecmp(name, neeldle)) {
        found = 1;
        pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOT_RATEDn, name);
        pcn_out(p0, CODE_INFO, FORMAT_ALL_RATED_WINS_dn, all_rated_wins);
        pcn_out(p0, CODE_INFO, FORMAT_ALL_RATED_LOSSES_dn, all_rated_losses);
        pcn_out(p0, CODE_INFO, FORMAT_RATED_WINS_VERSUS_RATED_PLAYERS_dn, rated_wins_vs_rated);
        pcn_out(p0, CODE_INFO, FORMAT_RATED_LOSSES_VERSUS_RATED_PLAYERS_dn, rated_losses_vs_rated);
        pcn_out(p0, CODE_INFO, FORMAT_HIGH_CONFIDENCE_WINS_dn, high_confidence_wins);
        pcn_out(p0, CODE_INFO, FORMAT_HIGH_CONFIDENCE_LOSSES_dn, high_confidence_losses);
        break;
      }
    }
  } /* end while() loop */
  fclose(fp);
  if (!found) pcn_out(p0,CODE_INFO, FORMAT_NO_RATING_INFORMATION_FOR_sn, neeldle);
  return COM_OK;
}

int com_translate(int p0, struct parameter * param)
{
#define BUFSIZE 1024
  char cmd[1000];
  char buf[BUFSIZE];
  FILE *ptr;

  if (!safestring(param[0].val.word)) {
    pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOUR_QUERY_CONTAINS_INVALID_CHARACTERS_);
    return COM_OK;
  }

  sprintf(cmd, "%s -ta -la %s", conffile.intergo_file, param[0].val.word);

  if ((ptr = popen(cmd, "r")) ) {
    while (fgets(buf, sizeof buf, ptr) )
      pcn_out(p0, CODE_TRANS, FORMAT_s, buf);
    pclose(ptr);
  }
  return COM_OKN;
}

int com_ping(int p0, struct parameter * param)
{
  UNUSED(param);
  pcn_out(p0, CODE_PING, FORMAT_PONG );
  return COM_OK;
}

int com_find(int p0, struct parameter * param)
{
  char buf[MAX_LINE_SIZE];
  int i;
  FILE *fp;
  const struct searchresult *psr;

  bldsearchdata(param[0].val.string);

  if ((fp = xyfopen(FILENAME_FIND, "r")) ) {
    while (fgets(buf, sizeof buf, fp)) {
      i = strlen(buf);
      if (i < 1) continue;
      buf[i - 1] = '\0';
      if (is_totally_blank(buf)) continue;
      psr = search(buf);

        if (psr) {
          pcn_out(p0, CODE_INFO, FORMAT_MATCHED_PLAYER_s_AT_ADDRESS_sn,
                        psr->szPlayer, psr->szMailAddr);
      }
    }
    fclose(fp);
  }
  return COM_OKN;
}

#if WANT_NOTIFY
int com_notify(int p0, struct parameter * param)
{
  int i, p1;

  if (param[0].type != TYPE_WORD) {
    if (!parray[p0].num_notify) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOUR_NOTIFY_LIST_IS_EMPTY_n);
      return COM_OK;
    }
    else {
      pcn_out(p0, CODE_INFO, FORMAT_YOUR_NOTIFY_LIST_CONTAINS_d_NAMES_n,
                  parray[p0].num_notify);

      for(i = 0; i < parray[p0].num_notify; i++) {
        pcn_out(p0,CODE_INFO, FORMATnotify_sn, parray[p0].notifyList[i]);
      }
      return COM_OK;
    }
  }
  else if (param[0].type == TYPE_WORD) {
    if (parray[p0].num_notify >= MAX_NOTIFY) {
      pcn_out(p0, CODE_ERROR, FORMAT_SORRY_YOUR_NOTIFY_LIST_IS_ALREADY_FULL_n);
      return COM_OK;
    }
    p1 = player_find_sloppy(param[0].val.word);
    if (p1<0) return COM_OK;
    for (i = 0; i < parray[p0].num_notify; i++) {
      if (!strcasecmp(parray[p0].notifyList[i], parray[p1].pname)) {
        pcn_out(p0, CODE_ERROR, FORMAT_YOUR_NOTIFY_LIST_ALREADY_INCLUDES_s_n, parray[p1].pname);
        player_unfix(p1);
        return COM_OK;
      }
    }
    if (p1 == p0) {
      pcn_out(p0, CODE_ERROR, FORMAT_YOU_CAN_T_NOTIFY_YOURSELF_n);
      player_unfix(p1);
      return COM_OK;
    }
    parray[p0].notifyList[parray[p0].num_notify++] = mystrdup(parray[p1].pname);
    pcn_out(p0, CODE_INFO, FORMAT_s_IS_NOW_ON_YOUR_NOTIFY_LIST_n,
                  parray[p1].pname);
    player_unfix(p1);
    return COM_OK;
  }
  return COM_BADPARAMETERS;
}

int com_unnotify(int p0, struct parameter * param)
{
  char *pname = NULL;
  int i;
  int unc = 0;

  if (param[0].type == TYPE_WORD) {
    pname = param[0].val.word;
  }
  for (i = 0; i < parray[p0].num_notify; i++) {
    if (!pname || !strcasecmp(pname, parray[p0].notifyList[i])) {
      pcn_out(p0, CODE_INFO, FORMAT_s_IS_REMOVED_FROM_YOUR_NOTIFY_LIST_n,
                parray[p0].notifyList[i]);
      free(parray[p0].notifyList[i]);
      parray[p0].notifyList[i] = NULL;
      if (i == (parray[p0].num_notify) - 1) {
        parray[p0].num_notify--;
      }
      unc++;
    }
  }
  if (unc) {
    for (i = 0; i < parray[p0].num_notify; i++) {
      if (!parray[p0].notifyList[i]) {
        if (parray[p0].notifyList[(parray[p0].num_notify) - 1]) {
          parray[p0].notifyList[i] = parray[p0].notifyList[(parray[p0].num_notify) - 1];
          i = i - 1;
          parray[p0].num_notify = parray[p0].num_notify - 1;
        }
      }
    }
  } else {
    pcn_out(p0, CODE_ERROR, FORMAT_NO_ONE_WAS_REMOVED_FROM_YOUR_NOTIFY_LIST_n );
  }
  return COM_OK;
}

int com_znotify(int p0, struct parameter * param)
{
  int p1, count = 0;

  for (p1 = 0; p1 < parray_top; p1++) {
    if (player_notified(p0, p1)) {
      if (!count) {
        pcn_out(p0, CODE_INFO, FORMAT_PRESENT_COMPANY_ON_YOUR_NOTIFY_LIST_n );
        pcn_out(p0, CODE_INFO, FORMAT_empty );
      }
      pprintf(p0, " %s", parray[p1].pname);
      count++;
    }
  }
  if (count)
    pprintf(p0, ".\n");
  else
    pcn_out(p0, CODE_INFO, FORMAT_NO_ONE_FROM_YOUR_NOTIFY_LIST_IS_LOGGED_ON_n);

  count = 0;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (player_notified(p1, p0)) {
      if (!count) {
        pcn_out(p0, CODE_INFO, FORMAT_THE_FOLLOWING_PLAYERS_HAVE_YOU_ON_THEIR_NOTIFY_LIST_n );
        pcn_out(p0, CODE_INFO, FORMAT_empty );
      }
      pprintf(p0, " %s", parray[p1].pname);
      count++;
    }
  }
  if (count)
    pprintf(p0, ".\n");
  else
    pcn_out(p0, CODE_INFO, FORMAT_NO_ONE_LOGGED_IN_HAS_YOU_ON_THEIR_NOTIFY_LIST_n);
  return COM_OK;
}


int player_notified(int p0, int p1)
/* is p1 on p's notify list? */
{
  int i;

  for (i = 0; i < parray[p0].num_notify; i++) {
    if (!parray[p1].slotstat.is_online) continue;
    if (!strcasecmp(parray[p0].notifyList[i], parray[p1].pname)) return 1;
  }
  return 0;
}


void player_notify_departure(int p0)
/* Notify those with notifiedby set on a departure */
{
  int p1;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (parray[p1].forget.notifiedby
      && !player_notified(p1, p0)
      && player_notified(p0, p1)) {
      if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_Gn);
      pcn_out_prompt(p1, CODE_INFO, FORMAT_NOTIFICATION_s_HAS_DEPARTED_AND_ISN_T_ON_YOUR_NOTIFY_LIST_n, parray[p0].pname);
    }
  }
}


int player_notify_present(int p0)
/* output Your arrival was notified by..... */
/* also notify those with notifiedby set if necessary */
{
  int p1;
  int count = 0;

  for (p1 = 0; p1 < parray_top; p1++) {
    if (player_notified(p0, p1)) {
      if (!count) {
        pcn_out(p0,CODE_INFO, FORMAT_PRESENT_COMPANY_INCLUDES_);
      }
      count++;
      pprintf(p0, " %s", parray[p1].pname);
      if (parray[p1].forget.notifiedby && !player_notified(p1, p0)) {
        if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_007N);
        pcn_out_prompt(p1, CODE_ERROR, FORMAT_NOTIFICATION_s_HAS_ARRIVED_AND_ISN_T_ON_YOUR_NOTIFY_LIST_n, parray[p0].pname);
      }
    }
  }
  if (count)
    pprintf(p0, ".\n");
  return count;
}

int player_notify(int p0, char *note1, char *note2)
/* notify those interested that p has arrived/departed */
{
  int p1;
  int count = 0;

  for (p1 = 0; p1 < parray_top; p1++) {
    if (player_notified(p1, p0)) {
      if (parray[p1].flags.want_bell) pcn_out(p1, CODE_BEEP, FORMAT_007N);
      pcn_out_prompt(p1, CODE_INFO, FORMAT_NOTIFICATION_s_HAS_s_n, parray[p0].pname, note1);
      if (!count) {
        pcn_out(p0,CODE_INFO, FORMAT_YOUR_s_WAS_NOTED_BY_, note2);
      }
      count++;
      pprintf(p0, " %s", parray[p1].pname);
    }
  }
  if (count)
    pprintf(p0, ".\n");
  return count;
}
#endif /* WANT_NOTIFY */


/**************************************************************************
 * automatch.c               by Erik Ekholm                Created 960305 *
 **************************************************************************/

/* NOTE Avk: This will go away.
** automatch seemed to #define it's own constants for KOMIxx
** Instead of #defining them, I try to hook them to conffile.default_komiXX.
*/

/* KOMIn is the standard komi for an even game on an nxn board. The value of
   one stone is twice as much. */

#ifdef DEFAULT_KOMI19
#define KOMI19 (conffile.default_komi19)
#else
#define KOMI19 5.5 /*standard komi = 5.5 => one stone is worth 11 points
                     From this follows that each komi point corresponds
                     to a 9 (100/11) point rating difference */
#endif

#ifdef DEFAULT_KOMI13
#define KOMI13 (conffile.default_komi13)
#else
#define KOMI13 8.5 /* From the Go FAQ. (Note BTW that the table in the FAQ
                     isn't quite consistent; the gap between kyu diff.
                     3 and 4 is much larger than between 2 and 3.) */
#endif

#ifdef DEFAULT_KOMI9
#define KOMI9 (conffile.default_komi9)
#else
#define KOMI9  5.5 /* 5.5 according to some pro-pro games. */
#endif



/* STONEPTSn is how many points in the rating system a stone on an nxn board
   corresponds to. The figures for smaller boards could be changed to fit
   statistical data better. */

#define STONEPTS19 100    /* 100 pts = one grade at NNGS */
#define STONEPTS13 300    /* 1 stone at 13x13 is 3 kyu grades, see Go FAQ. */
#define STONEPTS9  600    /* My best guess */


/* THRESHOLDn decides at which rating difference handicap is increased.
   Avoiding negative komi might be a good idea, since it makes it harder
   for Black to succeed with a mirror go strategy. */

#define THRESHOLD19 0  /* Values from 0 to 5 possible. 5 avoids neg. komi. */
#define THRESHOLD13 0  /* 0 - 8 possible. 8 avoids neg. komi */
#define THRESHOLD9  0  /* 0 - 5 possible. 5 avoids neg. komi */


/* OFFSETn is used for eliminating round-off errors */

#define OFFSET19 ((STONEPTS19 + 2 * KOMI19) / (KOMI19 * 4))
#define OFFSET13 ((STONEPTS13 + 2 * KOMI13) / (KOMI13 * 4))
#define OFFSET9  ((STONEPTS9  + 2 * KOMI9)  / (KOMI9  * 4))


/*---------------------------------------------------------------------------
 * AutoMatch sets stones and komi given positive ratingdiff (in NNGS rating
 * points) and boardsize (for 19, 13 and 9), so that both players have as
 * close to 50% chance of winning as possible.
 *
 * Please note that an even game is represented as a game with 1 stone 5.5
 * komi, thus NOT a zero stone game
 */

void AutoMatch(int ratingdiff, int boardsize, int *stones, float *komi)
{
  int units;  /* help variable, number of total komi units. */

  switch (boardsize) {

  default:
  case 19:
    units = (ratingdiff + OFFSET19) / (STONEPTS19 / (KOMI19 * 2.0));
    *stones = 1 + (units + THRESHOLD19) / (KOMI19 * 2);
    *komi = KOMI19 - units + (KOMI19 * 2) * (*stones - 1);
    break;

  case 13:
    units = (ratingdiff + OFFSET13) / (STONEPTS13 / (KOMI13 * 2.0));
    *stones = 1 + (units + THRESHOLD13) / (KOMI13 * 2);
    *komi = KOMI13 - units + (KOMI13 * 2) * (*stones - 1);
    break;

  case 9:
    units = (ratingdiff + OFFSET9) / (STONEPTS9 / (KOMI9 * 2.0));
    *stones = 1 + (units + THRESHOLD9) / (KOMI9 * 2);
    *komi = KOMI9 - units + (KOMI9 * 2) * (*stones - 1);
    break;
  }
}

/* I have not approached the question of how to deal with rating differences
   of more than 9 stones.  Solutions: 1) Allowed, use free or default
   placement.  2) Use max 9 stones, but larger komi. 3) Truncate at 9
   stones. 4) Let the match command reject such automatch requests.  */


static int index_lookup(char* found , char * find)
{
  FILE *fp;
  char linebuff[80];
  int rights;
  int namelen;

  /* Check for a match in the index file */

  fp = xyfopen(FILENAME_LISTINDEX, "r");
  if (!fp) return -2;

  rights= -1;

  namelen=strlen(find);
  while ((fgets(linebuff,sizeof linebuff,fp))) {
    if (sscanf(linebuff, "%s %d", found, &rights) != 2) {
      Logit("bad format in index\"%s\": %s", filename(), linebuff);
      continue;
    }
    if (!strncmp(found, find, namelen)) break;
    rights= -1;
  }
  fclose(fp);
  return rights;
}
