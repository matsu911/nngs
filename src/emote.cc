// -*-C++-*-
//   
// emote.cc -- look up strings in config file for formulating messages
// to clients
//
// Copyright 1995 J. Alan Eldridge.	
//
// modification history
// ==================================================
// 1995/01/13 alane@wozzle: created.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#include "emote.h"

// although this is a C++ source file, the enclosed functions have to
// be callable from C, hence the 'extern "C"' stuff. <sigh>

// static data

static const char *PATHNAME = "./emotestr";

/* get lines from a config file - the following rules apply

  1. the two-character sequence \# is treated as a literal #.
  2. otherwise, all characters from a # (inclusive) to a newline
     (exclusive) are treated as a comment.
  3. otherwise, a \ immediately followed by a newline is a 
     line-continuation, and is simply ignored.
  4. all other characters are treated literally.
  5a.if a line is read in its entirety, the last character
     in the buffer (before the zero) will be a newline.
  5b.a line that is too long to fit in the buffer will be
     retrieved with multiple calls.     
*/

/* ugets(fp,buf,size) -- get one logical line. may be
   an empty line. returns # of characters read, or EOF
   if end-of-file was reached on the first read.     */

static int ugets(FILE *fp, char *buf, int size)
{
  int c;        /*   current character from input            */
  int nc=0;     /*   # characters in current input line      */
  int skip=0;   /*   are we skipping to end-of-line?         */
  int nw=0;     /*   # of non-whitespace characters we got   */

  size--;   /*   leave room for zero byte      */

  while ((c=getc(fp)) != EOF) {
    /* skipping comments?                   */
    if (skip) {
      skip = c!='\n';
      if (!skip) {
	ungetc(c,fp);
      }
      continue;
    }

    /* start comment                        */
    if (c=='#') {
      skip=1; 
      continue;
    }
    
    /* line continuation with \-NL?
       or an escaped '#' character?         */

    if (c=='\\') {
      int c2=getc(fp);
      
      if (c2=='\n') {
	continue;
      } else if (c2=='#') {
	c=c2;
      } else {
	ungetc(c2,fp);
      }
    }

    /* ok, it's "ordinary" so add it        */
    if (nc < size) {
      nc++;
      *buf++=c; 
      if (!isspace(c)) {
	nw++;
      }
    } else {
      ungetc(c,fp);
      break;
    }

    /* if we hit newline we're done         */
    if (c=='\n') {
      break;
    }
  }

  /* terminate string with zero byte           */
  *buf=0;

  /* make sure we got some non-whitespace ...
     otherwise just call it an empty line      */
  if (!nw)
    nc=0;

  /* if got chars, return how many. else, if
     we got EOF, return that. otherwise, we
     just got an empty line, so return zero.   */
  return nc ? nc : c==EOF ? EOF : 0;
}

/**************************************************

  utabgets(fp,buf,size) -- get next non-blank
  logical line (may be several physical lines
  joined together with \-NL) from file.

  loop while ugets(fp,buf) returns zero.
  at this point we either (1) have some text
  or (2) have hit EOF.

***************************************************/

int utabgets(FILE *fp, char *buf, int size)
{
  int nc;

  while ((nc=ugets(fp,buf,size)) == 0) {
    /* empty loop */
  }
  return nc;
}

// helper to find a keyword

static const char *findkey(const char *psz)
{
#if 0
  class File {
  private:
    FILE *m_pf;
  public:
    File(const char *pszName, const char *pszMode = "r") {
      if (pszName) {
	m_pf = fopen(pszName, pszMode);
      } else {
	m_pf = 0;
      }
    }
    ~File() {
      if (m_pf) {
	fclose(m_pf);
      }
    }
    operator FILE *() const {
      return m_pf;
    }
  };

  File f(PATHNAME); // all that was just 'cause I don't want to worry about
                    // closing the file when I'm done with it; this way, the
                    // destructor for File does it any way I leave the 
                    // function... (let the C++ compiler do as much work for
                    // you as it can)

  FILE *pf = f;

  if (!pf) {
    return 0;
  }

  static char szLine[ 2000 ]; /* static 'cause I'm gonna return a
				 ptr into it */

  while (utabgets(pf, szLine, sizeof(szLine)) > 0) {
    char szKW[ 100 ];

    if (sscanf(szLine, "%s", szKW) == 1 && !strcmp(szKW, psz)) {
      // now do this the hard way ...
      // first, skip any leading whitespace
      char *pc = szLine;
      while (isspace(*pc)) {
	pc++;
      }
      // now skip the keyword
      pc += strlen(psz);
      // now skip any more whitespace (there's gotta be at least one char
      // of white because we're guaranteed that by the sscanf I did)
      while (*pc && isspace(*pc)) {
	pc++;
      }
      if (*pc) {
	// now we're at the start of the string we want to return, so
	// we just zap the newline and we're outa here
	char *pcNL = strchr(pc, '\n');
	if (pcNL) {
	  *pcNL = 0;
	}
	return pc;
      }
    }      
  }
#endif
  return 0;
}

