/* Malloc routines, written by Per-Erik Martin, 1990, ANSIfied 1999.
**
** This implements the common malloc(3C) functions, malloc(), calloc(),
** realloc() and free().
** It does *not* implement any of the undocumented 'features' common in
** some other implementions of malloc.
** Two extra functions are available:
**
** char *top_malloc(size, bufsize)
**   Like malloc, but tries to put the block at the end of the used memory
**   space with 'bufsize' bytes of free space before it (may be zero).
**   Realloc will try to avoid moving this block downwards so it may grow
**   upwards freely, without moving.
**
** char *maybe_realloc(ptr, size)
**   Like realloc without actually reallocate the block. Useful for checking
**   "where will this block go if we do a realloc now?". Don't call any
**   other malloc functions between the maybe_realloc call and the 'true'
**   realloc call.
**
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_PEM_MALLOC
#ifndef __CYGWIN__

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "malloc.h"

extern void abort();

/* Configuration:
**
**  ALIGN_TYPE   is the C type with the hardest alignment restriction, usually
**               double.
**  NO_RELEASE   stops free and realloc from trying to release memory when
**               freed. This is usually defined under Unix since sbrk(-size)
**		 doesn't actually release any memory.
**  NO_SPLIT     stops malloc and realloc from trying to split to large user
**               blocks. If this is defined malloc will probably waste a lot
**	         more memory.
**  MALLOC_STATS Enables the malloc_stats function to the price of a small
**               overhead (counting calls, logging sizes etc).
**  MALLOC_DEBUG Enables a lot of sanity and safety checks. The overhead is
**               quite big.
**
** Pepe related:
**  PROFILING    Tells malloc that profiling is done (which may use brk/sbrk).
**  DEBUGGING    Tells malloc that debugging with cdb/xdb is done. (No
**               releasing of memory, since the debugging doesn't like that).
**  SALLOC       Tells malloc that it's used in conjunctions with the salloc
**               routines. (No releasing of memory, since salloc fills freed
**		 memory with zeros.)
*/
#ifndef ALIGN_TYPE
#define ALIGN_TYPE double
#endif
#ifndef NO_RELEASE
#define NO_RELEASE 0
#endif
#ifndef NO_SPLIT
#define NO_SPLIT 0
#endif
#ifndef MALLOC_STATS
#define MALLOC_STATS 0
#endif
#ifndef MALLOC_DEBUG
#define MALLOC_DEBUG 0
#endif

/* gcc v2.8.1 does a better job with this. */
/*#define REGISTER register*/
#define REGISTER

#define S_( Body ) do { Body } while (0)


#if MALLOC_DEBUG || defined(DEBUGGING)

#include <stdio.h>

#define USED_CHECK(Who, B) \
S_( \
  if (is_free((B)->size)) \
    fprintf(stderr, "(%s) Used block is tagged free: %x->%u\n", \
	    (Who), (B)->size, tail_size(B)); \
  if ((B)->size != tail_size(B)) \
    fprintf(stderr, \
	    "(%s) Head and tail sizes are different: %p->%u != %u\n", \
            (Who), (B), (B)->size, tail_size(B)); \
  )

#else /* not MALLOC_DEBUG || defined(DEBUGGING) */

#define USED_CHECK(Who, B)

#endif /* MALLOC_DEBUG || defined(DEBUGGING) */


#define MAX(A, B)  ((A) > (B) ? (A) : (B))

#define ROUND(X, Y)  ((X) % (Y) ? (X) + ((Y) - ((X) % (Y))) : (X))
#define ALIGN(X, Y)  ((char *)ROUND((ulong)(X), (Y)))

/* The minimum size of blocks allocated; the maximum of a minimal free
   block and a minimal non-empty used block. */
#define MIN_BLOCK_SIZE \
  MAX( ROUND(sizeof(block_t) + sizeof(size_t), ALIGN_SIZE), \
       ROUND(ALIGN_SIZE + sizeof(word_t) + sizeof(size_t), ALIGN_SIZE) )


/*
**  Start of System Dependent section.
*/

/* Specify word_t and size_t for each architecture. They should be the same,
   the difference between them are mainly cosmetic (we use size_t when we
   mean a size, word_t otherwise). */
