/* adminproc.c
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsconfig.h"
#include "conffile.h"
#include "nngsmain.h"
#include "adminproc.h"
#include "common.h"
#include "network.h"
#include "servercodes.h"
#include "channel.h"
#include "playerdb.h"
#include "utils.h"
#include "ladder.h"
#include "files.h"
#include "ip_ban.h"

int com_adrop(int p, struct parameter* param)
{
  FILE *fp;
  const struct ladderplayer *lp;
  char *name = param[0].val.string;
  int lnum = param[1].val.integer;

  switch(lnum) {
  case 9:
  case 19: break;
  default:
    pcn_out(p, CODE_ERROR, FORMAT_NO_SUCH_LADDER);
    return COM_OK;
  }

  lp = ladder_player_named(lnum, name);

  if (!lp) {
    pcn_out(p, CODE_ERROR, FORMAT_NO_SUCH_LADDER_PLAYER);
    return COM_OK;
  }
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  ladder_remove_at(lnum, lp->idx);
  switch (lnum) {
  case 9: fp = xyfopen(FILENAME_LADDER9, "w"); break;
  default:
  case 19: fp = xyfopen(FILENAME_LADDER19, "w"); break;
  }
  if (!fp) {
    pcn_out(p, CODE_ERROR, FORMAT_THERE_WAS_AN_INTERNAL_ERROR_);
    return COM_OK;
  }
  switch (lnum) {
  case 9: num_9 = ladder_save(fp, lnum); break;
  default:
  case 19: num_19 = ladder_save(fp, lnum); break;
  }
  fclose(fp);
  player_resort();
  return COM_OK;
}

int com_createnews(int p, struct parameter* param)
{
  FILE *fp;
  char junk[MAX_LINE_SIZE];
  char *junkp;
  int crtime;
  char count[10];
  int len;

/* get number of next news entry */

  sprintf(count,"0");
  fp = xyfopen(FILENAME_NEWSINDEX, "r");
  if (!fp) {
    return COM_OK; 
  }
  while ((junkp=fgets(junk, sizeof junk, fp))) {
    if ((len = strlen(junk))<=1) continue;
    sscanf(junkp, "%d %s", &crtime, count);  
  }
  fclose(fp);
  sprintf(count, "%d", atoi(count) + 1);

/* create new entry in index file */

  fp = xyfopen(FILENAME_NEWSINDEX, "a");
  fprintf(fp, "%d %s %s\n", (int) globclock.time, count, param[0].val.string);
  fclose(fp);
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  return COM_OK;
}

int com_createadmnews(int p, struct parameter* param)
{
  FILE *fp;
  char junk[MAX_LINE_SIZE];
  char *junkp;
  int crtime;
  char count[10];
  int len;

/* get number of next news entry */

  sprintf(count,"0");
  fp = xyfopen(FILENAME_ADMINNEWSINDEX, "r");
  if (!fp) {
    return COM_OK; 
  }
  while ((junkp=fgets(junk, sizeof junk, fp))) {
    if ((len = strlen(junk))<=1) continue;
    sscanf(junkp, "%d %s", &crtime, count);  
  }
  fclose(fp);
  sprintf(count, "%d", atoi(count) + 1);

/* create new entry in index file */

  fp = xyfopen(FILENAME_ADMINNEWSINDEX, "a");
  fprintf(fp, "%d %s %s\n", (int) globclock.time, count, param[0].val.string);
  fclose(fp);
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  return COM_OK;
}

int com_anews(int p, struct parameter* param)
{
  FILE *fp;
  char junk[MAX_LINE_SIZE];
  char *junkp;
  time_t crtime;
  char count[10];
  int flag, len;

  if (!param[0].type || !strcmp(param[0].val.word,"all")) {

/* no params - then just display index over news */
   
    pprintf(p, "%s\n    **** ADMIN BULLETIN BOARD ****\n",
    		parray[p].flags.is_client ? "6 Info" : "");
    fp = xyfopen(FILENAME_ADMINNEWSINDEX, "r");
    if (!fp) {
      return COM_OK; 
    }
    flag=0;
    while ((junkp=fgets(junk, sizeof junk, fp))) {
      if ((len = strlen(junk))<=1) continue;
      junk[len-1]=0;
      sscanf(junkp, "%d %s", &crtime, count);
      junkp=nextword(junkp); 
      junkp=nextword(junkp);
      if (param[0].type==TYPE_WORD && !strcmp(param[0].val.word,"all")) {
        pprintf(p, "%3s (%s) %s\n", count, time2str_local(&crtime), junkp);
        flag=1;
      } else {
        if (crtime - player_lastconnect(p) > 0) {
          pprintf(p, "%3s (%s) %s\n", count, time2str_local(&crtime), junkp);
          flag=1;
	}
      }
    }
    fclose(fp);
    crtime = player_lastconnect(p);
    if (!flag) {
      pprintf(p, "There is no news since your last login (%s).\n",time2str_local(&crtime));
    } else {
      pprintf(p, "%s\n",
                parray[p].flags.is_client ? "6 Info" : "");
    }
  } else {

/* check if the specific news file exist in index */

    fp = xyfopen(FILENAME_ADMINNEWSINDEX, "r");
    if (!fp) {
      return COM_OK; 
    }
    flag=0;
    while ((junkp=fgets(junk, sizeof junk, fp))) {
      if ((len = strlen(junk))<=1) continue;
      junk[len-1]=0;
      sscanf(junkp, "%d %s", (int *) &crtime, count);
      if (!strcmp(count,param[0].val.word)) {
        junkp=nextword(junkp); 
        junkp=nextword(junkp);
        pprintf(p, "\nNEWS %3s (%s)\n\n         %s\n\n", count, time2str_local(&crtime), junkp);
        break;
      }
    }
    fclose(fp);
    if (!junkp) {
      pprintf(p, "Bad index number!\n");
      return COM_OK;
    }

/* file exists - show it */

    fp = xyfopen(FILENAME_ADMINNEWS_s, "r", param[0].val.word);
    if (!fp) {
      pprintf(p, "No more info.\n");
      return COM_OK; 
    }
    fclose(fp);   
    if (pxysend_file(p, FILENAME_ADMINNEWS_s, param[0].val.word) < 0) {
      pprintf(p, "Internal error - couldn't send news file!\n");
    }
  }
  pprintf(p, "%s\n", parray[p].flags.is_client ? "6 Info" : "");
  return COM_OK;
}


