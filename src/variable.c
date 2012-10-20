/* variable.c
 */

/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1998 Erik Van Riper (geek@nngs.cosmic.org)
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "nngsconfig.h"
#include "nngsmain.h"
#include "variable.h"
#include "common.h"
#include "command.h"
#include "servercodes.h"
#include "playerdb.h"
#include "utils.h"
#include "plan.h"
#include "channel.h"
static int set_boolean_var(int *var, const char *val);

static int set_boolean_var(int *var, const char *val)
{
  int v = -1;

  if (!val)
    return (*var = !*var); /* Toggle */

  if (sscanf(val, "%d", &v) != 1) {
    if (!strcasecmp(val, "off")) v = 0;
    else if (!strcasecmp(val, "false")) v = 0;
    else if (!strcasecmp(val, "on")) v = 1;
    else if (!strcasecmp(val, "true")) v = 1;
  }
  if (v == 0 || v == 1) return (*var = v);
  else return -1;
}


static int set_open(int p, const char *var, const char *val)
{
  int v = set_boolean_var (&parray[p].flags.is_open, val);
  UNUSED(var);

  if (v < 0) return VAR_BADVAL;
  if (!v) {
      player_decline_offers(p, -1, PEND_MATCH);
      player_withdraw_offers(p, -1, PEND_MATCH);
      parray[p].flags.is_looking = 0;
  }
  pcn_out(p, CODE_INFO, FORMAT_SET_OPEN_TO_BE_s_, (parray[p].flags.is_open) ? "True" : "False");
  return VAR_OK;
}


static int set_ropen(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var (&parray[p].ropen, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_ROPEN_TO_BE_s_, (parray[p].ropen) ? "True" : "False");
  return VAR_OK;
}


static int set_shout(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var (&parray[p].i_shout, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_SHOUT_TO_BE_s_, (parray[p].i_shout) ? "True" : "False");
  if (parray[p].i_shout) channel_add(CHANNEL_SHOUT, p);
  else channel_remove(CHANNEL_SHOUT, p);
  return VAR_OK;
}

static int set_client(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].flags.is_client, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_CLIENT_TO_BE_s_, (parray[p].flags.is_client)  ? "True" : "False");
  if (parray[p].flags.is_client) parray[p].i_verbose = 0;
  return VAR_OK;
}

static int set_lshout(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].i_lshout, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_LSHOUT_TO_BE_s_, (parray[p].i_lshout)  ? "True" : "False");
  if (parray[p].i_lshout) channel_add(CHANNEL_LSHOUT, p);
  else channel_remove(CHANNEL_LSHOUT, p);

  return VAR_OK;
}

static int set_gshout(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].i_gshout, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_GSHOUT_TO_BE_s_, (parray[p].i_gshout)  ? "True" : "False");
  if (parray[p].i_gshout) channel_add(CHANNEL_GSHOUT, p);
  else channel_remove(CHANNEL_GSHOUT, p);
  return VAR_OK;
}

static int set_looking(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].flags.is_looking, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_LOOKING_TO_BE_s_, (parray[p].flags.is_looking)  ? "True" : "False");
  if (parray[p].flags.is_looking) parray[p].flags.is_open = 1;
  return VAR_OK;
}

static int set_kibitz(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].i_kibitz, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_KIBITZ_TO_BE_s_, (parray[p].i_kibitz)  ? "True" : "False");
  return VAR_OK;
}

static int set_tell(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].flags.want_tells, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_TELL_TO_BE_s_, (parray[p].flags.want_tells)  ? "True" : "False");
  return VAR_OK;
}

static int set_robot(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].i_robot, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_ROBOT_TO_BE_s_, (parray[p].i_robot)  ? "True" : "False");
  return VAR_OK;
}

static int set_notifiedby(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var(&parray[p].forget.notifiedby, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_NOTIFIED_TO_BE_s_, (parray[p].forget.notifiedby) ? "True" : "False");
  return VAR_OK;
}

static int set_verbose(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var (&parray[p].i_verbose, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_VERBOSE_TO_BE_s_, (parray[p].i_verbose) ? "True" : "False");
  return VAR_OK;
}

