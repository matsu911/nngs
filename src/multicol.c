/* multicol.c
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

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsconfig.h"
#include "common.h"
#include "multicol.h"
#include "utils.h"

/* grimm */
/*char *strchr( char *s, int c);*/
/* added for warning */

struct multicol *multicol_start(int maxArray)
{
  int i;
  struct multicol *m;

  m = malloc(sizeof *m);
  m->arraySize = maxArray;
  m->num = 0;
  m->strArray = malloc(m->arraySize * sizeof *m->strArray);
  for (i = 0; i < m->arraySize; i++)
    m->strArray[i] = NULL;
  return m;
}

int multicol_store(struct multicol * m, const char *str)
{
  if (m->num >= m->arraySize)
    return -1;
  if (!str)
    return -1;
  m->strArray[m->num] = mystrdup(str);
  m->num++;
  return 0;
}

int multicol_store_sorted(struct multicol *m, const char *str)
/* use this instead of milticol_store to print a list sorted */
{
  int i;  int found = 0;
  if (m->num >= m->arraySize)
    return -1;
  if (!str)
    return -1;
  for (i = m->num; (i > 0) && (!found); i--) {
    if (strcasecmp(str, m->strArray[i-1]) >= 0) {
      found = 1;
      m->strArray[i] = mystrdup(str);
    } else {
      m->strArray[i] = m->strArray[i-1];
    }
  }
  if (!found) m->strArray[0] = mystrdup(str);
  m->num++;
  return 0;
}

int multicol_pprint(struct multicol * m, int player, int cols, int space)
{
  int i;
  int maxWidth = 0;
  int numPerLine;
  int numLines;
  int on, theone, len;
  int done;
  int temp;

  pprintf(player, "\n");
  for (i = 0; i < m->num; i++) {
    temp = strlen(m->strArray[i]);	/* loon: yes, this is pathetic */
    if (strchr(m->strArray[i], '\033'))
      temp -= 8;
    if (temp > maxWidth)
      maxWidth = temp;
  }
  maxWidth += space;
  numPerLine = cols / maxWidth;
  numLines = m->num / numPerLine;
  if (numLines * numPerLine < m->num)
    numLines++;
  on = 0;
  done = 0;
  while (!done) {
    for (i = 0; i < numPerLine; i++) {
      theone = on + numLines * i;
      if (theone >= m->num) {
	break;
      }
      temp = strlen(m->strArray[theone]);	/* loon: yes, still pathetic */
      if (strchr(m->strArray[theone], '\033'))
	temp -= 8;
      len = maxWidth - temp;
      if (i == numPerLine - 1)
	len -= space;
      pprintf(player, "%s ", m->strArray[theone]);
      while (len) {
	pprintf(player, " ");
	len--;
      }
    }
    pprintf(player, "\n");
    on += 1;
    if (on >= numLines)
      break;
  }
  return 0;
}

int multicol_end(struct multicol * m)
{
  int i;
  for (i = 0; i < m->num; i++)
    free(m->strArray[i]);
  free(m);
  return 0;
}
