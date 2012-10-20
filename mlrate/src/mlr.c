/* $Id: mlr.c,v 1.5 2002/06/24 16:19:36 pem Exp $
**
** Per-Erik Martin (pem@pem.nu) 1997-12.
**
** The Maximum Likelihood rating algorithm.
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
#include <math.h>
#include <assert.h>		/* Disable with -DNDEBUG */
#include "mlr.h"
#include "rank.h"
#include "player.h"
#include "game.h"
#include "circular.h"
#include "error.h"

/* The termination condition on maximum change of any player's rating
** in one turn of the outer loop.
*/
#ifndef CHANGE_LIMIT
#define CHANGE_LIMIT 0.001
#endif

/* The termination condition of the rating interval of one player in
** the inner (bisection) loop
*/
#ifndef CLOSE_ENOUGH
#define CLOSE_ENOUGH 0.0005
#endif

/* A safety. Don't run more than this many turns in the outer loop.
*/
#ifndef GLOBAL_TURNS_MAX
#define GLOBAL_TURNS_MAX 1000
#endif

#define FABS(X)  ((X) < 0.0 ? (-(X)) : (X))

/* The magic constant.
**
** Popular values are:
**   9/16   (IGS, probably too high)
**   4/9    (NNGS, ok, but maybe too high as well)
**   1/e    (feels right :)
*/
double Mlrate_s = (4.0/9.0);

#define S Mlrate_s


/* Note:
**   This is the derivatives of 0.5 * S^x and 1 - 0.5 * S^x, short of
**   some multiplicative constants. You can get other combinations of
**   the signs, but the equation solver in mlrate() depends on this!
*/
static double
dP(double x, int win)
{
  double tmp = 0;

  if (win)
    if (x < 0)
      return -1;
    else
    {
      tmp = pow(S,x);
      return tmp/(tmp-2);
    }
  else
    if (x > 0)
      return 1;
    else
    {
      tmp = pow(S,-x);
      return tmp/(2-tmp);
    }
}

static double
P(double x, int win)
{
  if (win)
    if (x < 0)
      return (pow(S,-x)/2);
    else
      return (1 - pow(S,x)/2);
  else
    if (x > 0)
      return (pow(S,x)/2);
    else
      return (1 - pow(S,-x)/2);
}

