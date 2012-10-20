/* command_list.h
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

#ifndef COMMAND_LIST_H
#define COMMAND_LIST_H

#undef WANT_LANGUAGE

#include "nngsconfig.h"
#include "comproc.h"
#include "gameproc.h"
#include "adminproc.h"

/*
  Parameter string format
  w - a word 
  o - an optional word 
  d - integer
  p - optional integer
  i - word or integer
  n - optional word or integer
  s - string to end of line
  t - optional string to end of line
  
  If the parameter option is given in lower case then the parameter is 
  converted to lower case before being passsed to the function. If it is
  in upper case, then the parameter is passed as typed.
 */
/*
 * AvK: added qsort: list does *not* have to be sorted.
 * Sentinel has been removed.
 */
 /* Name	Options	Functions	Security */
struct command_type command_list[] = {
  {"aban",             "ooo",   com_aban,       ADMIN_ADMIN },
  {"accept",		"n",	com_accept,	ADMIN_USER },
  {"actitle",           "dS",   com_actitle,    ADMIN_ADMIN },
  {"addlist",           "ww",   com_addlist,   	ADMIN_ADMIN },
  {"addtime",           "d",    com_moretime,  	ADMIN_USER },
  {"adjourn",           "",     com_adjourn,   	ADMIN_USER },
  {"admins",            "",     com_admins,   	ADMIN_USER },
  {"adrop",             "wd",   com_adrop,      ADMIN_MASTER },
  {"ahelp",             "o",    com_adhelp,     ADMIN_ADMIN },
  {"alias",		"oT",	com_alias,	ADMIN_USER },
  {"all",		"n",	com_allob,	ADMIN_USER },
  {"announce",          "S",    com_announce,   ADMIN_ADMIN },
  {"arank",             "ww",   com_arank,      ADMIN_MASTER },
  {"asetadmin",         "wd",   com_asetadmin,  ADMIN_DEMIGOD },
  {"asetemail",         "wO",   com_asetemail,  ADMIN_ADMIN },
  {"asetpasswd",        "wW",   com_asetpasswd, ADMIN_ADMIN },
  {"asetrealname",      "wT",   com_asetrealname, ADMIN_ADMIN },
  {"asetsilent",        "w",    com_asetsilent, ADMIN_MASTER },
  {"asetwater",         "wd",   com_asetwater,  ADMIN_ADMIN },
  {"ashout",            "S",    com_ashout,     ADMIN_ADMIN },
  {"ausers",            "",     com_ausers,     ADMIN_ADMIN },
  {"awho",		"O",	com_awho,	ADMIN_USER },
  {"ayt",           	"",     com_ayt,    	ADMIN_USER },
  {"beep",		"w",	com_beep,	ADMIN_USER },
  {"bell",		"",	com_bell,	ADMIN_USER },
  {"best",		"no",	com_best,	ADMIN_USER },
  {"bmuzzle",           "o",    com_bmuzzle,     ADMIN_MASTER },
  {"censor",		"o",	com_censor,	ADMIN_USER },
  {"channel",		"p",	com_channel,	ADMIN_USER },
  {"chkip",             "w",    com_chk_ip,    ADMIN_ADMIN },
  {"chkpl",             "w",    com_chk_player, ADMIN_ADMIN },
  {"chksc",             "d",    com_chk_socket, ADMIN_ADMIN },
  {"choice",            "w",    com_choice,     ADMIN_USER },
  {"client",		"d",	com_which_client,	ADMIN_USER },
  {"clntvrfy",		"T",    com_clntvrfy,   ADMIN_USER },
  {"ctitle",            "dS",   com_ctitle,     ADMIN_USER },
  {"date",		"",	com_date,	ADMIN_USER },
  {"debug",             "p",    com_asetdebug,  ADMIN_DEMIGOD },
  {"decline",		"n",	com_decline,	ADMIN_USER },
  {"dnd",               "d",    com_dnd,        ADMIN_USER },
  {"done",		"",	com_done,	ADMIN_USER },
  {"drop",		"o",	com_drop,	ADMIN_USER },
  {"emote",		"wwT",	com_emote,	ADMIN_USER },
  {"erase",		"",	com_clearmess,	ADMIN_USER },
  {"event",		"S",	com_event,	ADMIN_USER },
  {"find",		"s",	com_find,	ADMIN_USER },
  {"free",		"",	com_free,	ADMIN_USER },
  {"fteach",		"",	com_pteach,	ADMIN_USER },
  {"games",		"o",	com_games,	ADMIN_USER },
  {"ginfo",		"p",	com_ginfo,	ADMIN_USER },
  {"git", 		"S",	com_git,	ADMIN_USER },
  {"gmuzzle",           "o",    com_gmuzzle,    ADMIN_MASTER },
  {"goematch",		"wwdd", com_goematch,	ADMIN_USER },
  {"gshout",            "S",    com_gshout,     ADMIN_USER },
  {"handicap",		"d",	com_handicap,	ADMIN_USER },
  {"handles",           "w",    com_handles,    ADMIN_USER },
  {"help",		"o",	com_help,	ADMIN_USER },
  {"hide",              "d",    com_hide,       ADMIN_MASTER },
  {"inchannel",		"pp",	com_inchannel,	ADMIN_USER },
  {"info",		"S",	com_info,	ADMIN_USER },
  {"invite",		"wp",	com_invite,	ADMIN_USER },
  {"it",                "S",    com_it,         ADMIN_USER },
  {"join",              "o",    com_join,       ADMIN_USER },
  {"kibitz",            "NT",   com_kibitz,     ADMIN_USER },
  {"komi",              "f",    com_komi,       ADMIN_USER },
  {"ladder",            "",     com_ladder,     ADMIN_USER },
  {"lashout",		"",	com_lashout,	ADMIN_ADMIN },
  {"last",              "",     com_llogons,    ADMIN_USER },
  {"lchan",		"p",	com_lchan,	ADMIN_USER },
  {"load",              "s",    com_load,       ADMIN_USER },
  {"lock",              "d",    com_lock,       ADMIN_USER },
  {"logons",            "o",    com_logons,     ADMIN_USER },
  {"look",              "s",    com_look,       ADMIN_USER },
  {"mail",	        "ws",   com_mailme,     ADMIN_USER },
  {"match",		"wwddd",com_gmatch,	ADMIN_USER },
  {"messages",		"oT",	com_messages,	ADMIN_USER },
  {"mhelp",          	"o",    com_mailhelp,   ADMIN_USER },
  {"mmess",          	"",     com_mailmess,   ADMIN_USER },
  {"moreinfo",          "",	com_infor,      ADMIN_USER },
  {"moretime",          "d",    com_moretime,   ADMIN_USER },
  {"moves",		"n",	com_gomoves,	ADMIN_USER },
  {"muzzle",            "o",    com_muzzle,     ADMIN_MASTER },
  {"next", 		"",	com_more, 	ADMIN_USER },
  {"noshout",           "",     com_noshout,    ADMIN_MASTER },
  {"nuke",              "w",    com_nuke,       ADMIN_DEMIGOD },
  {"observe",		"n",	com_observe,	ADMIN_USER },
  {"open",		"",	com_open,	ADMIN_USER },
  {"password",		"WW",	com_password,	ADMIN_USER },
  {"pause",		"",	com_pause,	ADMIN_USER },
  {"pending",		"",	com_pending,	ADMIN_USER },
  {"ping",		"",	com_ping,	ADMIN_USER }, 
  {"pose",		"wS",	com_pose,	ADMIN_DEMIGOD },
  {"post",              "S",    com_note,       ADMIN_USER },
  {"problem",		"p",	com_problem,	ADMIN_USER }, 
  {"pzz",		"wT",	com_pme,	ADMIN_USER },
  {"qu",		"",	com_quit,	ADMIN_USER },
  {"quit",		"",	com_quit,	ADMIN_USER },
  {"rank",		"S",	com_ranked,	ADMIN_USER },
  {"refresh",		"p",	com_refresh,	ADMIN_USER },
  {"register",         "WWT",  com_register,   ADMIN_USER },
  {"reload_ladders",    "",     com_reload_ladders,  ADMIN_DEMIGOD  },
  {"remplayer",         "w",    com_remplayer,  ADMIN_DEMIGOD  },
  {"reset",		"",  	com_reset,   	ADMIN_USER },	
  {"resign",		"",	com_resign,	ADMIN_USER },
  {"results",		"o",	com_history,	ADMIN_USER },
  {"rresults",		"o",	com_rhistory,	ADMIN_USER },
  {"save",		"",	com_save,	ADMIN_USER },
  {"say",		"S",	com_say,	ADMIN_USER },
  {"score",             "p",    com_score,      ADMIN_USER },
  {"set",		"wT",	com_set,	ADMIN_USER },
  {"sgf",		"o",	com_sgf,	ADMIN_USER },
  {"shout",		"S",	com_shout,	ADMIN_USER },
  {"showlist",          "o",    com_showlist,   ADMIN_ADMIN },

