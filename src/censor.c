/* censor.c
** Per-Erik Martin (pem@nexus.se) 1999-05-08
**
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
 
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
 
#include "utils.h"
#include "censor.h"

#define INITSIZE 8


struct censor
{
  char **censored;
  size_t size, count, iter;
};


size_t censor_count(struct censor * a)
{
  return a ? a->count : 0;
}


struct censor *
censor_init(void)
{
  struct censor * a;

  a = malloc(sizeof *a);
  assert(a != NULL);
  a->size = a->count = 0;
  a->censored = malloc(INITSIZE*sizeof *a->censored);
  assert(a->censored != NULL);
  a->size = INITSIZE;
  return a;
}

/*
** Finds an censored, returns 1 if found, 0 otherwise.
** If found, *ip is set to the location, otherwise *ip is the
** place we last looked at, which is where it should be if it
** was here.
**
** This uses binary search, so the list must of course be sorted
** alphabetically.
*/
static int
censor_index(size_t *ip, const char *c, struct censor * a)
{
  size_t left, right;

  if (!a)
    return 0;

  left = 0;
  right = a->count;
  while (left < right)
  {
    size_t i = (left+right)/2;
    int x = strcasecmp(c, a->censored[i]);

    if (x < 0)
      right = i;
    else if (x > 0)
      left = i+1;
    else
    {
      *ip = i;
      return 1;
    }
  }
  *ip = left;			/* Not found, left is the insertion point. */
  return 0;
}

char *
censor_lookup(const char *c, struct censor * a)
{
  size_t i;

  if (censor_index(&i, c, a))
    return a->censored[i];
  return NULL;
}

int
censor_add(const char *c, struct censor * a)
{
  size_t i;

  assert(a);
  if (censor_index(&i, c, a))
  {				/* Found it, replace at i. */
    free(a->censored[i]);
    a->censored[i] = mystrdup(c);
    return 1;
  }

  /* Not found. Check if we need to resize before inserting. */
  if (a->count == a->size)
  {
    a->size *= 2;
    a->censored = realloc(a->censored, a->size*sizeof *a->censored);
    assert(a->censored != NULL);
  }

  if (i < a->count)
  {				/* Must shuffle the rest first. */
    size_t j;

    for (j = a->count ; j > i ; j--)
      a->censored[j] = a->censored[j-1];
  }
  a->censored[i] = mystrdup(c);
  a->count += 1;
  return 0;
}

int
censor_rem(const char *c, struct censor * a)
{
  size_t i;

  if (censor_index(&i, c, a))
  {
    size_t j;

    /* Zap it. */
    free(a->censored[i]);
    a->censored[i] = NULL;

    /* Shuffle the rest. */
    for (j = i ; j < a->count-1 ; j++)
      a->censored[j] = a->censored[j+1];
    a->count -= 1;
    a->censored[a->count] = NULL;
    return 1;
  }
  return 0;
}

void
censor_start(struct censor * a)
{
  if (a)
    a->iter = 0;
}

int
censor_next(char **cp, struct censor * a)
{
  if (a)
  {
    if (a->iter < a->count)
    {
      if (cp)
	*cp = a->censored[a->iter];
      a->iter += 1;
      return 1;
    }
    a->iter = 0;
  }
  return 0;
}

void
censor_clear(struct censor * c)
{
  if (c && c->censored)
  {
    size_t i = c->count;

    while (i--)
    {
      if (c->censored[i])
      {
	free(c->censored[i]);
	c->censored[i] = NULL;
      }
    }
    c->count = 0;
  }
}

void
censor_free(struct censor * c)
{
  if (c)
  {
    if (c->censored)
    {
      censor_clear(c);
      free(c->censored);
      c->censored = NULL;
    }
    free(c);
  }
}