int strcmpwild(char *haystack, char *needle) {
  size_t i,mlen;

  if ((mlen=strlen(haystack)) < strlen(needle))
    return 1;
  for (i = 0; i < mlen; i++) {
    if (needle[i] == '*')
      return 0;
    if (haystack[i] != needle[i])
      return 1;
  }
  return 0;
}

int com_chk_ip(int p, struct parameter* param)
{
  char *ipstr = param[0].val.word;
  int p1;

  pcn_out(p, CODE_INFO, FORMAT_MATCHES_THE_FOLLOWING_PLAYER_S_n);
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_valid) continue;
    if (strcmpwild(dotQuad(parray[p1].thisHost), ipstr)) continue;
    pcn_out(p, CODE_INFO, FORMAT__s, parray[p1].pname);
    }
  return COM_OK;
}

int com_ausers(int p, struct parameter* param)
{
  int p1;
  UNUSED(param);

  pcn_out(p, CODE_INFO,FORMAT_NAME_FROM_LAST_TELL_CHAN_STATUSn);
  pcn_out(p, CODE_INFO,FORMAT_n);

  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_online) continue;
    pcn_out(p, CODE_INFO, FORMAT_s_s_s_d_s_dn,
                parray[p1].pname,
                dotQuad(parray[p1].thisHost),
                parray[p1].forget.last_tell >= 0 ? 
                         parray[parray[p1].forget.last_tell].pname : "----",
                parray[p1].last_channel,
                parray[p1].session.gnum < 0 ? 
                 (parray[p1].session.num_observe > 0 ? "O" : "-") : "P",
                parray[p1].session.gnum < 0 ?
                 (parray[p1].session.num_observe>0 ? parray[p1].session.observe_list[0] : 0) :
                 parray[p1].session.gnum + 1);
  }
  return COM_OK;
}

int com_asetdebug(int p, struct parameter* param)
{
  
  if (param[0].type == TYPE_NULL) {
    pcn_out(p, CODE_INFO, FORMAT_DEBUG_LEVEL_SET_TO_d, conffile.debug_general);
    return COM_OK;
  }
  
  else {
    conffile.debug_general = param[0].val.integer;
    if (conffile.debug_general < 0) conffile.debug_general = 0;
  }
  return COM_OK;
}

int com_chk_socket(int p, struct parameter* param)
{
  int fd = param[0].val.integer;
  int p1, cnt;

  cnt = 0;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].session.socket == fd) continue;
    cnt++;
    pcn_out(p, CODE_INFO, FORMAT_SOCKET_d_IS_USED_BY_s,
      fd, parray[p1].pname);
  }
  if (!cnt)
    pcn_out(p, CODE_INFO, FORMAT_SOCKET_d_IS_UNUSED_, fd);
  return COM_OK;
}


int com_chk_player(int p, struct parameter* param)
{
  char *pname = param[0].val.word;
  int p1;

  p1 = player_fetch(pname);
  if (p1< 0) return COM_OK;
  if (!parray[p1].slotstat.is_online) {
    pcn_out(p, CODE_INFO, FORMAT_s_IS_NOT_LOGGED_IN_n, pname);
    stolower(pname);
    pcn_out(p, CODE_INFO, FORMAT_NAME_sn, parray[p1].pname);
    pcn_out(p, CODE_INFO, FORMAT_LOGIN_sn, parray[p1].login);
    pcn_out(p, CODE_INFO, FORMAT_FULLNAME_sn, IFNULL0(parray[p1].fullname, "(none)") );
    pcn_out(p, CODE_INFO, FORMAT_EMAILADDRESS_sn, IFNULL0(parray[p1].email, "(none)") );
    pcn_out(p, CODE_INFO, FORMAT_ADMINLEVEL_dn, parray[p1].adminLevel);
    pcn_out(p, CODE_INFO, FORMAT_MUZZLED_dn, parray[p1].muzzled);
    pcn_out(p, CODE_INFO, FORMAT_GMUZZLED_dn, parray[p1].gmuzzled);
    pcn_out(p, CODE_INFO, FORMAT_BMUZZLED_dn, parray[p1].bmuzzled);
    pcn_out(p, CODE_INFO, FORMAT_LASTHOST_s, dotQuad(parray[p1].lastHost));
    player_unfix(p1);
    return COM_OK;
  } 
  pcn_out(p, CODE_INFO, FORMAT_s_IS_NUMBER_d_IN_PARRAY_OF_SIZE_dn,
                pname, p1, parray_top);
  pcn_out(p, CODE_INFO, FORMAT_NAME_sn, parray[p1].pname);
  pcn_out(p, CODE_INFO, FORMAT_LOGIN_sn, parray[p1].login);
  pcn_out(p, CODE_INFO, FORMAT_FULLNAME_sn, IFNULL0(parray[p1].fullname, "(none)") );
  pcn_out(p, CODE_INFO, FORMAT_EMAILADDRESS_sn, IFNULL0(parray[p1].email, "(none)") );
  pcn_out(p, CODE_INFO, FORMAT_SOCKET_dn, parray[p1].session.socket);
  pcn_out(p, CODE_INFO, FORMAT_REGISTERED_dn, (int) parray[p1].slotstat.is_registered);
  pcn_out(p, CODE_INFO, FORMAT_LAST_TELL_dn, parray[p1].forget.last_tell);
  pcn_out(p, CODE_INFO, FORMAT_LAST_CHANNEL_dn, parray[p1].last_channel);
  pcn_out(p, CODE_INFO, FORMAT_LOGON_TIME_dn, parray[p1].session.logon_time);
  pcn_out(p, CODE_INFO, FORMAT_ADMINLEVEL_dn, parray[p1].adminLevel);
  pcn_out(p, CODE_INFO, FORMAT_STATE_dn, parray[p1].session.protostate);
  pcn_out(p, CODE_INFO, FORMAT_MUZZLED_dn, parray[p1].muzzled);
  pcn_out(p, CODE_INFO, FORMAT_GMUZZLED_dn, parray[p1].gmuzzled);
  pcn_out(p, CODE_INFO, FORMAT_BMUZZLED_dn, parray[p1].bmuzzled);
  pcn_out(p, CODE_INFO, FORMAT_THISHOST_sn, dotQuad(parray[p1].thisHost));
  pcn_out(p, CODE_INFO, FORMAT_LASTHOST_s, dotQuad(parray[p1].lastHost));
  player_unfix(p1);
  return COM_OK;
}

