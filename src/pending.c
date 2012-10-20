
/*
    NNGS - The No Name Go Server
    Copyright (C) 1995-1996 Erik Van Riper (geek@nngs.cosmic.org)
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif


#include "playerdb.h" /* for PENDING_SIZE */
#include "pending.h"
#include "utils.h"

#ifndef DEBUG_PENDING
#define DEBUG_PENDING 0
#endif

static struct pending pendarray[PENDING_SIZE]; 
static struct pending *pendlist=NULL;
static struct pending *pendfree=NULL;
static struct pending **pendtail=NULL;
static struct{
	int avail , valid ;
	int free , used;
	} pendcnt = {0,0,0,0};

static struct pending * pending_alloc(void);
static void pending_free(struct pending * this);
static void pending_ins(struct pending **hnd, struct pending * this);
static void pending_cut(struct pending * this);
static void pending_add(struct pending * this);
#if DEBUG_PENDING
static char * pending_dmp(void);
#endif /* DEBUG_PENDING */

	/*
	** Initialize array. create linked list under freelist.
	*/
void pending_init(void)
{
  struct pending **hnd;
  size_t ii;

  pendlist= NULL;
  pendtail = &pendlist;
  pendfree= NULL;
  hnd= &pendfree;
  for(ii=0; ii < COUNTOF(pendarray); ii++ ) {
    pendarray[ii].is_valid = 0;
    pendarray[ii].seq = 0;
    pendarray[ii].hnd = hnd;
    *hnd = & pendarray[ii];
    hnd = & pendarray[ii].nxt;
  }
  *hnd = NULL;
pendcnt.avail = ii;
#if DEBUG_PENDING
Logit("Pending_init : %s", pending_dmp());
#endif /* DEBUG_PENDING */
}


static struct pending * pending_alloc(void)
{
  struct pending * this;

  this= pendfree;
  if (!this) return NULL;
  pending_cut(this);
  pendcnt.avail--;
  pendcnt.valid++;
  this->is_valid=1;
  return this;
}


static void pending_free(struct pending * this)
{
  if (!this) return;
  if (pendtail == &this->nxt) pendtail = NULL;
  pending_cut(this);
  pending_ins(&pendfree,this);
  pendcnt.avail++;
  if (this->is_valid) pendcnt.valid++;
  this->is_valid = 0;
}
	/* cut ptr out of linked list. */
static void pending_cut(struct pending * this)
{
  struct pending * nxt;

  if (!this) return;
  nxt = this->nxt;
  if (nxt==this) nxt= NULL;
  if (this->hnd) *(this->hnd) = nxt;
  if (nxt) nxt->hnd = this->hnd;
  this->hnd=NULL;
  this->nxt=NULL;
}

	/* insert ins before *hnd */
static void pending_ins(struct pending **hnd, struct pending * this)
{
  if (!this) return;
  this->nxt = *hnd;
  this->hnd= hnd;
  *hnd = this;
  if (this->nxt) {
    hnd = &this->nxt;
    this = this->nxt;
    this->hnd= hnd;
  }
}

	/* append at tail */
static void pending_add(struct pending * this)
{
  if (!this) return;
  if (!pendtail) pendtail = &pendlist;

  while (*pendtail) pendtail = &(*pendtail)->nxt;
  pending_ins(pendtail,this);
  while (*pendtail) pendtail = &(*pendtail)->nxt;
}

	/* Allocate pending structure */
struct pending *pending_new(int from, int to, int type)
{
  struct pending *new;

  new= pending_alloc();
  if (new) {
    new->seq=0;
    new->whofrom = from;new->whoto = to;new->type = type;
    pending_add(new);
  }
#if DEBUG_PENDING
Logit("Pending_new(%d,%d,%d) := %p%s", from,to,type,new,pending_dmp() );
#endif /* DEBUG_PENDING */
  return new;
}

	/* Free pending structure instance by setting it's is_valid field
        ** to zero.
        ** Later , the actual cutting it out of it's current LL 
	** and inserting it into freelist is done by pending_count()
	** all this is done to support the silly count/find/next
	** iterator.
	*/