typedef ulong word_t;

#ifndef WORD_BIT
#define WORD_BIT 32
#endif

#define SIZE_T_BITS WORD_BIT

/* ALIGN_TYPE is the type with the hardest alignment restriction; double
   should hold for most systems.
   ALIGN_SIZE is never less than a word size. It should be the sizeof an
   even number of words. */
#define ALIGN_AUX \
  (sizeof(struct { ALIGN_TYPE t; char c; }) - sizeof(ALIGN_TYPE))
#define ALIGN_SIZE \
  MAX( ALIGN_AUX, sizeof(word_t))


/* System dependent memory functions.
   Define these functions:
     int above_lower_bound(char *)
     int below_upper_bound(char *)
     char *get_mem(size_t)
     char *where_to_get_mem(void)
     int release_mem(char *, size_t)
     char *expand_mem(char *, size_t)
   Only *_bound(), get_mem() and where_to_get_mem() are mandatory,
   the rest may be declared as dummys, like this:
     #define release_mem(a, b) (0)
     #define expand_mem(a, b) (NULL)
*/

extern int end;

static char *Mem_top = NULL;

#ifdef PROFILING
static char *Mem_bot = NULL;
#define SET_BOT S_( if (Mem_bot == NULL) Mem_bot = (char *)sbrk(0); )
#else
#define Mem_bot ((char *)&end)
#define SET_BOT
#endif

#define above_lower_bound(P) ((char *)(P) > Mem_bot)
#define below_upper_bound(P) ((char *)(P) < Mem_top)

static uint log2(ulong x);

/* Return a pointer to a new memory block of size 'size'. */
static char *
get_mem(size_t size)
{
  SET_BOT;
  if (size > 0)
  {
    static char *brk_point = NULL;
    REGISTER char *p;

    if (Mem_top == NULL || Mem_top + size > brk_point)
    {
      REGISTER size_t sz = 4096;

      while (sz < size)
	sz <<= 1;
      p = (char *)sbrk((long)(sz));
      if (p == (char *)(-1))
	return NULL;
      brk_point = p + sz;
      if (Mem_top == NULL)
	Mem_top = p;
    }
    /* Mem_top != NULL && Mem_top + size <= brk_point */
    p = Mem_top;
    Mem_top += size;
    return ALIGN(p, ALIGN_SIZE);
  }
  return NULL;
}

/* Like get_mem() but doesn't actually allocate any memory, only returns
   a pointer to the memory we would allocate if we did. */
#define where_to_get_mem(Ignored_size) ALIGN(Mem_top, ALIGN_SIZE)

/* Try to release the area starting at 'p' with size 'size'.
   Returns 1 if it succeeds, 0 otherwise. */
#if NO_RELEASE || defined(DEBUGGING) || defined(SALLOC)
#define release_mem(p, size) (0)
#else

#define release_mem(p, size) \
  ((size) > 0 && (p)+(size) == Mem_top && try_release_mem(p, size))

static int
try_release_mem(char *p, size_t size)
{
  REGISTER char *np = (char *)sbrk((int)(-size));

  if (np != (char *)(-1))
  {
    if (np != p)
      (void)sbrk((int)size);	/* Undo. */
    else
    {
      Mem_top = p;
      return 1;
    }
  }
  return 0;
}

#endif

/* Try to get a new area of size 'nsize', adjacent to the area starting
   at 'p' of size 'osize'.
   Returns a pointer to the expansion block (that is, p+osize) on success,
   NULL otherwise. */
static char *
expand_mem(char *p, size_t osize, size_t nsize)
{
  if (osize > 0 && nsize > 0 && p+osize == Mem_top /* && p+osize == sbrk(0)*/)
    return get_mem(nsize);
  return NULL;
}

/*
**  End of System Dependent section.      
*/


/*
**  Truncated log2().
*/

/* 98% percent of the returned values are between 6 and 12. */
static uint
log2(ulong x)
{
  REGISTER uint y, z;

  z = x >> 6;
  if (z)
  {
    for (y = 6 ; z >>= 1 ; y += 1)
      ;
    return y;
  }
  z = x;
  for (y = 0 ; z >>= 1 ; y += 1)
    ;
  return y;
}