int com_remplayer(int p, struct parameter* param)
{
  char *pname = param[0].val.word;
  char playerlower[MAX_LOGIN_NAME+1];
  int p1;

  strcpy(playerlower, pname);
  stolower(playerlower);
  p1 = player_fetch(playerlower);
  if (p1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_NO_PLAYER_BY_THE_NAME_s_IS_REGISTERED_, pname);
    return COM_OK;
  }
  if (parray[p].adminLevel <= parray[p1].adminLevel) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_REMOVE_AN_ADMIN_WITH_A_LEVEL_HIGHER_THAN_OR_EQUAL_TO_YOURSELF_);
    player_unfix(p1);
    return COM_OK;
  }
  if (parray[p1].slotstat.is_registered && parray[p1].slotstat.is_online) {
    pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THAT_NAME_IS_LOGGED_IN_n);
    player_unfix(p1);
    return COM_OK;
  }
  if (player_kill(playerlower)) {
    pcn_out(p, CODE_ERROR, FORMAT_REMPLAYER_FAILED_ );
  } else {
    pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_REMOVED_, pname);
  }
  Show_Admin_Command(p, param[0].val.word, " ");
  player_unfix(p1);
  return COM_OK;
}

int com_raisedead(int p, struct parameter* param)
{
  char *oldplayer = param[0].val.word;
  char *newplayer ;
  char oldlower[MAX_LOGIN_NAME+1], newlower[MAX_LOGIN_NAME+1];
  
  int p1,p2;

  if (param[1].val.word) newplayer = param[1].val.word;
  else newplayer = oldplayer;
  strcpy(oldlower, oldplayer);
  stolower(oldlower);
  strcpy(newlower, newplayer);
  stolower(newlower);

  p1 = player_fetch(oldlower);
  if (p1 >=0) {
    if (parray[p1].slotstat.is_registered) {
      pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THE_NAME_s_IS_ALREADY_REGISTERED_n, oldplayer);
      pcn_out(p, CODE_ERROR, FORMAT_OBTAIN_A_NEW_HANDLE_FOR_THE_DEAD_PERSON_n);
      pcn_out(p, CODE_ERROR, FORMAT_THEN_USE_RAISEDEAD_OLDNAME_NEWNAME_);
      player_unfix(p1);
      return COM_OK;
    }
    if (parray[p1].slotstat.is_online) {
      pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THAT_NAME_IS_LOGGED_IN_n);
      pcn_out(p, CODE_ERROR, FORMAT_CAN_T_RAISE_UNTIL_THEY_LEAVE_);
      player_unfix(p1);
      return COM_OK;
    }
  }
  if (newplayer==oldplayer) {
    if (player_raise(oldlower)) {
      pcn_out(p, CODE_ERROR, FORMAT_RAISEDEAD_FAILED_);
    } else {
      pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_RAISED_FROM_DEAD_, oldplayer);
    }
    player_unfix(p1);
    return COM_OK;
  } else {
    p2 = player_fetch(newlower);
    if (p2>=0 && parray[p2].slotstat.is_registered) {
      pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THE_NAME_s_IS_ALREADY_REGISTERED_, newplayer);
      pcn_out(p, CODE_ERROR, FORMAT_OBTAIN_ANOTHER_NEW_HANDLE_FOR_THE_DEAD_PERSON_);
      player_unfix(p1);
      player_unfix(p2);
      return COM_OK;
    }
  }
  if (!player_rename(oldlower, newlower)) {
    pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_REINCARNATED_TO_s_, oldplayer, newplayer);
    do_copy(parray[p2].pname, newplayer, sizeof parray[0].pname);
    player_save(p2);
  } else {
    pcn_out(p, CODE_ERROR, FORMAT_RAISEDEAD_FAILED_);
  }
  player_unfix(p1);
  player_unfix(p2);
  return COM_OK;
}

static int shutdownTime = 0;
static int lastTimeLeft;
static int shutdownStartTime;
static char downer[sizeof parray[0].pname];
void ShutDown()
{
  int p1;
  time_t shuttime = globclock.time;

  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_connected) continue;
    pcn_out(p1, CODE_DOWN, FORMAT_SERVER_SHUTDOWN_STARTED_BY_s_n, downer);
  }
  TerminateCleanup();
#if 0
  fprintf(stderr, "Shutdown ordered at %s by %s.\n", time2str_local(&shuttime), downer);
#endif
  Logit("Shutdown ordered at %s (Local) by %s.", time2str_local(&shuttime), downer);
  net_closeAll();
  system("touch .shutdown");
  main_exit(0);
}

