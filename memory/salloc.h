#ifndef _SALLOC_H_
#define _SALLOC_H_

/* "Safe" malloc. Does some runtime checks in malloc/realloc/free,
** with a performance penalty of course.                          
** Turned on with -DSALLOC                                        
*/

#ifdef SALLOC

#define malloc	Malloc
#define free	Free
#define cfree	Cfree
#define calloc	Calloc
#define realloc	Realloc
#define strdup  Strdup

extern void *Malloc(size_t n);
extern char *Realloc(void *p, size_t n);
extern char *Calloc(size_t m, size_t size);
extern void Free(void *p);
extern void Cfree(void *p, size_t m, size_t size);
extern char *Strdup(const char *s);

#endif /* SALLOC */

#endif /* _SALLOC_H_ */