/*
**  Free table and lists.
*/

/* The block head.
   Since we always allocate words we know that the lower two bits are
   unused in the size word, so we use it for a tag telling if a block
   are used (1) or free (0).
   The next and prev pointers are only used for free blocks. In used
   blocks they may be used for user data.
   Each block ends with a copy of the size word.
*/
typedef struct block
{
  size_t size;
  struct block *next, *prev;
} block_t;

#define size_tag(Size)      ((Size) & 1)
#define size_size(Size)     ((size_t)((Size) & (~1)))
#define tag_size(Size, Tag) ((size_t)(size_size(Size) | (Tag)))
/* Use these: */
#define is_free(Size)       (size_tag(Size) == 0)
#define is_used(Size)       (size_tag(Size) == 1)
#define mark_free(Size)     tag_size(Size, 0)
#define mark_used(Size)     tag_size(Size, 1)
#define free_size(Size)     (Size)
#define used_size(Size)     size_size(Size)
#define tail_size(B) \
  (*((size_t *)((char *)(B) + size_size((B)->size - sizeof(size_t)))))
#define set_sizes(B, Size) \
  ((B)->size = (Size), tail_size(B) = (B)->size)

/* The Top_block is *not* guaranteed to be on the top all the time.
   We just avoid doing things to it that would risk moving it downwards. */
static block_t *Top_block = NULL;

#define FREE_TABLE_SIZE  SIZE_T_BITS

static block_t *free_table[FREE_TABLE_SIZE];

/* Maybe split the used block. */
#if NO_SPLIT
#define maybe_split(B, Rsize)
#else
#define maybe_split(B, Rsize) \
S_( \
  if (used_size((B)->size) - (Rsize) > (Rsize)) \
    split(B, Rsize); \
  )
#endif

/* Join the adjacent used blocks. */
#define join(B1, B2) \
  set_sizes((B1), mark_used(used_size((B1)->size) + used_size((B2)->size)))

/* Takes the user size 's' and returns the real block size.
   We need room for two size words, the user data and at least
   ALIGN_SIZE so we can align the user pointer. */
static size_t
real_size(REGISTER size_t s)
{
  s = ROUND(ALIGN_SIZE + s + sizeof(size_t), ALIGN_SIZE);
  if (s < MIN_BLOCK_SIZE)
    s = MIN_BLOCK_SIZE;
  return s;
}

/* Returns the (possibly aligned) user pointer into the block 'B'. */
#define user_pointer(B) ((char *)(B) + ALIGN_SIZE)

/* 'P' is an user pointer. Returns the real block pointer. */
#define block_pointer(P) ((block_t *)((P) - ALIGN_SIZE))

/* Returns pointer to a free block. */
static block_t *
find_free_block(REGISTER size_t rsize)
{
  REGISTER uint i;
  REGISTER block_t *b;

  for (i = log2(rsize) ; i < FREE_TABLE_SIZE ; i += 1)
    for (b = free_table[i] ; b ; b = b->next)
      if (rsize <= free_size(b->size))
	return b;
  return NULL;
}

/* Find a free block for realloc. Part of the heuristics of realloc() is this
   function:
   'b' is the old (used) block and 'rsize' is the new (requested) size; 'rsize'
   must be larger than b->size.
   First we check if there is smaller block adjacent at the upper bound of 'b',
   or at the lower bound.
   If not we try to find a rsized block which may, or may not, be
   adjacent to 'b'.
   On return we set '*bp' to the found block (or NULL) and returns a code
   telling what type of block we found. */

/* Return codes for find_realloc_block() */
#define GOT_NOTHING 0
#define GOT_UPPER   1
#define GOT_LOWER   2
#define GOT_ANOTHER 3

