/* plan.c
** Per-Erik Martin (pem@nexus.se) 1999-05-08
**
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "utils.h"
#include "plan.h"

#define INITSIZE 10

struct plan
{
  char **lines;
  size_t size, count, iter;
};

size_t plan_count(struct plan * p)
{
  return p ? p->count : 0;
}


struct plan *
plan_init(void)
{
  struct plan * p;

  p = malloc(sizeof *p);
  assert(p != NULL);
  p->size = p->count = 0;
  p->lines = malloc(INITSIZE * sizeof *p->lines);
  assert(p->lines != NULL);
  p->size = INITSIZE;
  return p;
}


char *
plan_get(size_t i, struct plan * p)
{
  return i < p->count ? NULL : p->lines[i];
}


void
plan_insert(const char *s, size_t i, struct plan * p)
{
  size_t max = (i > p->count ? i+1 : p->count+1);

  while (max >= p->size)
  {
    p->size *= 2;
    p->lines = realloc(p->lines, p->size*sizeof *p->lines);
    assert(p->lines != NULL);
  }
  if (i < p->count)
  {
    size_t j;

    for (j = p->count ; j > i ; j--) /* Shift */
      p->lines[j] = p->lines[j-1];
  }
  else
  {
    size_t j;

    for (j = p->count ; j < i ; j++) /* Fill with "" */
      p->lines[j] = mystrdup("");;
  }
  p->lines[i] = mystrdup(IFNULL0(s,""));
  p->count = max;
}


void
plan_set(const char *s, size_t i, struct plan * p)
{
  if (i >= p->count)
    plan_insert(s, i, p);
  else
  {
    free(p->lines[i]);
    p->lines[i] = mystrdup(IFNULL0(s,""));
  }
}

void
plan_add(const char *s, struct plan * p)
{
  plan_insert(s, p->count, p);
}

void
plan_rem(size_t i, struct plan * p)
{
  if (i < p->count && p->count > 0)
  {
    size_t j;

    free(p->lines[i]);
    for (j = i ; j < p->count-1 ; j++) /* Shift */
      p->lines[j] = p->lines[j+1];
    p->count -= 1;
    p->lines[p->count] = NULL;
  }
}


void plan_start(struct plan * p)
{
  if (p)
    p->iter = 0;
}


int plan_next(char **lp, struct plan * p)
{
  if (p)
  {
    if (p->iter < p->count)
    {
      if (lp)
	*lp = p->lines[p->iter];
      p->iter += 1;
      return 1;
    }
    p->iter = 0;
  }
  return 0;
}


void plan_clear(struct plan * p)
{
  if (p && p->lines)
  {
    size_t i = p->count;

    while (i--)
    {
      free(p->lines[i]);
      p->lines[i] = NULL;
    }
    p->count = 0;
  }
}


void plan_free(struct plan * p)
{
  if (p)
  {
    if (p->lines)
    {
      plan_clear(p);
      free(p->lines);
      p->lines = NULL;
    }
    free(p);
  }
}