void pending_delete(struct pending *this)
{
  if (!this) return;
  if (this->is_valid) pendcnt.valid--;
  this->is_valid=0;
#if DEBUG_PENDING
Logit("Pending_delete(%p:%d,%d,%d) : %s"
	, this,this->whofrom,this->whoto,this->type, pending_dmp() );
#endif /* DEBUG_PENDING */
  return ;
}


struct pending *pending_find(int from, int to, int type)
{
  struct pending *ptr;
  struct pending *nxt;

  for (ptr=pendlist; ptr; ptr=nxt) {
    if (ptr->nxt==ptr) ptr->nxt=NULL;
    nxt = ptr->nxt;
    if (!ptr->is_valid) {
      pending_free(ptr);
      continue;
    }
    if (ptr->whofrom != from && from >= 0) continue;
    if (ptr->whoto != to && to >= 0) continue;
    if (ptr->type != type && type >= 0) continue;
    ptr->seq = 1;
    break;
  }
#if DEBUG_PENDING
if (ptr)
	Logit("Pending_find(%d,%d,%d):= %p:{%d,%d,%d}",from,to,type
	,ptr,ptr->whofrom,ptr->whoto,ptr->type);
else
	Logit("Pending_find(%d,%d,%d):= NULL",from,to,type);
#endif /* DEBUG_PENDING */
  return ptr;
}

struct pending * pending_next(struct pending *this, int from, int to, int type)
{
  unsigned seq=0;
  struct pending *nxt;

  if (!this) return NULL;
  seq=this->seq;
  for (this=this->nxt; this ; this=nxt) {
    if (this->nxt==this) this->nxt=NULL;
    nxt = this->nxt;
    if (!this->is_valid) {
      pending_free(this);
      continue;
    }
    if (this->whofrom != from && from >= 0) continue;
    if (this->whoto != to && to >= 0) continue;
    if (this->type != type && type >= 0) continue;
    this->seq = ++seq;
    break;
  }
#if DEBUG_PENDING
if (this)
	Logit("Pending_next(%d,%d,%d):= %p:{%d,%d,%d}",from,to,type
	,this,this->whofrom,this->whoto,this->type);
else
	Logit("Pending_next(%d,%d,%d):= %p:{%d,%d,%d}",from,to,type);
#endif /* DEBUG_PENDING */
  return this;
}

int pending_count(int from, int to, int type)
{
  struct pending *ptr, *nxt;
  int count=0;

  for (ptr=pendlist; ptr; ptr=nxt) {
    if (ptr->nxt==ptr) ptr->nxt=NULL;
    nxt=ptr->nxt;
    if (!ptr->is_valid) {
      pending_free(ptr);
      continue;
    }
    if (ptr->whofrom != from && from >= 0) continue;
    if (ptr->whoto != to && to >= 0) continue;
    if (ptr->type != type && type >= 0) continue;
    count++;
  }
#if DEBUG_PENDING
Logit("Pending_count(%d,%d,%d):= %d:%s",from,to,type,count, pending_dmp() );
#endif /* DEBUG_PENDING */
  return count;
}


#if DEBUG_PENDING
static char * pending_dmp(void)
{
  int pos;
  struct pending *ptr;
  char buff[100];

  pendcnt.used=0;
  for (ptr=pendlist; ptr; ptr=ptr->nxt) {
    pendcnt.used++;
  }

  pendcnt.free=0;
  for (ptr=pendfree; ptr; ptr=ptr->nxt) {
    pendcnt.free++;
  }

  pos = sprintf(buff,"Avail=%d,Valid=%d,Free=%d,Used=%d"
	, pendcnt.avail,pendcnt.valid,pendcnt.free,pendcnt.used);

return statstr_dup(buff, pos);
}
#endif /* DEBUG_PENDING */
