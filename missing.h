#ifndef MISSING_H
#define MISSING_H 1
#include <stdio.h>

#if ( !HAVE_FILENO )
#ifndef fileno	/* Avoid trouble; (if) fileno() is a macro */
extern int fileno(FILE *fp);
#endif
#endif

#if ( !HAVE_POPEN )
extern FILE *popen(const char *, const char*);
#endif
#if ( !HAVE_PCLOSE )
extern int pclose(FILE *fp);
#endif

#if ( !HAVE_VSNPRINTF )
#define HAVE_SNPRINTF 0
#include <stdarg.h>
extern int vsnprintf(char *buff, size_t s, const char *format, va_list ap);
#endif

#if ( !HAVE_SNPRINTF )
extern int snprintf(char *buff, size_t s, const char *format, ...);
#endif

#if ( !HAVE_FTRUNCATE )
#define off_t size_t
extern int ftruncate(int fd, off_t pos);
#undef off_t 
#endif

#endif /* MISSING_H */