void
mlrate(double mean, FILE *fdiag)
{
  double maxchange;		/* The max change of one turn. */
  size_t pcount, gcount, rcount; /* Player, game, and removed counters. */
  size_t wcount, lcount;	/* Win/loss counters. */
  double wsum, lsum;		/* Weighted sums. */
  size_t globturns = 0;		/* Counts the turns of the outer loop. */
  size_t hcount[10];		/* Handicap game counters. */
  player_t p;			/* A player. */
  piter_t piter;		/* An iterator over players. */
  char *pflags;

  assert(0.0 < mean && mean < RANK_MAXIMUM);

  pflags = (char *)malloc(player_count());
  if (pflags == NULL)
    errex("malloc(%lu) failed", player_count());
  memset(pflags, 0, player_count());

  /* Assign a start rating for every player. */
  player_start(&piter);
  while ((p = player_next(&piter)) != NO_PLAYER)
    if (!player_get_rated(p) && !player_get_ignore(p))
    {
      player_set_rank(p, mean);
      player_set_rated(p, 1);
    }

  /* Remove players with no wins or no losses against other rated 
  ** players; then again and again, until no more can be removed.
  ** While we're at it, count some statistics as well.
  */
  do {
    pcount = gcount = rcount = 0;
    hcount[0] = hcount[1] = hcount[2] = hcount[3] = hcount[4] =
      hcount[5] = hcount[6] = hcount[7] = hcount[8] = hcount[9] = 0;
    player_start(&piter);
    while ((p = player_next(&piter)) != NO_PLAYER)
      if (player_get_rated(p) && !player_get_ignore(p))
      {
	game_t g;
	giter_t giter;
	unsigned oppcount = 0;

	wcount = lcount = 0;
	wsum = lsum = 0.0;

	player_games_start(p, &giter);
	while ((g = player_games_next(&giter)) != NO_GAME)
	  if (game_weight(g) > 0.0)
	  {
	    double a = game_advantage(g);

	    if (a >= 10.0)
	      hcount[0] += 1;
	    else if (a >= 1.0)
	      hcount[(unsigned)floor(a)] += 1;
	    if (p == game_winner(g) && player_get_rated(game_loser(g)))
	    {
	      wcount += 1;
	      wsum += game_weight(g);
	      if (!pflags[game_loser(g)])
	      {
		pflags[game_loser(g)] = 1;
		oppcount += 1;
	      }
	    }
	    else if (p == game_loser(g) && player_get_rated(game_winner(g)))
	    {
	      lcount += 1;
	      lsum += game_weight(g);
	      if (!pflags[game_winner(g)])
	      {
		pflags[game_winner(g)] = 1;
		oppcount += 1;
	      }
	    }
	    else
	      game_set_weight(g, 0.0);
	  }
	  else
	    game_set_weight(g, 0.0);

	if (wsum < 0.25 || lsum < 0.25 || oppcount < 3)
	{
	  player_set_rated(p, 0);
	  rcount += 1;
	}
	else
	{
	  pcount += 1;
	  gcount += wcount + lcount;
	}
	player_set_ratedgames(p, wcount, lcount);
	player_set_wratedgames(p, wsum, lsum);

	/* Clear flags. */
	player_games_start(p, &giter);
	while ((g = player_games_next(&giter)) != NO_GAME)
	  pflags[game_loser(g)] = pflags[game_winner(g)] = 0;
      }
  } while (rcount > 0);

  player_gc_games();

  if (fdiag)
  {
    int i;

    fprintf(fdiag, "\nRemaining:\n%6lu players\n%6lu games\n\n",
	    (unsigned long)pcount, (unsigned long)gcount/2);
    for (i = 1 ; i <= 9 ; i++)
      fprintf(fdiag, "Advantage   %2d: %5lu games\n",
	      i, (unsigned long)hcount[i]);
    fprintf(fdiag,   "Advantage >=10: %5lu games\n\n",
	    (unsigned long)hcount[0]);
  }
  if (pcount == 0 || gcount == 0)
    errex("No player or no games");

  /*
  ** The outer loop.
  */
  do {	/* while (maxchange > CHANGE_LIMIT && globturns < GLOBAL_TURNS_MAX); */

    int maxp = 0;
    pcount = 0;
    maxchange = 0.0;
    globturns += 1;

    /*
    **  Loop over all players.
    */
    player_start(&piter);
    while ((p = player_next(&piter)) != NO_PLAYER)
    {
      /*
      **  We use bisection to find the root of the derivative (the maximum).
      */
      irank_t r, oldrank;
      irank_t ileft = RANK_MINIMUM, iright = RANK_MAXIMUM;

      if (!player_get_rated(p) || player_get_ignore(p))
	continue;

      /*
      ** Inner (bisection) loop.
      */
      pcount += 1;
      r = oldrank = player_get_rank(p);
      do {			/*  while (iright - ileft > CLOSE_ENOUGH); */
	game_t g;
	double sum = 0.0;
	giter_t giter;

	player_games_start(p, &giter);
	while ((g = player_games_next(&giter)) != NO_GAME)
	{
	  player_t opp;
	  double diff;

	  if (p == game_winner(g))
	  {
	    opp = game_loser(g);

	    if (player_get_rated(opp))
	    {
	      diff = RANK_DIFF(r, player_get_rank(opp))
		+ game_advantage(g);
	      sum += dP(diff, 1) * game_weight(g);
	    }
	  }
	  else
	  {
	    opp = game_winner(g);

	    if (player_get_rated(opp))
	    {
	      diff = RANK_DIFF(r, player_get_rank(opp))
		- game_advantage(g);
	      sum += dP(diff, 0) * game_weight(g);
	    }
	  }
	}

	if (sum > 0.0)
	  iright = r;		/* Root's somewhere to the left. */
	else
	  ileft = r;		/* Root's somewhere to the right. */
	r = (iright + ileft)/2;

      } while (iright - ileft > CLOSE_ENOUGH);

      if (r > oldrank)
      {
	if (r - oldrank > maxchange)
	{
	  maxchange = r - oldrank;
	  maxp = p;
	}
      }
      else
      {
	if (oldrank - r > maxchange)
	{
	  maxchange = oldrank - r;
	  maxp = p;
	}
      }
      player_set_rank(p, r);

    }	/* while ((p = player_next())) */

#ifdef MAXP
    fprintf(stderr, "\n--- Maxp: %s rank=%g ww=%g wl=%g w=%u l=%u rg=%u\n",
	    player_get_name(maxp),
	    player_get_rank(maxp),
	    player_get_wwins(maxp),
	    player_get_wlosses(maxp),
	    player_get_wins(maxp),
	    player_get_losses(maxp),
	    player_get_ratedgames(maxp));
#endif

    if (globturns > 100)
      circular_check(maxp);

    if (fdiag)
    {
      fprintf(fdiag, " %3lu: %6.3f", (unsigned long)globturns, maxchange);
      if (globturns % 5)
	fflush(fdiag);
      else
	fputc('\n', fdiag);
    }

  } while (maxchange > CHANGE_LIMIT && globturns < GLOBAL_TURNS_MAX);

  if (fdiag)
  {
    if (globturns % 5)
      fputc('\n', fdiag);
    fputc('\n', fdiag);
  }
  if (globturns == GLOBAL_TURNS_MAX)
    errex("Aborted after maximum %u turns\n", GLOBAL_TURNS_MAX);

  if (pflags != NULL)
    free(pflags);
}