extern "C" void EmoteInit(const char *pszPathName)
{
  return;
  PATHNAME = strdup(pszPathName);
}

extern "C" int EmoteLook(const char *pszKey)
{
  return 0;
  return findkey(pszKey) != 0;
}

class EmoteArgs {
private:
  char m_szArgs[ 2000 ];
  int m_pos;
  int skipws() {
    while (m_szArgs[ m_pos ] && isspace(m_szArgs[ m_pos ])) {
      m_pos++;
    }
    return m_szArgs[ m_pos ];
  }
public:
  EmoteArgs(const char *psz) {
    if (!psz) {
      psz = "";
    }
    strncpy(m_szArgs, psz, 2000); m_szArgs[ 1999 ] = 0;
    m_pos = 0; 
  }

  const char *next() {
    int c = skipws();
    if (!c) {
      return 0;
    }
    if (c == '\'' || c == '\"') {
      m_pos++;
      char *psz = m_szArgs + m_pos;
      while (m_szArgs[ m_pos ] && m_szArgs[ m_pos ] != c) {
	m_pos++;
      }
      if (m_szArgs[ m_pos ]) {
	m_szArgs[ m_pos++ ] = 0;
      }
      return psz;
    } else {
      char *psz = m_szArgs + m_pos;
      while (m_szArgs[ m_pos ] && !isspace(m_szArgs[ m_pos ])) {
	m_pos++;
      }
      if (m_szArgs[ m_pos ]) {
	m_szArgs[ m_pos++ ] = 0;
      }
      return psz;
    }
  }
};

extern "C" const char *EmoteMkStr(const char *pszKey, const char *pszUsr,
				  const char *pszArgs, int fClient)
{
#if 0
  static char szStr[ 2000 ]; // static 'cause I'm gonna return it

  const char *pszFmtStr = findkey(pszKey);

  if (!pszFmtStr) {
    return NULL; // somebody fucked up, and it isn't me
  }

  EmoteArgs args(pszArgs);

  char *pc = szStr;
  int len = 0;
  const int MAX = sizeof(szStr) - 1;

  char c;

  while ((c = *pszFmtStr++) != 0) {
    if (c == '\\') {
      c = *pszFmtStr++;
      if (!c) {
	break;
      }
      if (c == 'n') {
	if (len < MAX) {
	  len++;
	  *pc++ = '\n';
	  if (fClient) {
	    if ((len += 4) < MAX) {
	      strcpy(pc, "500 ");
	      pc += 4;
	    } else {
	      break;
	    }
	  }
	} else {
	  break;
	}
      } else {
	if (len < MAX) {
	  len++;
	  *pc++ = c;
	} else {
	  break;
	}
      }
    } else if (c == '%') {
      c = *pszFmtStr++;
      if (!c) {
	break;
      } else if (c == 'U' || c == 's') {
	const char *pszArg;

	if (c == 'U') {
	  pszArg = pszUsr;
	} else {
	  pszArg = args.next();
	}

	if (!pszArg) {
	  continue;
	}

	int argLen = strlen(pszArg);

	if (len + argLen < MAX) {
	  for (int ch = *pszArg; ch; ch = *++pszArg) {
	    if (ch == '\\') {
	      ch = *++pszArg;
	      if (!ch) {
		break;
	      }
	      if (ch == 'n') {
		strcpy(pc++, "\n");
		argLen--;
		if (fClient) {
		  if ((argLen += 4) + len >= MAX) {
		    break; // overflow
		  }
		  strcpy(pc, "500 ");
		  pc += 4;
		}
	      } else {
		argLen--;
		*pc++ = ch;
	      }
	    } else {
	      *pc++ = ch;
	    }
	  }
	  len += argLen;
	} else {
	  break;
	}
      } else if (len < MAX) {
	*pc++ = c; len++;
      } else {
	break;
      }
    } else if (len < MAX) {
      *pc++ = c; len++;
    } else {
      break;
    }
  }

  *pc = 0;
  return szStr;
#else
  return NULL;
#endif
}

/*#define TESTMAIN*/
#if TESTMAIN

int main(int, const char **)
{
  EmoteInit("./emotestr");

  const char *psz = EmoteMkStr("wanker", "alane", "'Newt Gingrich' pathetic", 1);

  if (psz) {
    puts(psz);
    psz = EmoteMkStr("wanker", "alane", "'Newt Gingrich'", 0);
    puts(psz);
  } else {
    puts("No wanker found. Odd, that.");
  }

  return 0;
}

#endif // TESTMAIN

