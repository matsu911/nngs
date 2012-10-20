/*
 * ip_ban.c ban ranges of ip adresses.
*/

/*
    NNGS - The No Name Go Server
    Copyright (C) 2004 Adriaan van Kessel

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

#include <stdio.h>
#include <stdlib.h>

#if WANT_MAIN
#define WANT_DUMP_RANGETREE 1
#endif
/*
 * Rangetree-structure for exclusion of ip-adresses.
 * Basic operations are add-range, remove-range
 * check-if-XX-is-in-range.
 * Ranges are split/merged when needed, eg
 * [1-9][11-19] + [10] <<-->> [1-19]
 * There is an attempt to keep the binary tree balanced
 * by applying obvious and cheap rotations on-the-fly.
 */
struct range {
  struct range **hnd; /* pointer to parent's pointer to this */
  struct range *nxt;
  struct range *prv;
  unsigned bot; /* lower bound of this range, inclusive */
  unsigned top; /* upper bound of this range, inclusive */
  };

static struct range * rangeroot = NULL;
static unsigned range_count = 0;
/****************************** interface */
#include "ip_ban.h"
/****************************** node manip */
static struct range **range_hnd(struct range **hnd, unsigned val);
static void node_app(struct range **hnd, struct range *np);
static void node_del(struct range *np);
static struct range *node_harvest(struct range *np);
/****************************** helpers */
static void range_dump0(FILE *fp, struct range *np);
static void node_rotate(struct range *old,struct range *new);
static struct range * range_harvest(unsigned bot, unsigned top);
/****************************** base ops */
static struct range * node_new(unsigned bot, unsigned top);
static void node_free(struct range *np);
static void node_cut(struct range *np);
static void node_glue(struct range **hnd, struct range *np);
static void node_dump(struct range *np, int lev);
static void node_dump0(FILE *fp, struct range *np, int lev);
/****************************** interface */
int range_check(unsigned bot, unsigned top)
{
  struct range **hnd ;
  unsigned tbot,ttop,cnt;

  if (bot==top) {
    hnd = range_hnd(&rangeroot, bot);
    return (hnd && *hnd) ? 1 : 0;
  }
  tbot = bot;
  ttop = 0;
  for(cnt=0;range_iter(&tbot, &ttop); ) {
    if (ttop < bot) continue;
    if (tbot > top) break;
    cnt++;
  }
  return cnt;
}

	/* Iterate through the tree. (rather clumsily ...)
	** on entry, pb and pt point to the values found on the previous call
	** , on return they point to the new range
	** ranges are returned in increasing order.
	** on the first call, set (*pb > *pt) to start things up.
	*/
int range_iter(unsigned *pb, unsigned *pt)
{
  unsigned val;
  struct range *best, *np, *match;

#if WANT_DUMP_RANGETREE
fprintf(stderr,"[Range_iter(%X,%X)]", pb ? *pb: 0,pt ? *pt: 0);
#endif

  best = NULL;
  if (*pb > *pt) {
    for(np = rangeroot; np ; ) {
      best = np;
      np = np->prv;
      }
    goto found ;
    }

  match = NULL;
  val = *pb;
  for(np= rangeroot; np ; ) {
#if WANT_DUMP_RANGETREE >1
    fprintf(stderr,"[%x,%x]", np->bot, np->top);
#endif
    if (val < np->bot) { /* need to descend */
      best = np;
      np = np->prv;
      continue;
      }
    else if (val > np->top) { /* or ascend */
      np = np->nxt; continue;
      }
    else  {  /* found old value ... */
      if (match) break; /* been there ... */
      match = np;
      np = np->nxt;
      }
    }

found:
  if (!best) return 0;
  *pb = best->bot;
  *pt = best->top;
  return 1;
}

int range_add(unsigned bot, unsigned top)
{
  struct range *yield, *np;
  int cnt ;

#if WANT_DUMP_RANGETREE
  fprintf(stderr,"[Range_add(%X,%X)]", bot,top);
#endif

  yield = range_harvest(bot-1, top+1);
  if (yield) yield->hnd = &yield;

  for(cnt=0;(np = node_harvest(yield)); cnt--) {
    if (np->bot < bot) bot = np->bot;
    if (np->top > top) top = np->top;
#if WANT_DUMP_RANGETREE
    fprintf(stderr, "[bot=%X,top=%X]Harv=", bot,top );
         node_dump0(stderr, np, 0);
#endif
    node_free(np);
  }

  np = node_new(bot, top);
  node_app(&rangeroot, np);

  return cnt+1;
}

	/* Remove range [bot,top] ,inclusive from the tree.
	** All exceptions are handled, causing nodes to be removed
	** reduced, fragmented, etc.
	** The tree might be rebalanced on the fly.
	*/