static int
find_realloc_block(REGISTER block_t *b, size_t rsize, REGISTER block_t **bp)
{
  REGISTER block_t *b1;
  REGISTER size_t s, *s1;

  s = rsize - used_size(b->size); /* Size needed of adjacent block. */
  b1 = (block_t *)((char *)b + used_size(b->size));
  if (below_upper_bound(b1) && is_free(b1->size) && free_size(b1->size) >= s)
  {				/* Next block is free. */
    *bp = b1;
    return GOT_UPPER;
  }
  s1 = (size_t *)((char *)b - sizeof(size_t));
  if (above_lower_bound(s1) && is_free(*s1) && free_size(*s1) >= s)
  {				/* Previous block is free. */
    *bp = (block_t *)((char *)b - free_size(*s1));
    return GOT_LOWER;
  }
  if ((*bp = find_free_block(rsize)))
    return GOT_ANOTHER;
  return GOT_NOTHING;
}

/* 'B' points to the actual block to be initialized.*/
#define init_block(B, S) set_sizes((B), mark_used(S))

/* 'b' points to the block to be unfreed. */
static void
set_unfree(REGISTER block_t *b)
{
  REGISTER uint i = log2(free_size(b->size));

  if (b->next)			/* Unlink block. */
    b->next->prev = b->prev;
  if (b->prev)
    b->prev->next = b->next;
  if (free_table[i] == b)
    free_table[i] = b->next;	/* Unlink first block. */
  init_block(b, free_size(b->size));
}

/* 'b' is a pointer to the block to be freed. */
static void
set_free(REGISTER block_t *b)
{
  REGISTER size_t size = used_size(b->size);

  if (!release_mem((char *)b, size))
  {
    REGISTER block_t *b1;
    REGISTER size_t *s1;
    REGISTER uint i;

    b1 = (block_t *)((char *)b + size);
    if (below_upper_bound(b1) && is_free(b1->size))
    {			/* Unfree and join the upper block. */
      size += free_size(b1->size);
      set_unfree(b1);
    }
    s1 = (size_t *)((char *)b - sizeof(size_t));
    if (above_lower_bound(s1) && is_free(*s1))
    {			/* Join with lower block. */
      size += free_size(*s1);
      b1 = (block_t *)((char *)b - free_size(*s1));
      set_unfree(b1);
      b = b1;
    }
    set_sizes(b, mark_free(size)); /* Update the block's sizes. */
    i = log2(size);
    b->next = free_table[i];	/* Link in first in chain. */
    b->prev = NULL;
    free_table[i] = b;
    if (b->next)
      b->next->prev = b;
  }
}

/* Splits the used block b into two halves. */
static void
split(REGISTER block_t *b, REGISTER size_t rsize)
{
  REGISTER block_t *f = (block_t *)((char *)b + rsize);

  set_sizes(f, mark_used(used_size(b->size) - rsize)); /* Init. */
  set_sizes(b, mark_used(rsize));
  set_free(f);
}


/*
**  Malloc statistics.
*/

#if MALLOC_STATS

static ulong
  Malloc_count = 0,
  Realloc_count = 0,
  Free_count = 0,
  Calloc_count = 0,
  Allocated = 0;

static struct malloc_stats malls;
static struct { unsigned long size, count, sum; } malltab[FREE_TABLE_SIZE];

struct malloc_stats *
malloc_stats(void)
{
  REGISTER uint i;

  malls.mallc = Malloc_count;
  malls.reallc = Realloc_count;
  malls.callc = Calloc_count;
  malls.freec = Free_count;
  malls.bot = (char *)&end;
  malls.stop = where_to_get_mem(0L);
  malls.top = Mem_top;
  malls.top_block = (char *)Top_block;
  malls.allocated = Allocated;
  malls.table_size = FREE_TABLE_SIZE;
  malls.table = (struct mstab *)malltab;
  for (i = 0 ; i < FREE_TABLE_SIZE ; i += 1)
    /*    if (free_table[i]) */
  {
    REGISTER block_t *b;
    REGISTER size_t blocks = 0, space = 0;

    for (b = free_table[i] ; b ; b = b->next)
    {
      blocks += 1;
      space += free_size(b->size);
    }
    malltab[i].size = (1L) << i;
    malltab[i].count = blocks;
    malltab[i].sum = space;
  }
  return &malls;
}

