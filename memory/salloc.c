#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_PEM_MALLOC
#ifndef __CYGWIN__

/*
  Safe memory allocation package.
  It is enabled by defining SALLOC and including salloc.h (done in types.h).
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char sm_log_buffer[100];
static size_t sm_count = 0, sm_memory = 0;

int sm_logging = 0;

typedef struct alloc_info
{
   struct alloc_info *nxt, *prv;
   long pc0, pc;
   int status;
   int id;
   size_t len;
} Info;

static Info anchor = { NULL, NULL, 0, 0, 0, 0, 0 };

#define error0(fmt)	\
   do { \
     fprintf(stderr, fmt); \
     putc('\n', stderr); \
     abort(); \
   } while (0)

#define error1(fmt, arg)	\
   do { \
     fprintf(stderr, fmt, arg); \
     putc('\n', stderr); \
     abort(); \
   } while (0)

#define DO_LOG(ARG) \
   do { \
     sprintf ARG; \
     if (sm_logging) \
       fprintf(stderr, "\n %d-%d %s\n", \
	       sm_count, sm_memory, sm_log_buffer); \
   } while (0)

#define LOG1(fmt,a1)		DO_LOG((sm_log_buffer,fmt,a1))
#define LOG2(fmt,a1,a2)		DO_LOG((sm_log_buffer,fmt,a1,a2))
#define LOG3(fmt,a1,a2,a3)	DO_LOG((sm_log_buffer,fmt,a1,a2,a3))
#define LOG4(fmt,a1,a2,a3,a4)	DO_LOG((sm_log_buffer,fmt,a1,a2,a3,a4))

/*
  Machine-dependent macros:
  OFFSET is sizeof(Info), suitably aligned
  PC(arg), when called with the name of the first arg to a function,
   gives the PC of the call.
*/

#define OFFSET		(((sizeof(Info) - 1) / 8 + 1) * 8)

#define PC(arg)		(*(((long *)(&arg)) - 1))

#define TAIL		1

#define ALLOC_OK	51
#define ALLOC_FREE	52
#define ALLOC_ID	4719
#define ALLOC_TAIL	113	/* A positive char */
#define ALLOC_TRASH	179

void
Salloc_init()
{
  sm_count = 0;
  sm_memory = 0;
  anchor.nxt = NULL;
}

static void
check_info(Info *ip)
{
  char *p = OFFSET + (char *)ip;

  if (ip->id != ALLOC_ID)
    error0("not allocated with the safe package");
  if (ip->len <= 0)
    error0("length trashed");
  if (p[ip->len] != ALLOC_TAIL)
    error0("end of block has been overwritten");
}

static void
check_info_chain(void)
{
  Info *ip;

  if (sm_logging < 0)
    return;

  for (ip = anchor.nxt ; ip != NULL ; ip = ip->nxt)
  {
    if (sm_logging > 1)
    {
      fprintf(stderr, "%#lx: ", (unsigned long)ip);
      if (ip->pc != ip->pc0)
	fprintf(stderr, "pc0 %#lx ", ip->pc0);
      fprintf(stderr, "pc %#lx ", ip->pc);
      fprintf(stderr, "len %d ", ip->len);
      fprintf(stderr, "\n");
    }
    check_info(ip);
  }
}

static Info *
ptr_to_info(char *p)
{
  Info *ip = (Info *)(p - OFFSET);

  if (p == 0)
    error0("NULL pointer");
  check_info(ip);
  return ip;
}

void *
Malloc(size_t n)
{
  char *p;
  Info *ip;

  check_info_chain();

  sm_count += 1;
  sm_memory += n;

  if (n <= 0)
    error1("malloc: attempt to allocate %d bytes", n);
  p = malloc(n + OFFSET + TAIL);
  if (p == 0)
#if 0
    error1("malloc: couldn't allocate %d bytes", n);
#else
  return NULL;
#endif

  ip = (Info *)p;
  ip->id = ALLOC_ID;
  ip->len = n;
  ip->status = ALLOC_OK;

  ip->pc0 = ip->pc = PC(n);

  ip->nxt = anchor.nxt;
  ip->prv = &anchor;
  if (ip->nxt)
  {
    if (ip->nxt->prv != &anchor)
      error0("info chain corrupted");
    ip->nxt->prv = ip;
  }
  anchor.nxt = ip;

  p += OFFSET;

  p[n] = ALLOC_TAIL;

  LOG2("malloc(%d) -> %#lx", n, (unsigned long)p);

  return p;
}

