/* $Id: rank.c,v 1.3 2002/01/27 18:03:01 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12.
**
**   Copyright (C) 1998-2002  Per-Erik Martin
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include <math.h>
#include "rank.h"

#define DAN_TO_IRANK(D)  (IRANK_DAN_ILIMIT + (D) - 0.5)

#define KYU_TO_IRANK(K)  (IRANK_DAN_ILIMIT - (K) + 0.5)

#define IRANK_TO_RANGE(R) ((R) >= IRANK_DAN_ILIMIT ? 'd' : 'k')

/* Note: This assumes you know R is a dan rank. */
#define IRANK_TO_DAN(R) \
  ((unsigned)((R) > IRANK_DAN_ICEILING ? IRANK_DAN_ICEILING : (R)) \
   - (unsigned)IRANK_DAN_ILIMIT + 1)

/* Note: This assumes you know R is a kyu rank. */
#define IRANK_TO_KYU(R) \
  ((unsigned)IRANK_DAN_ILIMIT - (unsigned)((R) < 0.0 ? 0.0 : (R)))


int
strtoirank(char *s, irank_t *rp)
{
  unsigned long i;
  char *p;

  assert(s != NULL && rp != NULL);

  i = strtoul(s, &p, 10);
  if (p == s)
    return 0;
  switch (*p)
  {
  case 'K': case 'k':
    if (p[1] && strcasecmp(p+1, "yu"))
      return 0;
    else
    {
      if (i < 1 || KYU_FLOOR < i)
	return 0;
      *rp = KYU_TO_IRANK((unsigned)i);
      return 1;
    }
  case 'D': case 'd':
    if (p[1] && strcasecmp(p+1, "an"))
      return 0;
    else
    {
      if (i < 1 || DAN_CEILING < i)
	return 0;
      *rp = DAN_TO_IRANK((unsigned)i);
      return 1;
    }
  }
  return 0;
}

void
iranktostr(irank_t r, char *s, unsigned smax)
{
  char c = IRANK_TO_RANGE(r);

  assert(s != NULL);

  switch (c)
  {
  case 'd':
    snprintf(s, smax, "%ud", IRANK_TO_DAN(r));
    break;
  case 'k':
    snprintf(s, smax, "%uk", IRANK_TO_KYU(r));
    break;
  default:
    break;
  }
}

#define KOMI_PER_STONE 11.0
#define KOMI_LEVEL      5.5

/* Calculate Black's advantage, barring the possible strength difference.
** This is actually not exactly as the traditional handicap is calculated,
** but this is supposedly more correct.
**
**  Hcap  Komi  Trad. Corr.
**    0    5.5  0.00  0.00
**    0    0.5  0.45
**    2    5.5        1.00
**    2    0.5  1.45
**    3    5.5        2.00
**    3    0.5  2.45
**    4    5.5        3.00
**    4    0.5  3.45
**    5    5.5        4.00
**    5    0.5  4.45
**  ...
*/

irank_t
blacks_advantage(double komi, unsigned hcap)
{
  if (hcap == 0)
    hcap = 1;
  return (hcap - (komi + KOMI_LEVEL)/KOMI_PER_STONE);
}

/* For testing. */
void
rank_diff_print(void)
{
#define NONE 0
#define TRAD 1
#define CORR 2
#define BOTH 3

  static struct
  {
    double komi; unsigned hcap; unsigned trad;
  } tset[12] = { {  5.5, 0, BOTH },
		 {  0.5, 0, TRAD },
		 {  5.5, 2, CORR },
		 {  0.5, 2, TRAD },
		 {  5.5, 3, CORR },
		 {  0.5, 3, TRAD },
		 {  5.5, 4, CORR },
		 {  0.5, 4, TRAD },
		 {  5.5, 5, CORR },
		 {  0.5, 5, TRAD },
		 {  0.0, 0, NONE } };

  unsigned i;
  fprintf(stderr, "  Game differences\n  Hcap  Komi  Trad. Corr.\n");
  for (i = 0 ; tset[i].trad != NONE ; i++)
  {
    double a = blacks_advantage(tset[i].komi, tset[i].hcap);

    switch (tset[i].trad)
    {
    case TRAD:
      fprintf(stderr, "  %3u  %5.1f %5.2f\n",
	      tset[i].hcap, tset[i].komi, a);
      break;
    case CORR:
      fprintf(stderr, "  %3u  %5.1f       %5.2f\n",
	      tset[i].hcap, tset[i].komi, a);
      break;
    default:
      fprintf(stderr, "  %3u  %5.1f %5.2f %5.2f\n",
	      tset[i].hcap, tset[i].komi, a, a);
      break;
    }
  }
  fprintf(stderr, "  ...\n");
}