  {"shutdown",          "o",    com_shutdown,   ADMIN_DEMIGOD },
  {"spair",		"wwww",	com_spair,	ADMIN_USER },
  {"sresign",		"s",	com_sresign,	ADMIN_USER }, 
  {"stats",		"o",	com_stats,	ADMIN_USER },
  {"status",		"nn",	com_status,	ADMIN_USER },
  {"stored",		"o",	com_stored,	ADMIN_USER },
  {"sublist",           "ww",   com_sublist,    ADMIN_ADMIN },
  {"teach",		"d",	com_teach,	ADMIN_USER },
  {"tell",		"nT",	com_tell,	ADMIN_USER },
  {"time",		"n",	com_time,	ADMIN_USER },
  {"title",		"S",	com_title,	ADMIN_USER },
  {"tmatch",		"wwddd",com_tmatch,	ADMIN_USER },
  {"toggle",		"wT",	com_set,	ADMIN_USER },
  {"touch",		"s",	com_touch,	ADMIN_USER },
  {"translate",		"W",	com_translate,	ADMIN_USER },
  {"unalias",		"w",	com_unalias,	ADMIN_USER },
  {"uncensor",		"o",	com_uncensor,	ADMIN_USER },
  {"undo",		"p",	com_undo,	ADMIN_USER },
  {"unfree",            "",     com_unfree,     ADMIN_USER },
  {"unhide",            "d",    com_unhide,     ADMIN_MASTER },
  {"unlock",            "d",    com_unlock,     ADMIN_USER },
  {"unobserve",		"n",	com_observe,	ADMIN_USER },
  {"unpause",		"",	com_unpause,	ADMIN_USER },
  {"uptime",		"",	com_uptime,	ADMIN_USER },
  {"variables",		"o",	com_variables,	ADMIN_USER },
  {"watching",		"",	com_watching,	ADMIN_USER },
  {"who",		"T",	com_who,	ADMIN_USER },
  {"withdraw",		"n",	com_withdraw,	ADMIN_USER },

#if WANT_UNWANTED
  {"nocaps",		"",	com_nocaps,	ADMIN_USER },
  {"raisedead",         "w",    com_raisedead,  ADMIN_DEMIGOD  },
  {"review",		"T",	com_review,	ADMIN_USER },
#endif

#if WANT_LADDER_SIFT
  {"forcerecalc",        "",    com_rating_recalc, ADMIN_DEMIGOD },
#endif /* WANT_LADDER_SIFT */