void ShutHeartBeat()
{
  time_t now = globclock.time;
  int p1;
  int timeLeft;
  int crossing = 0;

  if (!shutdownTime) return;
  if (!lastTimeLeft) lastTimeLeft = shutdownTime;
  timeLeft = shutdownTime - (now - shutdownStartTime);
  if (lastTimeLeft > 3600 && timeLeft <= 3600) crossing = 1;
  if (lastTimeLeft > 2400 && timeLeft <= 2400) crossing = 1;
  if (lastTimeLeft > 1200 && timeLeft <= 1200) crossing = 1;
  if (lastTimeLeft > 600 && timeLeft <= 600) crossing = 1;
  if (lastTimeLeft > 300 && timeLeft <= 300) crossing = 1;
  if (lastTimeLeft > 120 && timeLeft <= 120) crossing = 1;
  if (lastTimeLeft > 60 && timeLeft <= 60) crossing = 1;
  if (lastTimeLeft > 10 && timeLeft <= 10) crossing = 1;
  if (crossing) {
    Logit("   **** Server going down in %d minutes and %d seconds. ****",
	    timeLeft / 60,
	    timeLeft - ((timeLeft / 60) * 60));
    for (p1 = 0; p1 < parray_top; p1++) {
      if (!parray[p1].slotstat.is_connected) continue;
      pcn_out_prompt(p1, CODE_DOWN,
		     FORMAT_SERVER_GOING_DOWN_IN_d_MINUTES_AND_d_SECONDS_n,
		     timeLeft / 60,
		     timeLeft - ((timeLeft / 60) * 60));
    }
  }
  lastTimeLeft = timeLeft;
  if (timeLeft <= 0) {
    ShutDown();
  }
}

int com_shutdown(int p, struct parameter* param)
{
  char *ptr;
  int p1, secs;

  do_copy(downer, parray[p].pname, sizeof downer);
  shutdownStartTime = globclock.time;   
  if (shutdownTime) {         /* Cancel any pending shutdowns */
    for (p1 = 0; p1 < parray_top; p1++) {
      if (!parray[p1].slotstat.is_connected) continue;
      pcn_out(p1, CODE_INFO, FORMAT_SERVER_SHUTDOWN_CANCELED_BY_s_n, downer);
    }
    shutdownTime = 0;
    if (param[0].type == TYPE_NULL)
      return COM_OK;
  }
  /* Work out how soon to shut down */
  if (param[0].type == TYPE_NULL)
    shutdownTime = 300;
  else {
    if (!strcmp(param[0].val.word, "now"))
      shutdownTime = 0;
    else if (!strcmp(param[0].val.word, "cancel"))
      return COM_OK;
    else {
      ptr = param[0].val.word;
      shutdownTime = secs = 0;
      p1 = 2;
      while (*ptr) {
      switch(*ptr) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          secs = secs*10 + *ptr-'0';
          break;
        case ':':
          if (p1--) {
            shutdownTime = shutdownTime*60 + secs;
            secs = 0;
            break;   
          }
        default:
          shutdownTime = 0;
          pcn_out(p, CODE_ERROR, FORMAT_I_DON_T_KNOW_WHAT_YOU_MEAN_BY_sn, param[0].val.word);
          return COM_OK;
      }
      ptr++;
      }
      shutdownTime = shutdownTime*60 + secs;
    }
  }
  if (shutdownTime <= 0)
    ShutDown();
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_connected) continue;
    pcn_out(p1, CODE_DOWN, FORMAT_SERVER_SHUTDOWN_STARTED_BY_s_n, downer);
    pcn_out_prompt(p1,CODE_DOWN,
     FORMAT_SERVER_GOING_DOWN_IN_d_MINUTES_AND_d_SECONDS_n,
                shutdownTime / 60, shutdownTime % 60);
  }
  lastTimeLeft = 0;
  return COM_OK;
}


int server_shutdown(int secs, char *why)
{
  int p1;

  if (shutdownTime && (shutdownTime <= secs)) {
    /* Server is already shutting down, I'll let it go */
    return 0;
  }
  do_copy(downer, "Automatic", sizeof downer);
  shutdownTime = secs;
  shutdownStartTime = globclock.time;
  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_connected) continue;
    pcn_out(p1, CODE_DOWN, FORMAT_AUTOMATIC_SERVER_SHUTDOWN_n);
    pcn_out(p1, CODE_DOWN, FORMAT_sn, why);
    pcn_out_prompt(p1,CODE_DOWN,
	FORMAT_SERVER_GOING_DOWN_IN_d_MINUTES_AND_d_SECONDS_n,
		   shutdownTime / 60,
		   shutdownTime - ((shutdownTime / 60) * 60));
  }
  Logit("    **** Automatic Server shutdown. ****");
  Logit(" %s", why);
  return 0;
}

int com_pose(int p, struct parameter* param)
{
  int p1;

  stolower(param[0].val.word);
  p1 = player_find_part_login(param[0].val.word);
  if (p1 < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_s_IS_NOT_LOGGED_IN_, param[0].val.word);
    return COM_OK;
  }
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
#if NONONOPE
  Logit("POSE: %s as %s: > %s <", parray[p].pname, parray[p1].pname, param[1].val.string); 
  for(j = 0; j < carray[CHANNEL_ASHOUT].count; j++) {
    p1= carray[CHANNEL_ASHOUT].members[j];
    if (!parray[p1].slotstat.is_online) continue;
    if (p1 == p) continue;
    pprintf(p1, "\n");
    pcn_out_prompt(p1, CODE_INFO, FORMAT_POSE_s_AS_s_s_n,
      parray[p].pname, parray[p1].pname,
      param[1].val.string);
  }
#endif
  pcommand(p1, param[1].val.string);
  return COM_OK;
}


int Show_Admin_Command(int p, const char *comm, const char *command)
{
  int j,p1;
  UNUSED(comm);
  UNUSED(command);

  Logit("ADMIN: %s > %s <", parray[p].pname, orig_command);
  for(j = 0; j < carray[CHANNEL_ASHOUT].count; j++) {
    p1 = carray[CHANNEL_ASHOUT].members[j];
    if (!parray[p1].slotstat.is_online) continue;
    pprintf(p1, "\n");
    pcn_out_prompt(p1, CODE_INFO, FORMAT_ADMIN_s_s_n,
      parray[p].pname, orig_command);
  }
  return 1;
}


