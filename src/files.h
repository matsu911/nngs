/* files.h
 * constants used by the xyfopen() function
 */
#ifndef FILES_H
#define FILES_H

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  Erik Van Riper (geek@imageek.york.cuny.edu)
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

#define FILENAME_CMDS 1
#define FILENAME_ACMDS 2
#define FILENAME_INFO 3
	/*
	** NameConvention used for filename #defines:
	** (player- and game- dirs):
	** The _c_ uses the (first) playername's first char as a subdir
	** The _b_,_w_ are like bw, but use either black's or white's name
	** The _s_ versions take an char* arg and use it
	**
	** (language subdirs):
	** The _q_ versions take *no* arg and insert a two-letter language dir
	** The _p_ versions take a language arg and insert the two-letter subdir
	** The _l_ versions take an int arg and insert the two-letter subdir
        ** The filename construction relies on 
	** _q=(_p+0) and _l==(_p +1) and _s=(_l + 1)
	*/
#define FILENAME_HELP 4
#define FILENAME_HELP_q 5
#define FILENAME_HELP_p 5
#define FILENAME_HELP_l (FILENAME_HELP+2)
#define FILENAME_HELP_s 7
#define FILENAME_HELP_q_s 8
#define FILENAME_HELP_p_s 8
#define FILENAME_HELP_l_s 9
#define FILENAME_HELP_s_s 10
#define FILENAME_HELP_l_index (FILENAME_HELP_l+5)
#define FILENAME_HELP_s_index (FILENAME_HELP_s+5)

#define FILENAME_AHELP 14
#define FILENAME_AHELP_q 15
#define FILENAME_AHELP_p 15
#define FILENAME_AHELP_l (FILENAME_AHELP+2)
#define FILENAME_AHELP_s 17
#define FILENAME_AHELP_q_s 18
#define FILENAME_AHELP_p_s 18
#define FILENAME_AHELP_l_s 19
#define FILENAME_AHELP_s_s 20
#define FILENAME_AHELP_l_index (FILENAME_AHELP_l+5)
#define FILENAME_AHELP_s_index (FILENAME_AHELP_s+5)

#define FILENAME_PLAYER 24
#define FILENAME_PLAYER_cs 25
#define FILENAME_PLAYER_cs_DELETE 26
#define FILENAME_PLAYER_cs_GAMES 27
#define FILENAME_PLAYER_cs_LOGONS 28
#define FILENAME_PLAYER_cs_MESSAGES 30

#define FILENAME_GAMES_s 31
#define FILENAME_GAMES_c 32
#define FILENAME_GAMES_bs_s 33
#define FILENAME_GAMES_ws_s 34

#define FILENAME_CGAMES 35
#define FILENAME_CGAMES_c 36
#define FILENAME_CGAMES_cs 37
#define FILENAME_CGAMES_ws_s_s 38
#define FILENAME_CGAMES_bs_s_s 39

#define FILENAME_RATINGS 40
#define FILENAME_NRATINGS 41
#define FILENAME_RESULTS 42
#define FILENAME_NRESULTS 43

#define FILENAME_LADDER9 45
#define FILENAME_LADDER19 46

#define FILENAME_NEWS_s 50
#define FILENAME_NEWSINDEX 51
#define FILENAME_ADMINNEWS_s 52
#define FILENAME_ADMINNEWSINDEX 53
#define FILENAME_NOTEFILE 54
#define FILENAME_LOGONS 55

#define FILENAME_MESS_WELCOME 60
#define FILENAME_MESS_UNREGISTERED 61
#define FILENAME_MESS_MOTD 62
#define FILENAME_MESS_MOTDs 63
#define FILENAME_MESS_AMOTD 64
#define FILENAME_MESS_LOGIN 65
#define FILENAME_MESS_LOGOUT 66

#define FILENAME_EMOTE 70
#define FILENAME_FIND 71
#define FILENAME_LISTINDEX 72
#define FILENAME_LIST_s 73
#define FILENAME_LIST_s_OLD 74
#define FILENAME_LIST_BAN 75

#define FILENAME_PROBLEM_d 76
#define FILENAME_SPOOL 80
#define FILENAME_SPOOL_s 81
#define FILENAME_SPOOL_sd 82

#endif /* FILES_H */
