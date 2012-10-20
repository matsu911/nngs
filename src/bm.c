/*
    NNGS - The No Name Go Server
    Copyright (C) 1995  J. Alan Eldridge (alane@wozzle.york.cuny.edu)

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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "bm.h"

int
bmInit(char *pat, struct boyermoore *pBM)
{
  int i, l;

  l = strlen(pat);
  pBM->pat = pat;
  for (i = 0; i < 256; i++)
    pBM->delta1[i] = l;
  i = l;
  while (*pat)
    pBM->delta1[(unsigned)*pat++] = --l;
  return pBM->p = i - 1;
}

char *
bmSrch(char *str, struct boyermoore *pBM)
{
  int i, j, slen, last;
  
  slen = strlen(str) - 1;

  i = pBM->p;
  while (i <= slen) {
    j = pBM->p;
    last = i;
    while (str[i] == pBM->pat[j]) {
      if (j > 0) {
        j--;
        i--;
      } else
        return str + i;
    }
    i += pBM->delta1[(unsigned)str[i]];
    if (i <= last)
      i = last + 2;
  }
  return 0;
}