int range_remove(unsigned bot, unsigned top)
{
  struct range *yield, *pp, *p2;
  int ins,del;

#if WANT_DUMP_RANGETREE
  fprintf(stderr,"[Range_remove(%X,%X)]", bot,top);
#endif
	/* extract the piece of the three that will be affected */
  yield = range_harvest(bot, top);
  if (!yield) return 0;

	/* consume the snipped subtree node by node.
	** Filter each node in it. For simpicity, the
	** original node is always deleted, the relevant sections
	** are copied into 0,1 or 2 new nodes.
	** The new nodes (if any) are reinserted into the
	** real tree.
	*/
  yield->hnd = &yield;
  for(ins=del=0; (pp = node_harvest(yield)); del++  ) {
    if (pp->top > top ) { /* keep upper side: adjust bot */
      p2 = node_new(top+1, pp->top);
      node_app(&rangeroot, p2);
      ins++;
      }
    if (pp->bot < bot) { /* keep lower side: adjust top */
      p2 = node_new(pp->bot, bot -1);
      node_app(&rangeroot, p2);
      ins++;
      }
    node_free(pp);
    }
  return (del) ? -del: ins;
}

void range_dump(FILE *fp)
{
  range_dump0(fp, rangeroot);
}

static void range_dump0(FILE *fp, struct range *np)
{
  if (!np) return;
  fprintf(fp, "%x %x\n", np->bot, np->top);
  if (np->prv) range_dump0(fp, np->prv);
  if (np->nxt) range_dump0(fp, np->nxt);
}
/****************************** node manip */
	/* find point (handle) in tree where val should be.
	** If 'val' currently exists, the return (*hnd) points to it,
	** if not found, (*hnd) points to the place where it should be.
	** While walking the tree, we attempt to do some trivial rotations.
	** ("vine-compacting")
	*/
static struct range **range_hnd(struct range **hnd, unsigned val)
{
  struct range *pp, *pl,*pr;

  while ((pp = *hnd)) {
    pl = pp->prv;
    pr = pp->nxt;
    if (!pl && pr && !pr->prv && pr->nxt) { /* rotate; make pr new root */
      node_rotate(pp,pr);
      continue;
    }
    /*else*/ if (!pr && pl && !pl->nxt && pl->prv) { /* idem; pl new root */
      node_rotate(pp,pl);
      continue;
    }
	/* Grandmother's rotation ... */
    /*else*/ if (pl && !pl->prv && !pl->nxt && pr && pr->prv && pr->nxt) {
      node_rotate(pp,pr);
      continue;
    }
    /*else*/ if (pr && !pr->prv && !pr->nxt && pl && pl->prv && pl->nxt) {
      node_rotate(pp,pl);
      continue;
    }
    if (val < pp->bot) {
           hnd = &pp->prv;
           continue;  }
    /*else*/ if (val > pp->top) {
           hnd = &pp->nxt;
           continue;  }
    /*else*/ break; /* found match */
    }

  return hnd;
}

static void node_app(struct range **hnd, struct range *np)
{
  hnd = range_hnd(hnd, np->bot);
  node_glue(hnd, np); return;
}

static void node_del(struct range *np)
{
  struct range **hnd,*child ;

  hnd = np->hnd;
  node_cut(np);

	/* simple cases: ther are zero or one child nodes.
	** If any: make them the new root
	*/
  if (!np->prv && !np->nxt) return;
  if (np->prv && !np->nxt) {
    node_glue(hnd, np->prv);
    np->prv = NULL;
    return;
  }
  if (!np->prv && np->nxt) {
    node_glue(hnd, np->nxt);
    np->nxt = NULL;
    return;
  }
	/* If both children are present: consume their subtrees,
	** and re-insert them onto the new root.
	** This can be costly, but there is no other way.
	** the rand() is an attempt to avoid ordered insertions -->vines
	*/
  while (np->prv && np->nxt) {
    child = node_harvest( (rand() &1) ? np->prv : np->nxt);
    node_app(hnd, child);
  }
  while ((child = node_harvest(np->prv) )) {
    node_app(hnd, child);
  }
  while ((child = node_harvest(np->nxt) )) {
    node_app(hnd, child);
  }
}

	/* Harvest one node from a tree.
	** We attempt to eat the leaves last, but we don't want to
	** rebalance, etc.
	** So we find a node with only one child (or no child at all),
	** replace it by that child, and return the node.
	** (all this is intended to avoid a sorted order when a tree is
	** harvested until empty)
	*/ 
static struct range *node_harvest(struct range *np)
{
  struct range **hnd;

  if (!np) return NULL;

	/* skip nodes with two children */
  while (np->prv && np->nxt) {
    np = (rand() &1) ? np->prv : np->nxt;
  }

