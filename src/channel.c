/* channel.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "common.h"
#include "command.h"
#include "channel.h"
#include "network.h"
#include "playerdb.h"
#include "utils.h"

struct channel carray[MAX_CHANNELS];

void channel_init()
{
  int i;

  for (i = 0; i < MAX_CHANNELS; i++) {
    carray[i].is_special = 0;
    carray[i].is_locked = 0;
    carray[i].is_hidden = 0;
    carray[i].is_private = 0;
    carray[i].Num_Yell = 0;
    carray[i].members = malloc(MAX_CHANNEL_MEMBERS * sizeof *carray[i].members);
    carray[i].count = 0;
   
    switch(i) {
    case 0:
      carray[i].ctitle = mystrdup("Admins House");
      carray[i].is_hidden = 1;
      break;

    case 5:
      carray[i].ctitle = mystrdup("Tournament Players Lounge");
      break;

    case 12:
      carray[i].ctitle = mystrdup("Robin's hood");
      break;

    case 22:
      carray[i].ctitle = mystrdup("Easy conversation in Japanese");
      break;

    case 33:
      carray[i].ctitle = mystrdup("NNGS Bar and Grill");
      break;

    case 42:
      carray[i].is_private = 1;
      carray[i].ctitle = mystrdup("To set the channel title, see \"help ctitle\"");
      break;

    case 46:
      carray[i].ctitle = mystrdup("Den svenska kanalen (Swedish)");
      break;

    case 49:
      carray[i].ctitle = mystrdup("Der deutsche Kanal (German)");
      break;
      
    case 81:
      /* The Japanese Channel =
	 ｢日本語チャンネル」 */
      carray[i].ctitle = mystrdup("｢日本語チャンネル」 (Japanese)");
      break;

    case 82:
      carray[i].ctitle = mystrdup("The Korean channel");
      break;

    case 88:
      /* The Chinese Channel =
	 ﾖﾐﾎﾄﾆｵｵﾀ = 0xd6 0xd0 0xce 0xc4 0xc6 0xb5 0xb5 0xc0 */
      carray[i].ctitle = mystrdup("ﾖﾐﾎﾄﾆｵｵﾀ (Chinese)");
      break;

    case 99:
      carray[i].ctitle = mystrdup("The Girls Club / Happy Channel :-)");
      break;

    case CHANNEL_ASHOUT:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("Admin Shout");
      carray[i].is_hidden = 1;
      break;

    case CHANNEL_SHOUT:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("Normal Shouts");
      break;

    case CHANNEL_GSHOUT:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("GoBot Shouts");
      break;

    case CHANNEL_LOGON:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("Logons");
      break;

    case CHANNEL_GAME:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("Games");
      break;

    case CHANNEL_LSHOUT:
      carray[i].is_special = 1;
      carray[i].ctitle = mystrdup("Ladder Shouts");
      break;

    default:
      carray[i].ctitle = mystrdup("No Title");
    }
  }
}

int on_channel(int ch, int p)
{
  int i;

  for (i = 0; i < carray[ch].count; i++)
    if (p == carray[ch].members[i])
      return 1;
  return 0;
}

int channel_remove(int ch, int p)
{
  int i, found;

  found = -1;
  for (i = 0; i < carray[ch].count && found < 0; i++)
    if (p == carray[ch].members[i]) found = i;
  if (found < 0) return 1;
  for (i = found; i < carray[ch].count - 1; i++)
    carray[ch].members[i] = carray[ch].members[i + 1];
  carray[ch].count -= 1;
  parray[p].nochannels -= 1;
  if (carray[ch].count == 0) {
    carray[ch].is_locked = 0;
    carray[ch].is_hidden = 0;
  }
  if (parray[p].nochannels < 0) parray[p].nochannels = 0;
  return 0;
}

int channel_add(int ch, int p)
{
  if (ch == 0 && parray[p].adminLevel < ADMIN_MASTER) return 3;

  if (carray[ch].count >= MAX_CHANNEL_MEMBERS) return 1;

  if (on_channel(ch, p)) return 1;

  if (carray[ch].is_locked && parray[p].adminLevel < ADMIN_MASTER) return 4;
  if (carray[ch].is_hidden && parray[p].adminLevel < ADMIN_MASTER) return 5;
  if (parray[p].nochannels == MAX_INCHANNELS 
    && parray[p].adminLevel < ADMIN_MASTER) return 2;
  carray[ch].members[carray[ch].count] = p;
  carray[ch].count++;
  parray[p].nochannels++;
  
  return 0;
}

int add_to_yell_stack(int ch, char *text)
{
  int i;

  /* First, check if stack is full.  If so, remove last yell,
     shuffle rest down. */

  if (carray[ch].Num_Yell == YELL_STACK_SIZE) {
    free(carray[ch].Yell_Stack[0]);
    for (i = 0; i < YELL_STACK_SIZE-1; i++) {
      carray[ch].Yell_Stack[i] = carray[ch].Yell_Stack[i + 1];
    }
    carray[ch].Num_Yell  -= 1;
  }

  carray[ch].Yell_Stack[carray[ch].Num_Yell++] = mystrdup(text);
  
  return carray[ch].Num_Yell;
}