int com_arank(int p, struct parameter* param)
{
  int p1;

  if (strlen(param[1].val.string) > 4) return COM_OK;
  p1 = player_fetch(param[0].val.word);
  if (p1 <0) return COM_OK;
  do_copy(parray[p1].ranked, param[1].val.string, sizeof parray[0].ranked);
  do_copy(parray[p1].srank, param[1].val.string, sizeof parray[0].srank);
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  player_dirty(p1);
  player_unfix(p1);
  return COM_OK;
}

int com_noshout(int p, struct parameter* param)
{
  int j,p1;
  UNUSED(param);

  if (carray[CHANNEL_SHOUT].is_locked) carray[CHANNEL_SHOUT].is_locked = 0;
  else  carray[CHANNEL_SHOUT].is_locked = 1;
  for(j = 0; j < carray[CHANNEL_ASHOUT].count; j++) {
    p1=carray[CHANNEL_ASHOUT].members[j];
    if (!parray[p1].slotstat.is_online) continue;
    if (p1 == p) continue;
    pprintf(p1, "\n");
    pcn_out_prompt(p1, CODE_INFO, FORMAT_s_JUST_TURNED_s_SHOUTS_n, 
      parray[p].pname,
      carray[CHANNEL_SHOUT].is_locked ? "off" : "on");
  }
  return COM_OK;
}
    

int com_announce(int p, struct parameter* param)
{
  int p1;
  UNUSED(param);

  for (p1 = 0; p1 < parray_top; p1++) {
    if (!parray[p1].slotstat.is_online) continue;
    pcn_out_prompt(p1, CODE_INFO, FORMAT_s_arrow_sn, 
                parray[p].pname, param[0].val.string);
  }
  return COM_OK;
}


int com_muzzle(int p, struct parameter* param)
{
  int p1;
  
  if (param[0].type == TYPE_NULL) {
    pcn_out(p, CODE_INFO, FORMAT_MUZZLED_PLAYERS_n);
    for (p1 = 0; p1 < parray_top; p1++) {
      if (!parray[p1].slotstat.is_online) continue;
      if (parray[p1].muzzled)
	pcn_out(p, CODE_INFO, FORMATkibitz_sn, parray[p1].pname);
    }
    return COM_OK;
  }
  p1 = player_fetch(param[0].val.word);
  if (p1 <0) return COM_OK;
  if (parray[p1].muzzled) {
    pcn_out(p, CODE_INFO, FORMAT_s_UNMUZZLED_, parray[p1].pname);
    parray[p1].muzzled = 0;
  } else {
    pcn_out(p, CODE_INFO, FORMAT_s_MUZZLED_, parray[p1].pname);
    parray[p1].muzzled = 1;
  }
  player_dirty(p1);
  player_unfix(p1);
  return COM_OK;
}


int com_bmuzzle(int p, struct parameter* param)
{
  int p1;
  
  if (param[0].type == TYPE_NULL) {
    pcn_out(p, CODE_INFO, FORMAT_BMUZZLED_PLAYERS_n);
    for (p1 = 0; p1 < parray_top; p1++) {
      if (!parray[p1].slotstat.is_online) continue;
      if (parray[p1].bmuzzled)
        pcn_out(p, CODE_INFO, FORMATkibitz_sn, parray[p1].pname);
    }
    return COM_OK;
  }
  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
  if (parray[p1].bmuzzled) {
    pcn_out(p, CODE_INFO, FORMAT_s_UNBMUZZLED_, parray[p1].pname);
    parray[p1].bmuzzled = 0;
  } else {
    pcn_out(p, CODE_INFO, FORMAT_s_BMUZZLED_, parray[p1].pname);
    parray[p1].bmuzzled = 1;
  }
  player_dirty(p1);
  player_unfix(p1);
  return COM_OK;
}


int com_gmuzzle(int p, struct parameter* param)
{
  int p1;
  
  if (param[0].type == TYPE_NULL) {
    pcn_out(p, CODE_INFO, FORMAT_GMUZZLED_PLAYERS_n);
    for (p1 = 0; p1 < parray_top; p1++) {
      if (!parray[p1].slotstat.is_online) continue;
      if (parray[p1].gmuzzled)
	pcn_out(p, CODE_INFO, FORMATkibitz_sn, parray[p1].pname);
    }
    return COM_OK;
  }
  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
  if (parray[p1].gmuzzled) {
    pcn_out(p, CODE_INFO, FORMAT_s_UNGMUZZLED_, parray[p1].pname);
    parray[p1].gmuzzled = 0;
  } else {
    pcn_out(p, CODE_INFO, FORMAT_s_GMUZZLED_, parray[p1].pname);
    parray[p1].gmuzzled = 1;
  }
  player_dirty(p1);
  player_unfix(p1);
  return COM_OK;
}


int com_asetpasswd(int p, struct parameter* param)
{
  int p1;
  char salt[3];

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
  if (parray[p].adminLevel <= parray[p1].adminLevel) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_ONLY_SET_PASSWORD_FOR_PLAYERS_BELOW_YOUR_ADMINLEVEL_);
    player_unfix(p1);
    return COM_OK;
  }
  if (param[1].val.word[0] == '*') {
    do_copy(parray[p1].passwd, param[1].val.word, sizeof parray[0].passwd);
    pcn_out(p, CODE_INFO, FORMAT_ACCOUNT_s_LOCKED_n, parray[p1].pname);
  } else {
    salt[0] = 'a' + rand() % 26;
    salt[1] = 'a' + rand() % 26;
    salt[2] = '\0';
    
    do_copy(parray[p1].passwd, mycrypt(param[1].val.word, salt), sizeof parray[0].passwd);
    pcn_out(p, CODE_INFO, FORMAT_PASSWORD_OF_s_CHANGED_TO_qsq_n, parray[p1].pname, param[1].val.word);
  }
  player_dirty(p1);
  if (parray[p1].slotstat.is_online) {
    if (param[1].val.word[0] == '*') {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_LOCKED_YOUR_ACCOUNT_n, parray[p].pname);
    } else {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_CHANGED_YOUR_PASSWORD_n, parray[p].pname);
    }
  }
  player_unfix(p1);
  return COM_OK;
}