  {"orating",           "o",    com_orating,     ADMIN_USER },
  {"osuggest",		"wo",	com_osuggest,	ADMIN_USER },

#if WANT_NNGSRATED
  {"rating",		"o",	com_nrating,	ADMIN_USER },
  {"suggest",		"wo",	com_nsuggest,	ADMIN_USER },
#endif

#if WANT_PAIR
  {"teamgo",		"d",	com_pair,	ADMIN_USER }, 
  {"pair",		"d",	com_pair,	ADMIN_USER }, 
#endif

#if WANT_NOTIFY
  {"znotify",		"",	com_znotify,	ADMIN_USER },  
  {"notify",		"o",	com_notify,	ADMIN_USER }, 
  {"unnotify",		"o",	com_unnotify,	ADMIN_USER }, 
#endif

#if NEWS_BB
  {"createadmnews",     "S",    com_createadmnews, ADMIN_MASTER },
  {"createnews",        "S",    com_createnews,  ADMIN_USER },
  {"news",	        "o",    com_news,       ADMIN_USER },
  {"anews",             "o",    com_anews,      ADMIN_ADMIN },
  {"zz",		"S",	com_me,		ADMIN_USER },
#endif
  /* by Syncanph */
  {"shownote",          "",     com_shownote,   ADMIN_USER },
};

struct alias_type g_alias_list[] = {
  {"/",		"emote "},
  {"w",		"who"},
  {"h",		"help"},
  {"t",		"tell"},
  {"m",		"match"},
  {"d",		"draw"},
  {"a",		"accept"},
  {"sh",	"shout"},
  {"vars",	"variables"},
  {"players",	"who A"},
  {"p",		"who A"},
  {"pl",	"who A"},
  {"o",		"observe"},
  {"r",		"refresh"},
  {"in",	"inchannel"},
  {"channels",	"inchannel"},
  {",",		"say"},
  {".",		"tell ."},
  {"*",		"pzz *"},
  {"^",		"tell ^"},
  {";",		"tell ,"},
  {"yell",	"tell ,"},
  {"`",         "next"},
  {"!",		"shout "},
  {"%",		"clntvrfy "},
  {"I",		"tell ."},
  {"i",		"tell ."},
  {"?",		"help "},
  {"exit",	"quit"},
  {"logout",	"quit"},
  {"'",      	"kibitz "},
  {"\"",      	"kibitz "},
  {"ma",	"match"},
  {"+",         "addlist "},
  {"-",         "sublist "},
  {"=",         "showlist "},
  {"#",		"ashout "},
  {"yy",	"zz"},
  {"user",	"awho "},
  {"users",	"awho "},
  {"ladder19",	"best 19"},
  {"ladder9",	"best 9"},
  {"pair-go",	"pair "},
  {"adhelp",    "ahelp"},
  {NULL, NULL}
};

#endif /* COMMAND_LIST_H */