#if DEBUGGING
void
print_malloc_stats(void)
{
  struct malloc_stats *s = malloc_stats();
  int i;
  unsigned long totsize;

  fprintf(stderr, "Bot = %p, STop = %p, Top = %p\n",
	  s->bot, s->stop, s->top);
  fprintf(stderr, "All = %lu, Top_block = %p\n",
	  s->allocated, s->top_block);
  for (i = 0, totsize = 0; i < s->table_size; i++)
  {
    if (s->table[i].count)
      fprintf(stderr, "%d: %ld min, %ld blks, %ld tot\n",
	      i, s->table[i].size, s->table[i].count, s->table[i].sum);
    totsize += s->table[i].sum;
  }
  fprintf(stderr, "  Total free: %ld\n", totsize);
}
#endif

#define MALLOC_CALL  (Malloc_count += 1)
#define REALLOC_CALL (Realloc_count += 1)
#define FREE_CALL    (Free_count += 1)
#define CALLOC_CALL  (Malloc_count -= 1, Calloc_count += 1)
#define ALLOCATED(S) (Allocated += (S))
#define FREED(S)     (Allocated -= (S))

#else /* no MALLOC_STATS */

#define MALLOC_CALL
#define REALLOC_CALL
#define FREE_CALL
#define CALLOC_CALL
#define ALLOCATED(S)
#define FREED(S)

#endif /* MALLOC_STATS */


/*
**  Standard user routines.
*/

void *
malloc(size_t size)
{
  REGISTER block_t *b;
  REGISTER size_t rsize;

  /* This is left unspecified by the standard. We could let malloc always
     return a valid pointer, but returning NULL is more consistent with
     the behavior of realloc (which calls malloc when the pointer argument
     is NULL). */
  if (size == 0)
    return NULL;

  MALLOC_CALL;
  rsize = real_size(size);
  if ((b = find_free_block(rsize)) != NULL)
  {
    set_unfree(b);
    maybe_split(b, rsize);
    ALLOCATED(used_size(b->size));
    return user_pointer(b);
  }
  if ((b = (block_t *)get_mem(rsize)) != NULL)
  {
    init_block(b, rsize);
    ALLOCATED(rsize);
    return user_pointer(b);
  }
  return NULL;
}

/* Used by maybe_realloc(). Pointless as user function. */
static void *
maybe_malloc(size_t size)
{
  REGISTER block_t *b;
  REGISTER size_t rsize;

  if (size == 0)
    return NULL;

  rsize = real_size(size);
  if ((b = find_free_block(rsize)) != NULL)
    return user_pointer(b);
  if ((b = (block_t *)where_to_get_mem(rsize)) != NULL)
    return user_pointer(b);
  return NULL;
}


/* Like malloc() but always gets new memory (at the top).
   If the bufsize is greater than zero, a free block of that size is
   allocated first to make room for possible future mallocs. */
void *
top_malloc(size_t size, size_t bufsize)
{
  REGISTER block_t *b;
  REGISTER size_t rsize = real_size(size);
  REGISTER size_t rbsiz = (bufsize ? real_size(bufsize) : 0);

  MALLOC_CALL;
  /* (Maybe) Get a free buffer. */
  if (rbsiz && (b = (block_t *)get_mem(rbsiz)) != NULL)
  {
    init_block(b, rbsiz);
    set_free(b);
  }
  /* Get the real block. */
  if ((b = (block_t *)get_mem(rsize)) != NULL)
  {
    init_block(b, rsize);
    Top_block = b;
    ALLOCATED(rsize);
    return user_pointer(b);
  }
  return NULL;
}