int com_asetemail(int p, struct parameter* param)
{
  int p1;

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
  if (param[1].type == TYPE_NULL) {
    parray[p1].email[0] = '\0';
    pcn_out(p, CODE_INFO, FORMAT_EMAIL_ADDRESS_FOR_s_REMOVEDn, parray[p1].pname);
  } else {
    do_copy(parray[p1].email, param[1].val.word, sizeof parray[0].email);
    pcn_out(p, CODE_INFO, FORMAT_EMAIL_ADDRESS_OF_s_CHANGED_TO_qsq_, parray[p1].pname, param[1].val.word);
  }
  player_dirty(p1);
  if (parray[p1].slotstat.is_online) {
    if (param[1].type == TYPE_NULL) {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_REMOVED_YOUR_EMAIL_ADDRESS_n, parray[p].pname);
    } else {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_CHANGED_YOUR_EMAIL_ADDRESS_n, parray[p].pname);
    }
  }
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  player_unfix(p1);
  return COM_OK;
}
 

int com_asetrealname(int p, struct parameter* param)
{
  int p1;

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
  if (param[1].type == TYPE_NULL) {
    do_copy(parray[p1].fullname, "-", sizeof parray[0].fullname);
    pcn_out(p, CODE_INFO, FORMAT_REAL_NAME_FOR_s_REMOVED,parray[p1].pname);
  } else {
    do_copy(parray[p1].fullname, param[1].val.word, sizeof parray[0].fullname);
    pcn_out(p, CODE_INFO, FORMAT_REAL_NAME_OF_s_CHANGED_TO_qsq_, parray[p1].pname, param[1].val.word);
  }
  player_dirty(p1);
  if (parray[p1].slotstat.is_online) {
    if (param[1].type == TYPE_NULL) {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_REMOVED_YOUR_REAL_NAME_n, parray[p].pname);
    } else {
      pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_CHANGED_YOUR_REAL_NAME_n, parray[p].pname);
    }
  }
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  player_unfix(p1);
  return COM_OK;
}


int com_asetsilent(int p, struct parameter* param)
{
  int p1;

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;

  parray[p1].silent_login = !parray[p1].silent_login;

  player_dirty(p1);

  pcn_out(p, CODE_INFO, FORMAT_CHANGED_s_S_LOGIN_TO_s_n, parray[p1].pname, parray[p1].silent_login ? "SILENT" : "NOT SILENT");


  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  player_unfix(p1);
  return COM_OK;
}


int com_asethandle(int p, struct parameter* param)
{
  char *oldname = param[0].val.word;
  char *newname = param[1].val.word;
  char oldlower[MAX_LOGIN_NAME+1], newlower[MAX_LOGIN_NAME+1];
  int p1, p2;

  strcpy(oldlower, oldname);
  stolower(oldlower);
  strcpy(newlower, newname);
  stolower(newlower);
  p1 = player_fetch(oldlower);
  if (p1 < 0 || !parray[p1].slotstat.is_registered) {
    pcn_out(p, CODE_ERROR, FORMAT_NO_PLAYER_BY_THE_NAME_s_IS_REGISTERED_, oldname);
    player_unfix(p1);
    return COM_OK;
  }
  if (p1 >= 0 && parray[p].adminLevel <= parray[p1].adminLevel) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_SET_HANDLES_FOR_AN_ADMIN_WITH_A_LEVEL_HIGHER_THAN_OR_EQUAL_TO_YOURSELF_);
    player_unfix(p1);
    return COM_OK;
  }
  if (p1 >= 0 && parray[p1].slotstat.is_online) {
    pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THAT_NAME_IS_LOGGED_IN_);
    player_unfix(p1);
    return COM_OK;
  }
  p2 = player_fetch(newlower);
  if (p2 >= 0 && parray[p2].slotstat.is_online) {
    pcn_out(p, CODE_ERROR, FORMAT_A_PLAYER_BY_THAT_NEW_NAME_IS_LOGGED_IN_);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }
  if (p2 >= 0 && parray[p2].slotstat.is_registered) {
    pcn_out(p, CODE_ERROR, FORMAT_SORRY_THAT_HANDLE_IS_ALREADY_TAKEN_);
    player_unfix(p1);
    player_unfix(p2);
    return COM_OK;
  }
  if (p2 < 0) p2 = player_new();
  do_copy(parray[p2].login, newlower, sizeof parray[p2].login);
  if (player_rename(oldlower, newlower) && !player_read(p2)) {
    pcn_out(p, CODE_ERROR, FORMAT_ASETHANDLE_FAILED_);
    player_unfix(p1);
    player_clear(p2);
    return COM_OK;
  }

  pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_RENAMED_TO_s_, oldname, newname);
  do_copy(parray[p2].pname, newname, sizeof parray[p2].pname);
  player_dirty(p2);
  player_unfix(p2);
  player_clear(p1);
  return COM_OK;
}