static int set_pinform(int p, const char *var, const char *val)
{
  UNUSED(var);

  if (set_boolean_var (&parray[p].flags.want_logins, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_PINFORM_TO_BE_s_, (parray[p].flags.want_logins) ? "True" : "False");
  if (parray[p].flags.want_logins) channel_add(CHANNEL_LOGON, p);
  else channel_remove(CHANNEL_LOGON, p);
  return VAR_OK;
}

static int set_quiet(int p, const char *var, const char *val)
{
  int  quiet;
  UNUSED(var);

  quiet = ~(parray[p].flags.want_logins || parray[p].flags.want_gshouts);
  if (set_boolean_var (&quiet, val) < 0) return VAR_BADVAL;
  parray[p].flags.want_logins = !quiet;
  parray[p].flags.want_gshouts = !quiet;

  pcn_out(p, CODE_INFO, FORMAT_SET_QUIET_TO_BE_s_, (parray[p].flags.want_logins) ? "False" : "True");
  if (parray[p].flags.want_logins) {
    channel_add(CHANNEL_LOGON, p);
    channel_add(CHANNEL_GAME, p);
  }
  else {  
    channel_remove(CHANNEL_LOGON, p);
    channel_remove(CHANNEL_GAME, p);
  }

  return VAR_OK;
}

static int set_ginform(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (set_boolean_var (&parray[p].flags.want_gshouts, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_GINFORM_TO_BE_s, (parray[p].flags.want_gshouts) ? "True" : "False");
  if (parray[p].flags.want_gshouts) channel_add(CHANNEL_GAME, p);
  else channel_remove(CHANNEL_GAME, p);

  return VAR_OK;
}

static int set_private(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (set_boolean_var (&parray[p].Private, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_PRIVATE_TO_BE_s_, (parray[p].Private) ? "True" : "False");
  return VAR_OK;
}

static int set_automail(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (set_boolean_var (&parray[p].automail, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_AUTOMAIL_TO_BE_s_, parray[p].automail ? "True" : "False");
  return VAR_OK;
}

static int set_extprompt(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (set_boolean_var (&parray[p].extprompt, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_EXTPROMPT_TO_BE_s_, parray[p].extprompt ? "True" : "False");
  return VAR_OK;
}

static int set_bell(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (set_boolean_var (&parray[p].flags.want_bell, val) < 0) return VAR_BADVAL;
  pcn_out(p, CODE_INFO, FORMAT_SET_BELL_TO_BE_s_, parray[p].flags.want_bell ? "True" : "False");
  return VAR_OK;
}

static int set_rank(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (!val) {
    do_copy(parray[p].rank, "-", sizeof parray[0].rank);
    pcn_out(p, CODE_INFO, FORMAT_UNSET_YOUR_RANK_INFO);
    return VAR_OK;
  }
  if (strlen(val) > MAX_RANK) {
    pcn_out(p, CODE_ERROR, FORMAT_THAT_STRING_IS_TOO_LONG_d_CHARACTERS_MAX_n, MAX_RANK);
    return VAR_BADVAL;
  }
  do_copy(parray[p].rank, val, sizeof parray[0].rank);

  pcn_out(p, CODE_INFO, FORMAT_RANK_SET_TO_s_n, parray[p].rank);
  return VAR_OK;
}


static int set_realname(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (!val) {
    do_copy(parray[p].fullname, "Not Provided", sizeof parray[0].fullname);
    pcn_out(p, CODE_INFO, FORMAT_UNSET_YOUR_REAL_NAME_INFO);
    return VAR_OK;
  }
  if (strlen(val)>MAX_FULLNAME) {
    pcn_out(p, CODE_INFO, FORMAT_THAT_STRING_IS_TOO_LONG_d_CHARACTERS_MAX_n, MAX_FULLNAME);
    return VAR_BADVAL;
  }
  do_copy(parray[p].fullname, val, sizeof parray[0].fullname);

  pcn_out(p, CODE_INFO, FORMAT_FULL_NAME_SET_TO_s_n, parray[p].fullname);
  return VAR_OK;
}


static int set_time(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 0 || v > 300) return VAR_BADVAL;
  parray[p].def_time = v;
  pcn_out(p, CODE_INFO, FORMAT_DEFAULT_TIME_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_language(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 0 || v >= LANGUAGE_COUNT) return VAR_BADVAL;
  parray[p].language = v;
  pcn_out(p, CODE_INFO, FORMAT_LANGUAGE_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_size(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 9 || v > 19) return VAR_BADVAL;
  parray[p].def_size = v;
  pcn_out(p, CODE_INFO, FORMAT_DEFAULT_SIZE_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_channel(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 0 || v >= MAX_NCHANNEL) return VAR_BADVAL;
  parray[p].last_channel = v;
  pcn_out(p, CODE_INFO, FORMAT_DEFAULT_CHANNEL_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_byo_time(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 0 || v > 300) return VAR_BADVAL;
  parray[p].def_byo_time = v;
  pcn_out(p, CODE_INFO, FORMAT_DEFAULT_BYO_YOMI_TIME_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_byo_stones(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 0 || v > 100) return VAR_BADVAL;
  parray[p].def_byo_stones = v;
  pcn_out(p, CODE_INFO, FORMAT_DEFAULT_BYO_YOMI_STONES_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_height(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 5 || v > 240) return VAR_BADVAL;
  parray[p].d_height = v;
  pcn_out(p, CODE_INFO, FORMAT_HEIGHT_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_width(int p, const char *var, const char *val)
{
  int v = -1;
  UNUSED(var);

  if (!val) return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1) return VAR_BADVAL;
  if (v < 32 || v > 240) return VAR_BADVAL;
  parray[p].d_width = v;
  pcn_out(p, CODE_INFO, FORMAT_WIDTH_SET_TO_d_n, v);
  return VAR_OK;
}


static int set_prompt(int p, const char *var, const char *val)
{
  if (!val) {
  UNUSED(var);
    do_copy(parray[p].prompt, "#> ", sizeof parray[0].prompt);
    return VAR_OK;
  }
  do_copy(parray[p].prompt, val, sizeof parray[0].prompt);
  return VAR_OK;
}


static int set_busy(int p, const char *var, const char *val)
{
  UNUSED(var);
  if (!val) {
    parray[p].busy[0] = '\0';
    pcn_out( p, CODE_INFO, FORMAT_YOUR_BUSY_STRING_WAS_CLEARED_n);
    return VAR_OK;
  }
  if (strlen(val) >= sizeof parray[p].busy) {
    pcn_out(p, CODE_ERROR, FORMAT_THAT_STRING_IS_TOO_LONG_n);
    return VAR_BADVAL;
  }
  do_copy(parray[p].busy,val, sizeof parray[p].busy);
    pcn_out( p, CODE_INFO, FORMAT_YOUR_BUSY_STRING_IS_SET_TO_qsqn, parray[p].busy);
  return VAR_OK;
}


#if 1
/* the prettier version of set_plan() */
static int set_plan(int p, const char *var, const char *val)
{
  size_t count = plan_count(parray[p].plan_lines);
  int where = atoi(var);	/* Must be an integer, no test needed. */

  /* The commands uses origin 1, with 0 meaning insert-in-front. We use
     0-origin, with -1 meaning insert-in-front. */
  where -= 1;

  if (where >= MAX_PLAN)
    return VAR_BADVAL;
  if (where < 0)
  {				/* Insert or remove first. */
    if (!val)
    {
      if (count > 0)
	plan_rem(0, parray[p].plan_lines); /* Remove first. */
    }
    else
    {
      plan_insert(val, 0, parray[p].plan_lines); /* Insert first. */
      if (count+1 > MAX_PLAN)
	plan_rem(MAX_PLAN, parray[p].plan_lines); /* Remove overflow. */
    }
  }
  else if (where >= (int)count)
  {				/* Add or remove last. */
    if (val)
      plan_add(val, parray[p].plan_lines); /* Add last. */
    else if ((int)count > 0 && where == (int)count-1)
      plan_rem(where, parray[p].plan_lines); /* Remove last. */
  }
  else				/* 0 <= where < count */
  {				/* Replace or clear line. */
    if (val)
      plan_set(val, where, parray[p].plan_lines); /* Replace line. */
    else if (count > 0)
    {
      if (where == (int)count-1)
	plan_rem(where, parray[p].plan_lines); /* Clear line. */
      else
	plan_set("", where, parray[p].plan_lines); /* Clear line. */
    }
  }

  return VAR_OK;
}
#else
/* this is really ugly. (Indeed, so it was replaced with new code /pem) */
static int set_plan(int p, const char *var, const char *val)
{
  int which, i;

  which = atoi(var);		/* Must be an integer, no test needed */

  if (which > MAX_PLAN) return VAR_BADVAL;

  if (which > parray[p].num_plan) which = parray[p].num_plan + 1;

  if (which == 0) {		/* shove from top */
    if (parray[p].num_plan >= MAX_PLAN) {	/* free the bottom string's
						   memory */
      if (parray[p].planLines[parray[p].num_plan - 1])
      {
	free(parray[p].planLines[parray[p].num_plan - 1]);
	parray[p].planLines[parray[p].num_plan - 1] = NULL;
      }
    }
    if (parray[p].num_plan) {
      for (i = (parray[p].num_plan >= MAX_PLAN) ? MAX_PLAN - 1 : parray[p].num_plan; i > 0; i--)
	parray[p].planLines[i] = parray[p].planLines[i - 1];
    }
    if (parray[p].num_plan < MAX_PLAN)
      parray[p].num_plan++;
    parray[p].planLines[0] = (val) ? mystrdup(val) : NULL;
    return VAR_OK;
  }
  if (which > parray[p].num_plan) {	/* new line at bottom */
    if (parray[p].num_plan >= MAX_PLAN) {	/* shove the old lines up */
      if (parray[p].planLines[0])
      {
	free(parray[p].planLines[0]);
	parray[p].planLines[0] = NULL;
      }
      for (i = 0; i < parray[p].num_plan; i++)
	parray[p].planLines[i] = parray[p].planLines[i + 1];
    } else {
      parray[p].num_plan++;
    }
    parray[p].planLines[which - 1] = (val) ? mystrdup(val): NULL);
    return VAR_OK;
  }
  which--;
  if (parray[p].planLines[which]) {
    free(parray[p].planLines[which]);
    parray[p].planLines[which] = NULL;
  }
  if (val) {
    parray[p].planLines[which] = mystrdup(val);
  } else {
    parray[p].planLines[which] = NULL;
    if (which == parray[p].num_plan - 1) {	/* clear nulls from bottom */
      for ( ;parray[p].num_plan > 0; parray[p].num_plan--) {
        if (parray[p].planLines[parray[p].num_plan - 1]) break;
      }
    } else if (which == 0) {	/* clear nulls from top */
      for (;which < parray[p].num_plan; which++) {
        if (parray[p].planLines[which] ) break;
      }
      if (which != parray[p].num_plan) {
	for (i = which; i < parray[p].num_plan; i++)
	  parray[p].planLines[i - which] = parray[p].planLines[i];
      }
      parray[p].num_plan -= which;
    }
  }
  return VAR_OK;
}
#endif /* off */

struct var_list variables[] = {
  {"0", set_plan},
  {"1", set_plan},
  {"2", set_plan},
  {"3", set_plan},
  {"4", set_plan},
  {"5", set_plan},
  {"6", set_plan},
  {"7", set_plan},
  {"8", set_plan},
  {"9", set_plan},
  {"automail", set_automail},
  {"bell", set_bell},
  {"beep", set_bell},
  {"busy", set_busy},
  {"byo_time", set_byo_time},
  {"byo_stones", set_byo_stones},
  {"channel", set_channel},
  {"client", set_client},
  {"def_time", set_time},
  {"def_size", set_size},
  {"def_byo_time", set_byo_time},
  {"def_byo_stones", set_byo_stones},
  {"extprompt", set_extprompt},
  {"ginform", set_ginform},
  {"game", set_ginform},
  {"gshout", set_gshout},
  {"lshout", set_lshout},
  {"height", set_height},
  {"i_game", set_ginform},
  {"i_login", set_pinform},
  {"kibitz", set_kibitz},
  {"language", set_language},
  {"looking", set_looking},
  {"notifiedby", set_notifiedby},
  {"open", set_open},
  {"pinform", set_pinform},
  {"player", set_pinform},
  {"private", set_private},
  {"prompt", set_prompt},
  {"quiet", set_quiet},
  {"rank", set_rank},
  {"realname", set_realname},
  {"ropen", set_ropen},
  {"robot", set_robot},
  {"shout", set_shout},
  {"size", set_size},
  {"stones", set_byo_stones},
  {"tell", set_tell},
  {"time", set_time},
  {"verbose", set_verbose},
  {"width", set_width},
  {NULL, NULL}
};


static int set_find(const char *var)
{
  int i = 0;
  int gotIt = -1;
  int len = strlen(var);

  while (variables[i].name) {
    if (!strncmp(variables[i].name, var, len)) {
      if (gotIt >= 0) {
	return -VAR_AMBIGUOUS;
      }
      gotIt = i;
    }
    i++;
  }
  if (gotIt >= 0) {
    return gotIt;
  }
  return -VAR_NOSUCH;
}


int var_set(int p, const char *var, const char *val, int *wh)
{
  int which;

  if (!var)
    return VAR_NOSUCH;
  if ((which = set_find(var)) < 0) {
    return -which;
  }
  *wh = which;
  return variables[which].var_func(p, (isdigit((int)*variables[which].name) ? var : variables[which].name), val);
}