void *
realloc(void *pp, size_t size)
{
  char *p = (char *)pp;
  block_t *b1;
  REGISTER char *p1;
  REGISTER block_t *b;
  REGISTER size_t rsize;

  /* Some ANSI behavior. */
  if (p == NULL)
    return malloc(size);
  if (size == 0)
  {
    extern void free();

    free(p);
    return NULL;
  }

  b = block_pointer(p);
  rsize = real_size(size);
  USED_CHECK("realloc", b);

  /* The "undefined" behavior. Abort if something is corrupt. */
  if (is_free(b->size) || b->size != tail_size(b))
    abort();

  REALLOC_CALL;
  FREED(used_size(b->size));
  if (used_size(b->size) >= rsize)
  {				/* Fits in the old block. */
    if (b != Top_block)
      maybe_split(b, rsize);
    ALLOCATED(used_size(b->size));
    return p;
  }
  switch (find_realloc_block(b, rsize, &b1))
  {
  case GOT_UPPER:
    set_unfree(b1);
    join(b, b1);
    maybe_split(b, rsize);
    ALLOCATED(used_size(b->size));
    return p;
  case GOT_LOWER:
    if (b == Top_block)
      break;			/* Don't move the top block downwards. */
    set_unfree(b1);
    join(b1, b);
    p1 = user_pointer(b1);
    /* Overlapping move! */
    memcpy(p1, p, used_size(b->size) - (p - (char *)b) - sizeof(size_t));
    maybe_split(b1, rsize);
    ALLOCATED(used_size(b1->size));
    return p1;
  case GOT_ANOTHER:
    /* Found a new block, move is necessary. */
    if (b == Top_block)
      break;			/* Don't move the top block. */
    set_unfree(b1);
    maybe_split(b1, rsize);
    p1 = user_pointer(b1);
    memcpy(p1, p, used_size(b->size) - (p - (char *)b) - sizeof(size_t));
    set_free(b);
    ALLOCATED(used_size(b1->size));
    return p1;
  }
  {
    REGISTER size_t tmp = rsize - used_size(b->size);

    if ((b1 = (block_t *)expand_mem((char *)b,
				    used_size(b->size),
				    tmp)))
    {				/* Allocated adjacent memory. */
      init_block(b1, tmp);
      join(b, b1);
      ALLOCATED(used_size(b->size));
      return p;
    }
  }
  /* Allocated a new block, move is necessary. */
  if ((b1 = (block_t *)get_mem(rsize)))
  {
    init_block(b1, rsize);
    p1 = user_pointer(b1);
    memcpy(p1, p, used_size(b->size) - (p - (char *)b) - sizeof(size_t));
    set_free(b);
    if (b == Top_block)
      Top_block = b1;
    ALLOCATED(rsize);
    return p1;
  }
  return NULL;
}

void *
maybe_realloc(void *pp, size_t size)
{
  char *p = (char *)pp;
  block_t *b1;
  REGISTER block_t *b;
  REGISTER size_t rsize;

  /* Some ANSI behavior. */
  if (p == NULL)
    return maybe_malloc(size);
  if (size == 0)
    return NULL;	/* The real realloc frees the pointer here. */

  b = block_pointer(p);
  rsize = real_size(size);
  USED_CHECK("maybe_realloc", b);

  /* The "undefined" behavior. Abort if something is corrupt. */
  if (is_free(b->size) || b->size != tail_size(b))
    abort();

  if (used_size(b->size) >= rsize)
    return p;			/* Will fit in the old block. */
  switch (find_realloc_block(b, rsize, &b1))
  {
  case GOT_UPPER:
    return p;
  case GOT_LOWER:
  case GOT_ANOTHER:
    if (b == Top_block)
      break;
    return user_pointer(b1);
  }
  if (where_to_get_mem(rsize - used_size(b->size)) ==
      (char *)b + used_size(b->size))
    return p;			/* Will allocate adjacent memory. */
  /* Will allocate a new block. */
  if ((b1 = (block_t *)where_to_get_mem(rsize)))
    return user_pointer(b1);
  return NULL;
}

void
free(void *pp)
{
  char *p = (char *)pp;
  block_t *b;

  /* ANSI standard behavior. */
  if (p == NULL)
    return;

  b = block_pointer(p);
  USED_CHECK("free", b);

  /* The "undefined" behavior. Abort if something is corrupt. */
  if (is_free(b->size) || b->size != tail_size(b))
    abort();

  FREE_CALL;
  FREED(used_size(b->size));
  if (b == Top_block)
    Top_block = NULL;
  set_free(b);
}

void *
calloc(size_t nelem, size_t elsize)
{
  char *p;
  size_t s = nelem * elsize;

  CALLOC_CALL;
  p = malloc(s);
  memset(p, 0, s);
  return p;
}

#endif /* CYGWIN */
#endif /* USE_PEM_MALLOC */