int com_asetadmin(int p, struct parameter* param)
{
  int p1, oldlevel;

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;
 
  if (parray[p].adminLevel <= parray[p1].adminLevel) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_ONLY_SET_ADMINLEVEL_FOR_PLAYERS_BELOW_YOUR_ADMINLEVEL_);
    player_unfix(p1);
    return COM_OK;
  }
  if (p1==p || !strcmp(parray[p1].login , parray[p].login) ) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_CHANGE_YOUR_OWN_ADMINLEVEL_);
    player_unfix(p1);
    return COM_OK;
  }
  if (param[1].val.integer >= parray[p].adminLevel) {
    pcn_out(p, CODE_ERROR, FORMAT_YOU_CAN_T_PROMOTE_SOMEONE_TO_OR_ABOVE_YOUR_ADMINLEVEL_);
    player_unfix(p1);
    return COM_OK;
  }

  oldlevel=parray[p1].adminLevel;
  parray[p1].adminLevel = param[1].val.integer;
  player_dirty(p1);
  pcn_out(p, CODE_INFO, FORMAT_ADMIN_LEVEL_OF_s_SET_TO_d_n, 
            parray[p1].pname, parray[p1].adminLevel);

  if (oldlevel < ADMIN_ADMIN && parray[p1].adminLevel >= ADMIN_ADMIN) {
    pcommand (p, "+admin %s\n",parray[p1].pname);
  }

  if (oldlevel >= ADMIN_ADMIN && parray[p1].adminLevel < ADMIN_ADMIN) {
    pcommand (p, "-admin %s\n",parray[p1].pname);
  }

  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  player_unfix(p1);
  return COM_OK;
}

/* This pseudo command is used to "bootstrap" the player database
 */
void create_admin_account(char *name)
{
  int p1, oldlevel;

  if (!name || !*name) return;
  p1 = player_fetch(name);
  if (p1 >= 0) {
    oldlevel = parray[p1].adminLevel;
    parray[p1].adminLevel = 100;
    Logit("Boot: altered %s adminlevel from %d to %d", parray[p1].pname, oldlevel, 100);
    parray[p1].slotstat.is_registered = 1;
    parray[p1].slotstat.is_valid = 1;
    player_dirty(p1);
    player_unfix(p1);
    return ;
    }
  p1 = player_new();
  if (p1 >= 0) {
    oldlevel = parray[p1].adminLevel;
    parray[p1].adminLevel = 100;
    do_copy(parray[p1].pname, name, sizeof parray[p1].pname);
    do_copy(parray[p1].login, name, sizeof parray[p1].login);
    do_copy(parray[p1].fullname, name, sizeof parray[p1].fullname);
    do_copy(parray[p1].email, "", sizeof parray[p1].email);
    do_copy(parray[p1].rank, "NR", sizeof parray[p1].email);
    do_copy(parray[p1].ranked, " ", sizeof parray[p1].ranked);
    do_copy(parray[p1].srank, "NR", sizeof parray[p1].srank);
    Logit("Boot: created %s with adminlevel from %d to %d", parray[p1].pname, oldlevel, 100);
    parray[p1].slotstat.is_registered = 1;
    parray[p1].slotstat.is_valid = 1;
    player_dirty(p1);
    player_unfix(p1);
    }
 
  return ;
}


int com_asetwater(int p, struct parameter* param)
{
  int p1;

  p1 = player_fetch(param[0].val.word);
  if (p1 < 0) return COM_OK;

  parray[p1].water = param[1].val.integer;
  player_dirty(p1);
  pcn_out(p, CODE_INFO, FORMAT_WATER_LEVEL_OF_s_SET_TO_d_,
            parray[p1].pname, parray[p1].water);
  if (parray[p1].slotstat.is_online) {
    pcn_out_prompt(p1, CODE_INFO, FORMAT_s_HAS_SET_YOUR_WATER_LEVEL_TO_d_n,
              parray[p].pname, parray[p1].water);
  }
  player_unfix(p1);
  return COM_OK;
}

int com_nuke(int p, struct parameter* param)
{
  int p1, pk, j;

  stolower(param[0].val.word);
  pk = player_find_part_login(param[0].val.word);
  if (pk < 0) {
    pcn_out(p, CODE_ERROR, FORMAT_s_ISN_T_LOGGED_IN_, param[0].val.word);
    return COM_OK;
  } 
  pcn_out(p, CODE_INFO, FORMAT_NUKING_s, param[0].val.word);
  Logit("NUKE: %s by %s", parray[pk].pname, parray[p].pname);
  for(j = 0; j < carray[CHANNEL_ASHOUT].count; j++) {
    p1=carray[CHANNEL_ASHOUT].members[j];
    if (!parray[p1].slotstat.is_online) continue;
    if (p1 == p) continue;
    pprintf(p1, "\n");
    pcn_out_prompt(p1, CODE_INFO, FORMAT_s_JUST_NUKED_s_n,
      parray[p].pname,
      parray[pk].pname);
  }
  process_disconnection(parray[pk].session.socket);
  return COM_OK;
}


int com_actitle(int p, struct parameter* param)
{
  int i;

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  if (strlen(param[1].val.string) > 1000) return COM_OK;

  free(carray[i].ctitle);
  carray[i].ctitle = mystrdup(param[1].val.string);
  Show_Admin_Command(p, param[0].val.word, param[1].val.string);
  return COM_OK;
}


int com_reload_ladders(int p, struct parameter* param)
{
  FILE *fp;
  UNUSED(p);
  UNUSED(param);
  
  ladder_delete(Ladder9);
  ladder_delete(Ladder19);
  Ladder9 = ladder_new(LADDERSIZE);
  Ladder19 = ladder_new(LADDERSIZE);
  num_9 = 0;
  fp = xyfopen(FILENAME_LADDER9, "r");
  if (fp) {
    num_9 = ladder_load(fp, Ladder9);
    Logit("%d players loaded from file %s", num_9, filename());
    fclose(fp);
  }
  num_19 = 0;
  fp = xyfopen(FILENAME_LADDER19, "r");
  if (fp) {
    num_19 = ladder_load(fp, Ladder19);
    Logit("%d players loaded from file %s", num_19, filename());
    fclose(fp);
  }
  return COM_OK;
}


int com_hide(int p, struct parameter* param)
{
  int i;
  UNUSED(p);

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  carray[i].is_hidden = 1;
  return COM_OK;
}


