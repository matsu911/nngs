/* alias.c
 ** Per-Erik Martin (pem@docs.uu.se) 1996-02-06
 **
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <assert.h>

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "alias.h"
#include "utils.h"

#define INITSIZE 8

struct aliaspair
{
  char *command, *alias;
};


struct alias
{
  struct aliaspair *aliases;
  size_t size, count, iter;
};

unsigned alias_count(struct alias * a)
{
  return a ? a->count : 0;
}

  struct alias *
alias_init(void)
{
  struct alias * a;

  a = malloc(sizeof *a);
  assert(a != NULL);
  a->size = a->count = 0;
  a->aliases = malloc(INITSIZE*sizeof *a->aliases);
  assert(a->aliases != NULL);
  a->size = INITSIZE;
  return a;
}

/*
 ** Finds an alias, returns 1 if found, 0 otherwise.
 ** If found, *ip is set to the location, otherwise *ip is the
 ** place we last looked at, which is where it should be if it
 ** was here.
 **
 ** This uses binary search, so the list must of course be sorted
 ** alphabetically.
 */
  static int
alias_index(size_t *ip, const char *cmd, struct alias * a)
{
  size_t left, right;

  if (!a) return 0;

  left = 0;
  right = a->count;
  while (left < right)
  {
    size_t i = (left+right)/2;
    int x = strcmp(cmd, a->aliases[i].command);

    if (!x) { *ip = i; return 1; }
    if (x < 0) right = i;
    else left = i+1;
  }
  *ip = left;			/* Not found, left is the insertion point. */
  return 0;
}


char * alias_lookup(const char *cmd, struct alias * a)
{
  size_t i;

  if (alias_index(&i, cmd, a))
    return a->aliases[i].alias;
  return NULL;
}


int alias_add(const char *cmd, const char *alias, struct alias * a)
{
  size_t i;

  assert(a);
  if (alias_index(&i, cmd, a))
  {				/* Found it, replace at i. */
    free(a->aliases[i].alias);
    a->aliases[i].alias =  mystrdup(alias);
    return 1;
  }

  /* Not found. Check if we need to resize before inserting. */
  if (a->count == a->size)
  {
    a->size *= 2;
    a->aliases = realloc(a->aliases, a->size*sizeof *a->aliases);
    assert(a->aliases != NULL);
  }

  if (i < a->count)
  {				/* Must shuffle the rest first. */
    size_t j;

    for (j = a->count ; j > i ; j--)
    {
      a->aliases[j].command = a->aliases[j-1].command;
      a->aliases[j].alias = a->aliases[j-1].alias;
    }
  }
  a->aliases[i].command = mystrdup(cmd);
  a->aliases[i].alias = mystrdup(alias);
  a->count += 1;
  return 0;
}

  int
alias_rem(const char *cmd, struct alias * a)
{
  size_t i;

  if (alias_index(&i, cmd, a))
  {
    size_t j;

    /* Zap it. */
    free(a->aliases[i].command);
    free(a->aliases[i].alias);
    a->aliases[i].command = a->aliases[i].alias = NULL;

    /* Shuffle the rest. */
    for (j = i ; j < a->count-1 ; j++)
    {
      a->aliases[j].command = a->aliases[j+1].command;
      a->aliases[j].alias = a->aliases[j+1].alias;
    }
    a->count -= 1;
    a->aliases[a->count].command = NULL;
    a->aliases[a->count].alias = NULL;
    return 1;
  }
  return 0;
}

  void
alias_start(struct alias * a)
{
  if (a)
    a->iter = 0;
}

  int
alias_next(char **cp, char **ap, struct alias * a)
{
  if (a)
  {
    if (a->iter < a->count)
    {
      if (cp)
        *cp = a->aliases[a->iter].command;
      if (ap)
        *ap = a->aliases[a->iter].alias;
      a->iter += 1;
      return 1;
    }
    a->iter = 0;
  }
  return 0;
}

  void
alias_free(struct alias * a)
{
  if (a)
  {
    if (a->aliases)
    {
      size_t i = a->count;

      while (i--)
      {
        if (a->aliases[i].command)
        {
          free(a->aliases[i].command);
          a->aliases[i].command = NULL;
        }
        if (a->aliases[i].alias)
        {
          free(a->aliases[i].alias);
          a->aliases[i].alias = NULL;
        }
      }
      free(a->aliases);
      a->aliases = NULL;
    }
    free(a);
  }
}