  hnd = np->hnd;
  node_cut(np);
  if (!np->prv && !np->nxt) return np;
  if (np->prv && !np->nxt) {
    node_glue(hnd, np->prv);
    np->prv = NULL;
    return np;
  }
  if (!np->prv && np->nxt) {
    node_glue(hnd, np->nxt);
    np->nxt = NULL;
    return np;
  }
return np;
}

/****************************** helpers */
	/* Harvest al nodes which overlap some of the [bot,top] range
	** The nodes are removed from the original tree, and
	** returned as (a pointer to) a new tree.
	*/
static struct range * range_harvest(unsigned bot, unsigned top)
{
struct range *yield,**hnd,*pp;

yield = NULL;
for (hnd = &rangeroot; (pp = *hnd);  ) {
  if (pp->bot > top) { hnd = &pp->prv; continue; }
  if (pp->top < bot) { hnd = &pp->nxt; continue; }
  node_del(pp);
  node_app(&yield, pp);
  }

return yield;
}

static void node_rotate(struct range *old,struct range *new)
{
  struct range **hnd;

  hnd = old->hnd;
  node_cut(old);
  node_cut(new);

#if WANT_DUMP_RANGETREE
  fprintf(stderr, "Rot Old=" ); node_dump(old, 0);
  fprintf(stderr, "Rot New=" ); node_dump(new, 0);
#endif
  node_glue(hnd, new);
/* this causes 3-recursion : node_app->node_hnd->node_rotate->node_app */
  node_app(hnd, old);
}

/****************************** base ops */
struct range * node_new(unsigned bot, unsigned top)
{
  struct range *np;

  np = malloc(sizeof *np);
  np->hnd = NULL; np->nxt = np->prv = NULL;
  np->bot = bot;
  np->top = top;
  range_count++;
  return np;
}

void node_free(struct range *np)
{
  free(np);
  range_count--;
}

static void node_dump(struct range *np, int lev)
{

  if (!np) {
    fprintf(stderr, "[Empty]"); return; }
  node_dump0(stderr, np, lev);
  if (lev < 0) lev = -lev;

  if (np->prv) { node_dump(np->prv, -1-lev); }
  if (np->nxt) { node_dump(np->nxt, lev+1); }
}

static void node_dump0(FILE *fp, struct range *np, int lev)
{
  int ii;

  if (lev < 0) {
    lev = -lev; for(ii=0; ii < lev; ii++) fputc(' ', fp);
    fprintf(fp,"Prv=" );
  }
  else if (lev > 0) {
    for(ii=0; ii < lev; ii++) fputc(' ', fp);
    fprintf(fp,"Nxt=" );
  }
  fprintf(fp,"{Bot=%x Top=%x}\n" , np->bot, np->top);
}

static void node_cut(struct range *np)
{
  if (np->hnd && *np->hnd) {
    *np->hnd = NULL; np->hnd = NULL;
  }
}

static void node_glue(struct range **hnd, struct range *np)
{
  if (hnd) *hnd = np;
  if (np) np->hnd = hnd;
}

#if WANT_MAIN
int main()
{
  char buff[100];
  int narg;
  char *cmd, *cp;
  unsigned u0, u1, u2;

  while (fgets(buff, sizeof buff, stdin)) {
    for(cp = buff; *cp == ' '; cp++) ;
    for(cmd = cp++; *cp == ' '; cp++) ;
    narg = sscanf(cp,"%x %x %x", &u0, &u1, &u2);
    switch (*cmd) {
    case '+': case 'a': case 'A':
      if (narg< 1) continue;
      if (narg>= 3) {
        while (1) {
          range_add(u0, u1);
          if (u0 + u2 < u0) break;
          if (u1 + u2 < u1) break;
          u0 += u2; u1 += u2;
        }
      continue;
      }
      if (narg< 2) u1=u0 ;
      range_add(u0, u1);
      break;
    case '-': case 'd': case 'D':
      if (narg< 1) continue;
      if (narg>= 3) {
        while (1) {
          range_remove(u0, u1);
          if (u0 + u2 < u0) break;
          if (u1 + u2 < u1) break;
          u0 += u2; u1 += u2;
        }
        continue;
      }
      if (narg< 2) u1=u0 ;
      range_remove(u0, u1);
      break;
    case '?': case 'c': case 'C':
      if (narg< 1) continue;
      narg = range_check(u0);
      fprintf(stderr,"Chk=%d\n", narg);
      break;
    case 'q': case 'Q':
      goto quit;
    case 'w': case 'W':
      range_dump(stderr); break;
    case 'p': case 'P':
    default:
      node_dump(rangeroot, 0);
    }
  }
quit:
  return 0;
}
#endif /* WANT_MAIN */
/****************************** eof */