int com_unhide(int p, struct parameter* param)
{
  int i;
  UNUSED(p);

  if (param[0].val.integer >= MAX_NCHANNEL || param[0].val.integer < 0) {
    return COM_OK;
  }

  i = param[0].val.integer;

  carray[i].is_hidden = 0;
  return COM_OK;
}


#if WANT_LADDER_SIFT
int com_rating_recalc(int p, struct parameter* param)
{
  UNUSED(p);
  UNUSED(param);

  Logit("Sifting 19x19 ladder");
  ladder_sift(Ladder19, 30);  /* Do the ladder stuff, 19x19 is 14 days */
  Logit("Sifting 9x9 ladder");
  ladder_sift(Ladder9, 30); /* Do the ladder stuff, 9x9 is 7 days */
  Logit("Done sifting");
  return COM_OK;
}
#endif /*WANT_LADDER_SIFT*/

int com_aban(int p, struct parameter* param)
{
  int cnt=0,rc=0, p1;
  const char * cmd;
  const char *name= NULL;
  unsigned bot,top;

  cmd = (param[0].type==TYPE_WORD) ? param[0].val.word : "." ;

  if (strspn(cmd,"+-?") 
      && (param[1].type!=TYPE_WORD || asc2ipaddr(param[1].val.word, &bot))
     ) {
    pcn_out(p, CODE_ERROR, FORMAT_NEED_IP_ADDRESSn);
    return COM_OK;
  }
  else if (strspn(cmd,"*/") 
      && (param[1].type!=TYPE_WORD || !(name=param[1].val.word) )
     ) {
    pcn_out(p, CODE_ERROR, FORMAT_NEED_NAMEn);
    return COM_OK;
  }
  if (param[2].type!=TYPE_WORD) {
    top = bot;
  }
  else if (strspn(cmd,"+-?") && asc2ipaddr(param[2].val.word, &top) ) {
    pcn_out(p, CODE_ERROR, FORMAT_NEED_IP_ADDRESSn);
    return COM_OK;
  }

  switch (*cmd) {
  case '+':
    cnt = rc = range_add(bot, top);
    break;
    pcn_out(p, CODE_INFO, FORMAT_IP_BAN_CHANGED_dn, cnt);
  case '-':
    cnt = rc = range_remove(bot, top);
    pcn_out(p, CODE_INFO, FORMAT_IP_BAN_CHANGED_dn, cnt);
    break;
  case '*': /* Same as +, but also reports currently logged in users ... */
  case '/': /* Same as -, idem */
    for (p1 = 0; p1 < parray_top; p1++) {
      const char *banned,*connected;
      if (!parray[p1].slotstat.is_valid) continue;
      if (strcmp(parray[p1].pname, name)) continue;
      rc = range_check(parray[p1].thisHost,parray[p1].thisHost);
      banned = (rc) ? "in" : "un";
      connected = (parray[p1].slotstat.is_connected) ? "is" : "not";
      pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_AT_s_sBANNED_RANGE_sONLINEn
        , parray[p1].pname, dotQuad(parray[p1].thisHost), banned, connected);
      rc = (*cmd == '*')
        ? range_add(parray[p1].thisHost,parray[p1].thisHost)
        : range_remove(parray[p1].thisHost,parray[p1].thisHost);
      if (!rc) continue;
      cnt += rc;
    }
    rc = 0;
    break;
  case '?':	/* Report current users within range. */
    rc = range_check(bot,top);
    pcn_out(p, CODE_INFO, FORMAT_IP_ADDRESS_s_s_BAN_IS_dn
      , dotQuad(bot), dotQuad(top), rc);
    for (p1 = 0; p1 < parray_top; p1++) {
      const char *banned,*connected;
      if (!parray[p1].slotstat.is_valid) continue;
      if (parray[p1].thisHost < bot) continue;
      if (parray[p1].thisHost > top) continue;
      rc = range_check(parray[p1].thisHost,parray[p1].thisHost);
      banned = (rc) ? "in" : "un";
      connected = (parray[p1].slotstat.is_connected) ? "is" : "not";
      pcn_out(p, CODE_INFO, FORMAT_PLAYER_s_AT_s_sBANNED_RANGE_sONLINEn
        , parray[p1].pname, dotQuad(parray[p1].thisHost), banned, connected);
      cnt++;
    }
    cnt = rc = 0;
    break;
  case '.':	/* print currently banned ranges */
    pcn_out(p, CODE_INFO, FORMAT_IP_RANGES_n);
    bot = 1; top = 0;
    for(cnt=rc=0 ;range_iter(&bot, &top); ) {
      cpprintf(p, CODE_INFO, "%s - %s\n", dotQuad(bot), dotQuad(top));
    }
    break;
  case '!':	/* Check current users; kick out banned ones */
    for(cnt=p1=0 ;p1 < COUNTOF(parray); p1++) {
      if (!parray[p1].slotstat.is_connected) continue;
      bot = parray[p1].thisHost;
      rc = range_check(bot,bot);
      if (!rc) continue;
      cpprintf(p, CODE_INFO, "Disconnecting %s fd=%d %s\n"
        , parray[p1].login,  parray[p1].session.socket, dotQuad(bot));
      Logit("Aban ! %s fd=%d %s"
        , parray[p1].login, rc, dotQuad(bot));
      if (p1 == p) continue;
      process_disconnection(parray[p1].session.socket);
      cnt++;
    }
    rc = 0;
    break;
  default:
    pcn_out(p, CODE_ERROR, FORMAT_NEED_ADCP_IP_ADDRESSn);
    return COM_OK;
    break;
  }
  if (rc) Logit("Banip %s %s %s (%d)", cmd, dotQuad(bot), dotQuad(top), cnt);
  if (cnt) {
    FILE *fp;
    fp = xyfopen(FILENAME_LIST_BAN, "w" );
    if (!fp) return COM_OK;
    range_dump(fp);
    fclose (fp);
  }
 
  return COM_OK;
}

