#ifndef _MALLOC_H_
#define _MALLOC_H_

#ifdef MALLOC_STATS

struct malloc_stats
{
  unsigned long mallc, reallc, callc, freec; /* Call counts. */
  char *bot,			/* &end */
       *stop,			/* Return value from sbrk(0). */
       *top;			/* The internally recorded memory top. This */
				/* should normally be the same as break. */
  unsigned long allocated;	/* Actual allocated memory. */
  char *top_block;		/* The top_malloc block, if any. */
  int table_size;		/* The size of the following table. */
  struct mstab			/* The block size, count and sum table. */
  {
    unsigned long size,		/* Minimum size of blocks in this slot. */
                  count,	/* Number of blocks in this slot. */
                  sum;		/* Total size of the blocks of this size.  */
  } *table;
};

extern struct malloc_stats *malloc_stats(void);

#endif  /* MALLOC_STATS */

#endif /* _MALLOC_H */