/*
**   Calculating an "error" interval.
**
** Note that the sum of the logarithm of the P values always is negative
** (that is, the player_P() value).
** This means that we can't simply multiply the value at the maximum point
** (the rating) with a percentage.
** This error function is the result of empricical studies of the behaviour
** of the logarithmic ML curve.
*/

static double
players_P(player_t p, double r)
{
  double sum = 0.0;
  size_t count = 0;
  game_t g;
  giter_t giter;

  player_games_start(p, &giter);
  while ((g = player_games_next(&giter)) != NO_GAME)
    if (p == game_winner(g))
    {
      player_t opp = game_loser(g);

      if (player_get_rated(opp))
	sum += log(P(RANK_DIFF(r, player_get_rank(opp)) + game_advantage(g),
		     1))
	  * game_weight(g);
      count += 1;
    }
    else
    {
      player_t opp = game_winner(g);

      if (player_get_rated(opp))
	sum += log(P(RANK_DIFF(r, player_get_rank(opp)) - game_advantage(g),
		     0))
	  * game_weight(g);
      count += 1;
    }
  return sum;
}

void
mlrate_errors(FILE *fdiag)
{
  player_t p;
  piter_t piter;

  player_start(&piter);
  while ((p = player_next(&piter)) != NO_PLAYER)
    if (player_get_rated(p) && !player_get_ignore(p))
    {
      irank_t r = player_get_rank(p);
      double plevel;
      irank_t low, lowleft, lowright, high, highleft, highright;

      lowright = highleft = r;
      plevel = 1.0/log(player_get_ratedgames(p) + 1.0) - players_P(p, r);
      lowleft = RANK_MINIMUM;
      highright = RANK_MAXIMUM;

      do {
	low = (lowleft + lowright)/2;
	high = (highleft + highright)/2;

	if (players_P(p, low) + plevel > 0)
	  lowright = low;	/* Root's somewhere to the left. */
	else
	  lowleft = low;	/* Root's somewhere to the right. */
	if (players_P(p, high) + plevel < 0)
	  highright = high;	/* Root's somewhere to the left. */
	else
	  highleft = high;	/* Root's somewhere to the right. */

      } while (lowright - lowleft > CLOSE_ENOUGH &&
	       highright - highleft > CLOSE_ENOUGH);

      player_set_low(p, low-r);
      player_set_high(p, high-r);
    }
}