char *
Calloc(size_t m, size_t size)
{
  char *p;
  Info *ip;
  int n = m * size;

  check_info_chain();

  sm_count += 1;
  sm_memory += n;

  if (n <= 0)
    error1("calloc: attempt to allocate %d bytes", n);
  p = malloc(n + OFFSET + TAIL);
  if (p == 0)
#if 0
    error1("calloc: couldn't allocate %d bytes", n);
#else
  return NULL;
#endif

  ip = (Info *)p;
  ip->id = ALLOC_ID;
  ip->len = n;
  ip->status = ALLOC_OK;

  ip->pc0 = ip->pc = PC(m);

  ip->nxt = anchor.nxt;
  ip->prv = &anchor;
  if (ip->nxt)
  {
    if (ip->nxt->prv != &anchor)
      error0("info chain corrupted");
    ip->nxt->prv = ip;
  }
  anchor.nxt = ip;

  p += OFFSET;

  memset(p, 0, (int)n);

  p[n] = ALLOC_TAIL;

  LOG3("calloc(%d,%d) -> %#lx", m, size, (unsigned long)p);

  return p;
}

void *
Realloc(void *p, size_t n)
{
  char *new_p;
  Info *ip;
  int old_n;

  if (p == NULL)
    return Malloc(n);

  check_info_chain();

  ip = ptr_to_info(p);

  if (n <= 0)
    error1("realloc: attempt to allocate %d bytes", n);
  new_p = realloc(ip, n + OFFSET + TAIL);
  if (new_p == 0)
#if 0
    error1("realloc: couldn't allocate %d bytes", n);
#else
  return NULL;
#endif
  ip = (Info *)new_p;
  old_n = ip->len;
  ip->len = n;

  ip->prv->nxt = ip;
  if (ip->nxt)
    ip->nxt->prv = ip;

  ip->pc = PC(p);

  sm_memory += (n - old_n);

  new_p += OFFSET;

  new_p[n] = ALLOC_TAIL;

  LOG4("realloc(%#lx,%d) -> %#lx [old length = %d]",
       (unsigned long)p, n, (unsigned long)new_p, old_n);

  return new_p;
}


void
Free(void *p)
{
  Info *ip;
  int old_n;

  check_info_chain();

  ip = ptr_to_info(p);

  if (ip->nxt)
    ip->nxt->prv = ip->prv;
  if (ip->prv)
    ip->prv->nxt = ip->nxt;

  old_n = ip->len;

  sm_count -= 1;
  sm_memory -= old_n;

  switch (ip->status)
  {
  case ALLOC_FREE:
    error0("free: already freed");
    break;
  case ALLOC_OK:
    ip->status = ALLOC_FREE;
    free(ip);
    memset(p, ALLOC_TRASH, old_n);
    break;
  default:
    error0("free: trashed status");
  }
  LOG2("free(%#lx) [length = %d]", (unsigned long)p, old_n);
}

void
Cfree(void *p, size_t m, size_t size)
{
  Info *ip;
  int old_n;

  check_info_chain();

  ip = ptr_to_info(p);

  if (ip->nxt)
    ip->nxt->prv = ip->prv;
  ip->prv->nxt = ip->nxt;

  old_n = ip->len;

  sm_count -= 1;
  sm_memory -= old_n;

  switch (ip->status)
  {
  case ALLOC_FREE:
    error0("cfree: already freed");
    break;
  case ALLOC_OK:
    ip->status = ALLOC_FREE;
    free(ip);
    break;
  default:
    error0("cfree: trashed status");
  }
  LOG2("cfree(%#lx) [length = %d]", (unsigned long)p, old_n);
}

char *
Strdup(char *s)
{
  char *p = (char *)Malloc(strlen(s) + 1);

  if (p != NULL)
    strcpy(p, s);
  return p;
}

#endif /* CYGWIN */
#endif /* USE_PEM_MALLOC */
