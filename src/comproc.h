/* comproc.h
 *
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1997  Erik Van Riper (geek@midway.com)
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

#ifndef COMPROC_H
#define COMPROC_H
#include <stdio.h> /* for FILE */

int com_rating_recalc(int, struct parameter *);
int com_more(int, struct parameter *);

int com_quit(int, struct parameter *);
int com_register(int, struct parameter *);
int com_ayt(int, struct parameter *);
int com_help(int, struct parameter *);
int com_info(int, struct parameter *);
int com_rank(int, struct parameter *);
int com_ranked(int, struct parameter *);
int com_infor(int, struct parameter *);
int com_adhelp(int, struct parameter *);
int com_shout(int, struct parameter *);
int com_ashout(int, struct parameter *);
int com_cshout(int, struct parameter *);
int com_gshout(int, struct parameter *);
int com_query(int, struct parameter *);
int com_it(int, struct parameter *);
int com_git(int, struct parameter *);
int com_tell(int, struct parameter *);
int com_say(int, struct parameter *);
int com_choice(int, struct parameter *);
int com_whisper(int, struct parameter *);
int com_kibitz(int, struct parameter *);
int com_set(int, struct parameter *);
int com_stats(int, struct parameter *);
int com_cstats(int, struct parameter *);
int com_password(int, struct parameter *);
int com_uptime(int, struct parameter *);
int com_date(int, struct parameter *);
int com_llogons(int, struct parameter *);
int com_logons(int, struct parameter *);
int com_who(int, struct parameter *);
int com_censor(int, struct parameter *);
int com_uncensor(int, struct parameter *);
int com_notify(int, struct parameter *);
int com_unnotify(int, struct parameter *);
int com_channel(int, struct parameter *);
int com_inchannel(int, struct parameter *);
int com_gmatch(int, struct parameter *);
int com_tmatch(int, struct parameter *);
int com_goematch(int, struct parameter *);
int com_cmatch(int, struct parameter *);
int com_decline(int, struct parameter *);
int com_withdraw(int, struct parameter *);
int com_pending(int, struct parameter *);
int com_accept(int, struct parameter *);
int com_refresh(int, struct parameter *);
int com_open(int, struct parameter *);
int com_bell(int, struct parameter *);
int com_style(int, struct parameter *);
int com_promote(int, struct parameter *);
int com_alias(int, struct parameter *);
int com_unalias(int, struct parameter *);
int create_new_gomatch(int, int, int, int, int, int, int, int);
int com_servers(int, struct parameter *);
int com_sendmessage(int, struct parameter *);
int com_messages(int, struct parameter *);
int com_clearmess(int, struct parameter *);
int com_variables(int, struct parameter *);
int com_mailsource(int, struct parameter *);
int com_mailhelp(int, struct parameter *);
int com_handles(int, struct parameter *);
int com_znotify(int, struct parameter *);
int com_addlist(int, struct parameter *);
int com_sublist(int, struct parameter *);
int com_showlist(int, struct parameter *);
int com_news(int, struct parameter *);
int com_beep(int, struct parameter *);
int com_qtell(int, struct parameter *);
int com_getpi(int, struct parameter *);
int com_getps(int, struct parameter *);
int com_pass(int, struct parameter *);
int com_undo(int, struct parameter *);
int com_komi(int, struct parameter *);
int com_handicap(int, struct parameter *);
int com_ladder(int, struct parameter *);
int com_join(int, struct parameter *);
int com_drop(int, struct parameter *);
int com_score(int, struct parameter *);
int com_awho(int, struct parameter *);
int com_review(int, struct parameter *);
int com_watching(int, struct parameter *);
int com_clntvrfy(int, struct parameter *);
int com_mailme(int, struct parameter *);
int com_me(int, struct parameter *);
int com_pme(int, struct parameter *);
int com_teach(int, struct parameter *);
int com_admins(int, struct parameter *);
int com_spair(int, struct parameter *);
int com_nocaps(int, struct parameter *);
int com_reset(int, struct parameter *);
int com_translate(int, struct parameter *);
int com_find(int, struct parameter *);
int com_best(int, struct parameter *);
int com_ctitle(int, struct parameter *);
int com_lock(int, struct parameter *);
int com_unlock(int, struct parameter *);
int com_invite(int, struct parameter *);
int com_emote(int, struct parameter *);
int com_mailmess(int, struct parameter *);
int com_note(int, struct parameter *);
int com_shownote(int, struct parameter *);
int com_ping(int, struct parameter *);

#if WANT_NNGSRATED
int com_nsuggest(int, struct parameter *);
int com_nrating(int, struct parameter *);
#endif
int com_orating(int, struct parameter *);
int com_osuggest(int, struct parameter *);

int plogins(int, FILE *);
int com_dnd(int, struct parameter *);
int com_which_client(int, struct parameter *);
void AutoMatch(int, int, int *, float *);
int in_list(const char *, const char *);
int in_list_match(const char *, const char *);
int com_lchan(int, struct parameter *);
int com_lashout(int, struct parameter *);

#define CLIENT_UNKNOWN		0
#define CLIENT_TELNET		1
#define CLIENT_XIGC		2
#define CLIENT_WINIGC		3
#define CLIENT_WIGC		4
#define CLIENT_CGOBAN		5
#define CLIENT_JAVA		6
#define CLIENT_TGIGC		7
#define CLIENT_TGWIN		8
#define CLIENT_FIGC		9
#define CLIENT_PCIGC		10
#define CLIENT_GOSERVANT	11
#define CLIENT_MACGO		12
#define CLIENT_AMIGAIGC		13
#define CLIENT_HAICLIENT	14
#define CLIENT_IGC		15
#define CLIENT_KGO		16
#define CLIENT_NEXTGO		17
#define CLIENT_OS2IGC		18
#define CLIENT_STIGC		19
#define CLIENT_XGOSPEL		20
#define CLIENT_TKGC		21
#define CLIENT_JAGOCLIENT	22
#define CLIENT_GTKGO		23

#endif /* COMPROC_H */
