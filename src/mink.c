/*
    NNGS - The No Name Go Server
    Copyright (C) 1995 John Tromp (tromp@daisy.uwaterloo.ca/tromp@cwi.nl)

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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
/* int random(void); */
#endif

#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef USING_DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#include "nngsmain.h"
#define MINK_C 1
#include "mink.h"
#include "utils.h"

int my_snprintf(char *dst, size_t dstlen, const char *fmt, ...);
#define snprintf my_snprintf

#define MOVECOLOR(i)	(((i) & 1) ? MINK_BLACK : MINK_WHITE)
#define LASTCOLOR(g)	MOVECOLOR((g)->movenr)
#define OTHERCOL(c)	(MINK_BLACK + MINK_WHITE - (c))

/* the following macros are to be used only w.r.t. a game struct called g */
#define VDIFF(g)	((g)->width+1)		/* vertical index difference */
#define SIZE(g)		((g)->width * (g)->height)	/* board size */
#define ESIZE(g)	(VDIFF(g)*((g)->height + 2))	/* extra board size */
#define MAXWidth	25
#define MAXSize		((MAXWidth+1)*(MAXWidth+2))

static char BOARDCHARS[] = ".#O .xo-+><|";      /* territory at +4 offsets */

static const char *stars[] = {
"",				/* 0 */
"aa",				/* 1 */
"",				/* 2 */
"",				/* 3 */
"",				/* 4 */
"cc",				/* 5 */
"",				/* 6 */
"dd",				/* 7 */
"cccffcff",			/* 8 */
"ggcccggccegeecegee",		/* 9 */
"hhccchhc",			/* 10 */
"iiccciiccfiffcfiff",		/* 11 */
"iidddiid",			/* 12 */
"jjdddjjddgjggdgjgg",		/* 13 */
"kkdddkkd",			/* 14 */
"lldddllddhlhhdhlhh",		/* 15 */
"mmdddmmd",			/* 16 */
"nndddnnddiniidinii",		/* 17 */
"oodddood",			/* 18 */
"ppdddppddjpjjdjpjj",	/* 19 */
"",				/* 20 */
"",				/* 21 */
"",				/* 22 */
"",				/* 23 */
"",				/* 24 */
"",				/* 25 */
"",				/* 26 */
};

static void mink_startgame(struct minkgame *gp)
{
  int x,y;

  gp->movenr = gp->logid = 0;
  gp->hash = 0L;
  gp->caps[MINK_WHITE] = gp->caps[MINK_BLACK] = 0;
  for (x=0; x<ESIZE(gp); x++)
    gp->board[x] = MINK_EMPTY;
  for (y=0; y<ESIZE(gp); y+=VDIFF(gp))
    gp->board[y] = MINK_EDGE;
  for (x=1; x<VDIFF(gp); x++)
    gp->board[x] = gp->board[x+ESIZE(gp)-VDIFF(gp)] = MINK_EDGE;
}

typedef xulong xulongpair[2];

static xulongpair *Zob;

void mink_init(void)
{
  int i,j;

  Zob = malloc(MAXSize * sizeof *Zob);
  for (i=0; i<MAXSize; i++)
    for (j=0; j<2; j++)
      Zob[i][j] = (xulong)random() << (sizeof(xulong)-1)/2 
	        | (xulong)random();
}

struct minkgame *mink_initgame(int width, int height, int rules)
{
  struct minkgame *gp;
  unsigned cnt;

  gp = malloc(sizeof *gp);
  assert(gp != NULL);
  memset (gp , 0, sizeof *gp);

  gp->width = width;
  gp->height = height;
  gp->rules = rules;
  gp->handicap = 0;
#if WANT_MINKKOMI
  gp->komi = MINK_KOMI;
#endif

  cnt = ESIZE(gp) ;
  gp->board = malloc(cnt * sizeof *gp->board);
  assert(gp->board != NULL);
  memset (gp->board , 0, cnt* sizeof *gp->board);
  gp->mvsize = cnt/2;		/* need at least 1 for 0x0 board:) */

  gp->moves = malloc(gp->mvsize * sizeof *gp->moves);
  assert(gp->moves != NULL);
  memset (gp->moves , 0, gp->mvsize * sizeof *gp->moves);

  gp->moves[0].hash = 0L;

  cnt = ESIZE(gp) ;
  gp->uf = malloc(cnt * sizeof *gp->uf);
  assert(gp->uf != NULL);
  memset (gp->uf , 0, cnt * sizeof *gp->uf);

  gp->logsize = ESIZE(gp);
  gp->uflog = malloc(gp->logsize * sizeof *gp->uflog);
  assert(gp->uflog != NULL);
  memset (gp->uflog , 0, gp->logsize * sizeof *gp->uflog);
/*  gp->nocaps = 0; */
  mink_startgame(gp);
  return gp;
}

void mink_savegame(FILE *fp, struct minkgame *gp, struct mvinfo *mi, int nmvinfos)
{
  int i,j,k,n,p,x,y,max;
  const char *star;
  struct kibitz *kp;

  if (gp->rules == RULES_ING && (n = gp->handicap)) {
    fprintf(fp, "HA[%d]", n);
  }
  for (k=0,i=1; i<=gp->movenr; i++) {
    p = gp->moves[i].point;
    if (p < -1) {
      if (gp->rules == RULES_NET) {	/* should always be the case */
        n = gp->handicap;
        if (mi) {
          fprintf(fp, "HA[%d]AB", n);
          star = stars[gp->width];
          max = strlen(star) / 2;
          for (j=n; j; j--)
          {
            if (max == 9 && (n == 5 || n == 7) && j==1)
              star = stars[gp->width]+(2*9-2);
            x = *star++;
            y = *star++ - 'a' + 1;
            fprintf(fp, "[%c%c]", x, 'a' + gp->height - y);
          }
          fprintf(fp, "\n");
        } else {
          fprintf(fp,";B[z%c]", 'a' + n);
        }
      }
    } else {
      fprintf(fp,";%c[", "WB"[i&1]);
      if (p == MINK_PASS)
        fprintf(fp, "tt");
      else fprintf(fp,"%c%c", 'a'-1+ p%VDIFF(gp), 'a' + gp->height - p/VDIFF(gp));
      fprintf(fp,"]");
    }
    if (i < nmvinfos && (kp = mi[i].kibitz)) {
      fprintf(fp,"C[\n");
        for (; kp; kp = kp->next) {	/* [PEM]: Must quote brackets. */
	  char *s;
	  for (s = kp->mess ; *s ; s++) {
	    switch (*s) {
            case '[': case ']': fputc('\\', fp);
              /* FALLTRU */
            default:
	      fputc(*s, fp);
            }
	  }
	  fputc('\n', fp);
	}
        fputs("]\n", fp);
      k = 0;
    } else if (++k == 12) {
      fputc('\n', fp);
      k = 0;
    }
  }
  if (k)
    fprintf(fp,"\n");
}

int mink_loadgame(FILE *fp, struct minkgame *gp)	/* return player_to_move if OK */
{
  char color, xx, yy;

  while (fscanf(fp, "; %c [ %c %c ] ", &color, &xx, &yy) == 3) {
    if (color != " WB"[LASTCOLOR(gp)]) return 0;
    if (xx == 'z') mink_sethcap(gp, yy - 'a');
    else if (xx == 't' && yy == 't') mink_pass(gp);
    else if (mink_play(gp, mink_point(gp, xx - ('a'-1), gp->height - (yy - 'a')),0) == 0)
       return 0;
  }
  return gp->movenr & 1 ? MINK_WHITE: MINK_BLACK ;
}

int mink_loadpos(FILE *fp, struct minkgame *gp)	/* return player_to_move if OK */
{
  char color, xx, yy;

  fscanf(fp, " ( ; Game[1]");
  while (fscanf(fp, " Add%c", &color) == 1) {
    while (fscanf(fp, " [ %c %c ] ", &xx, &yy) == 2) {
      if (color != " WB"[LASTCOLOR(gp)]) mink_pass(gp);
      if (mink_play(gp, mink_point(gp, xx - ('a'-1), gp->height - (yy - 'a')),0) == 0)
        return 0;
    }
  }
  return gp->movenr & 1 ? MINK_WHITE: MINK_BLACK ;
}

xulong mink_gethash(struct minkgame *gp)
{
  return gp->hash;
}

#if WANT_MINKKOMI
void mink_setkomi(struct minkgame *gp, float k)	/* set the komi */
{
  gp->komi = k;
}

float mink_getkomi(struct minkgame *gp)		/* set the komi */
{
  return gp->komi;
}
#endif

int mink_movenum(struct minkgame *gp)	/* return move number (#moves played in game) */
{
  return gp->movenr;
}

void freeminkgame(struct minkgame *gp)
{
  free(gp->uflog);
  free(gp->uf);
  free(gp->moves);
  free(gp->board);
  free(gp);
}
/*
void setnocaps(struct minkgame *gp, int value)
{
  gp->nocaps = value;
}
*/
int mink_sethcap(struct minkgame *gp, int n)	/* returns 1 if succesful */
{
  int i,x,y,max;
  const char *star;

  if (gp->movenr || n==0)
    return 0;
  gp->handicap = n;
  if (gp->rules == RULES_NET) {
    star = stars[gp->width];
    max = strlen(star) / 2;
    if (n > max)
      return 0;
    for (i=n; i; i--)
    {
      if (max == 9 && (n == 5 || n == 7) && i==1)
        star = stars[gp->width]+(2*9-2);
      x = *star++ - 'a' + 1;
      y = *star++ - 'a' + 1;
      mink_play(gp,mink_point(gp,x,y),0);
      gp->movenr = 0;
    }
    gp->moves[gp->movenr = 1].point = -1-n;
  }
  return 1;
}


#if 0
static int mink_libs(struct minkgame *gp, int p)	/* find #liberty-edges of p's group */
{
  while ((p = gp->uf[p]) > 0) ;
  return -p;
}
#endif


static void growlog(struct minkgame *gp)
{
  if (gp->logid >= gp->logsize) {
    gp->logsize *= 2;
    gp->uflog = realloc(gp->uflog, gp->logsize * sizeof *gp->uflog);
    assert(gp->uflog != NULL);
  }
}


static void ufmark(struct minkgame *gp)		/* put a move-marker on union-find log */
{
  growlog(gp);
  gp->uflog[gp->logid++].index = 0;
}


static void ufset(struct minkgame *gp, int i, int v)	/* logged change to union-find structure */
{
  growlog(gp);
  gp->uflog[gp->logid].index = i;
  gp->uflog[gp->logid++].value = gp->uf[i];
  gp->uf[i] = v;
}

static void ufadd(struct minkgame *gp, int i, int v)	/* logged change to union-find structure */
{
  growlog(gp);
  gp->uflog[gp->logid].index = i;
  gp->uflog[gp->logid++].value = gp->uf[i];
  gp->uf[i] += v;
}

static void fill(struct minkgame *gp, int p, int c)	/* fill empty space from point p with color c */
{
  if (gp->board[p] == MINK_EMPTY) {
    gp->hash ^= Zob[p][c];
    gp->board[p] = c;
    gp->caps[c]--;
    fill(gp,p-VDIFF(gp),c);
    fill(gp,p-1,c);
    fill(gp,p+1,c);
    fill(gp,p+VDIFF(gp),c);
  }
}

static int capture(struct minkgame *gp, int p, int c)	/* return #stones captured */
{
  if (gp->board[p] == MINK_EMPTY || gp->board[p] == MINK_EDGE)
    return 0;
  if (gp->board[p] == c) {
    gp->hash ^= Zob[p][c];
    gp->board[p] = MINK_EMPTY;
    gp->caps[c]++;
    return 1 + capture(gp,p-VDIFF(gp),c) + capture(gp,p-1,c)
             + capture(gp,p+1,c) + capture(gp,p+VDIFF(gp),c);
  }
  for (; gp->uf[p] >0; p = gp->uf[p]) ;
  ufadd(gp,p,-1);		/* give extra liberty(edge) */
  return 0;
}

/* exam nb of color c stone; return whether this nb gets captured */
static int neighbour(struct minkgame *gp, int p, int c)
{
  int nc,nr;

  if ((nc = gp->board[p]) == MINK_EDGE)
    return 0;
  if (nc == MINK_EMPTY) {
    ufadd(gp,gp->root,-1);	/* extra liberty */
    gp->kostat = -1;
    return 0;
  }
  for (nr=p; gp->uf[nr] > 0; nr = gp->uf[nr]) ;	/* find neighbour root */
  if (nc == c) {
    if (nr == gp->root)			/* same group; discount liberty */
      ufadd(gp,nr,1);
    else {
      nc = gp->uf[gp->root] + gp->uf[nr] + 1;	/* friendly group; share libs */
      if (gp->uf[gp->root] < gp->uf[nr]) {		/* orig. group has more libs */
        ufset(gp,gp->root,nc);
        ufset(gp,nr,gp->root);
      } else {
        ufset(gp,nr,nc);
        ufset(gp,gp->root,nr);
        gp->root = nr;
      }
    }
    gp->kostat = -1;
    return 0;
  }
  if (gp->uf[nr] == -1) {
    if (capture(gp,p,nc)==1 && !gp->kostat)
      gp->kostat = p;
    else gp->kostat = -1;
    return 1;
  }
  ufadd(gp,nr,1);
  return 0;
}

int mink_back(struct minkgame *gp)	/* return 1 on succes */
{
  int p,c,i;
  struct minkmove *mv;

  if (gp->movenr == 0)
    return 0;
  c = LASTCOLOR(gp);
  mv = &(gp->moves[gp->movenr--]);
  p = mv->point;
  if (p < -1) {
    mink_startgame(gp);
    return 1;
  }
  if (p == MINK_PASS)
    return 1;
/*  if (gp->nocaps == 0) { */
    if (mv->self)
      fill(gp,p,c);
    else { 
      if (mv->up)
        fill(gp,p-VDIFF(gp),OTHERCOL(c));
      if (mv->left)
        fill(gp,p-1,OTHERCOL(c));
      if (mv->right)
        fill(gp,p+1,OTHERCOL(c));
      if (mv->down)
        fill(gp,p+VDIFF(gp),OTHERCOL(c));
    }
/*  } */
  for (; (i = gp->uflog[--gp->logid].index); gp->uf[i] = gp->uflog[gp->logid].value);
  gp->board[p] = MINK_EMPTY;
  gp->hash = (mv-1)->hash;
  return 1;
}

void mink_forward(struct minkgame *gp)
{
  int p;

  if ((p = gp->moves[gp->movenr+1].point) == MINK_PASS)
    gp->movenr++;
  else mink_play(gp,p,0);
}

static void growmoves(struct minkgame *gp)
{
  if (gp->movenr >= gp->mvsize - 1) {
    gp->mvsize *= 2;
    gp->moves = realloc(gp->moves, gp->mvsize * sizeof *gp->moves);
    assert(gp->moves != NULL);
  }
}

int mink_is_valid_move(struct minkgame *gp, char *s)		/* return point != 0 if s is go_move */
{
  char xx;
  int yy;

  if (strcmp(s, "pass") == 0)
    return 1;
  if (sscanf(s,"%c %d", &xx, &yy) < 2)
    return 0;
#if UPPERCASE
  xx = tolower(xx);
#endif
  if (xx < 'a' || xx  == 'i') return 0;
  if (xx > 'i') xx--;
  if ((xx -= ('a'-1)) > gp->width || yy < 1 || yy > gp->height) return 0;
  return mink_point(gp,xx,yy);
}

	/* convert coords to point */
int mink_point(struct minkgame *gp, int xx, int yy)
{
  return yy * VDIFF(gp) + xx;
}

	/* return whether current position repeats older one */
static int mink_superko(struct minkgame *gp)
{
  int *curboard,diff=1,i,j,n;

  for (i = gp->movenr; (i-=2) >= 0;) {
    if (gp->moves[i].hash == gp->hash) { 
#if HASHFAITH
      return 1;
#else
      curboard = malloc(ESIZE(gp) * sizeof *curboard);
      for (j=0; j<ESIZE(gp); j++)
        curboard[j] = gp->board[j];
      n = gp->movenr;
      do mink_back(gp); while (gp->movenr > i);
      for (j=0; j<ESIZE(gp); j++) {
        if ((diff = (curboard[j] != gp->board[j]))) break;
      }
      do mink_forward(gp); while (gp->movenr < n);
/* only works if forward doesn't check for superko:( */
      free(curboard);
      if (!diff) return 1;
#endif
    }
  }
  return 0;
}

int mink_play(struct minkgame *gp, int p, int ko)	/* return whether move is legal */
{
  int c;
  struct minkmove *mv;

  if (gp->board[p] != MINK_EMPTY || p == gp->moves[gp->movenr].ko)
    return 0;
  if (gp->rules == RULES_ING &&
     (gp->movenr&1) && gp->movenr < 2 * gp->handicap)
    return 0;			/* w should pass handicap-1 turns */
  growmoves(gp);
  mv = &(gp->moves[++gp->movenr]);
  gp->board[p] = c = LASTCOLOR(gp);
  gp->hash ^= Zob[p][c];
  mv->point = p;
  ufmark(gp);
  ufset(gp,gp->root=p,0);	/* zero liberties for the moment */
  gp->kostat = 0;
/*  if (gp->nocaps == 0) { */
    mv->up = neighbour(gp,p-VDIFF(gp),c);
    mv->left = neighbour(gp,p-1,c);
    mv->right = neighbour(gp,p+1,c);
    mv->down = neighbour(gp,p+VDIFF(gp),c);
    if ((mv->self = !gp->uf[gp->root])) {	/* suicide */
      capture(gp,p,c); 
      if (gp->rules == RULES_NET) {
        mink_back(gp);
        return 0;				/* forbidden:( */
      }
    }
/*  } */
  if (ko && mink_superko(gp)) {
    mink_back(gp);
    return 0;
  }
  mv->ko = gp->kostat;
  mv->hash = gp->hash;
  return 1;
}

int mink_pass(struct minkgame *gp)	/* if pass is i'th consecutive one, return i */
{
  int i;

  growmoves(gp);
  gp->moves[i = ++gp->movenr].point = MINK_PASS;
  gp->moves[gp->movenr].ko = 0;
  gp->moves[gp->movenr].hash = gp->moves[gp->movenr-1].hash;
  while (gp->moves[--i].point == MINK_PASS) ;
  return gp->movenr - i;
}


/* remove group of color c at p; return whether succesful */
/* for NET compatibility, allow arbitrary removes for now */
int mink_removedead(struct minkgame *gp, int p, int c)
{
  if (0 && gp->board[p] != c)
    return 0;
  return capture(gp,p,gp->board[p]) ? 1 : 0;
}


void mink_replay(struct minkgame *gp)	/* replay game, e.g. to undo all removes */
{
  int p,movecnt = gp->movenr;

  Logit("replaying to move nr. %d.", movecnt);
  mink_startgame(gp);
  if (movecnt && (p = gp->moves[1].point) < -1)
    mink_sethcap(gp,-1-p);
  while (gp->movenr < movecnt) {
/*    Logit("at move %d now.", gp->movenr); */
    mink_forward(gp);
  }
}


#if 0
static void showpass(struct minkgame *gp)
{
  mink_pass(gp);
  printf("%c passes.\n",BOARDCHARS[LASTCOLOR(gp)]);
}
#endif


static char cnv_file2ch(int i)
{
  return 'A' + i-1 >= 'I' ?  'A' + i : 'A' + i-1;
}


void mink_listmove(struct minkgame *gp, int i, char *buf)	/* list move i in game gp */
{
  int pt;

  *buf++ = (i&1) ? 'B' : 'W';
  pt = gp->moves[i].point;
  if (pt < -1)
    sprintf(buf,"Handicap %d",-1-pt);
  else if (pt == MINK_PASS)
    sprintf(buf,"Pass");
  else sprintf(buf, "%c%d", cnv_file2ch(pt%VDIFF(gp)), pt/VDIFF(gp));
}


void mink_printboard(struct minkgame *gp, twodstring buf)
{
  int p,x,y,xpos;
  const char *star;

  sprintf(buf[0],"   ");
  for (x=1; x<=gp->width; x++)
     sprintf(buf[0]+2*x+1," %c", cnv_file2ch(x));
  for (y = 1; y <= gp->height; y++)
  {
    sprintf(buf[y],"%2d |", gp->height+1-y);
    for (x=1, xpos=4; ; x++)
    {
      buf[y][xpos++] = BOARDCHARS[gp->board[mink_point(gp,x,gp->height+1-y)]];
      if (x == gp->width)
        break;
      buf[y][xpos++] = ' ';
    }
    sprintf(buf[y]+xpos,"| %2d", gp->height+1-y);
  }
  sprintf(buf[y],"   ");
  for (x=1; x<=gp->width; x++)
     sprintf(buf[y]+2*x+1," %c", cnv_file2ch(x));
  p = gp->moves[gp->movenr].point;
  if (p > 0 && p != MINK_PASS ) {
    y = gp->height + 1 - p / VDIFF(gp);
    x = 2 * (p % VDIFF(gp)) + 2;
    buf[y][2] = BOARDCHARS[9];
    buf[y][x-1] = BOARDCHARS[9];
    buf[y][x+1] = BOARDCHARS[10];
    buf[y][2*gp->width+4] = BOARDCHARS[10];
  }
  if (gp->width < (int) sizeof stars)
  {
    star = stars[gp->width];
    while (*star)
    {
      x = *star++ - 'a' + 1;
      y = *star++ - 'a' + 1;
      if (gp->board[mink_point(gp,x,y)] == MINK_EMPTY)
        buf[gp->height+1-y][x*2+2] = BOARDCHARS[8];
    }
  }
}

int mink_raw_printboard(char * buff, size_t buflen, struct minkgame *gp)
{
  int x,y,len, pos;

  len = snprintf(buff, buflen, "buff[%u]\n" , (unsigned)buflen);
  if (len < 0) return len;

  pos = len;
  len = snprintf(buff+pos, buflen-pos, "0x%p\n" , (void*)gp );
  if (len < 0) return pos;
  if (!gp) return pos+len;
  pos--;
  len = snprintf(buff+pos, buflen-pos, ":%d:%d:%d:%d:%d\n"
	, (int)gp->height, (int)gp->width
	, (int)gp->rules, (int)gp->handicap, (int)gp->mvsize 
	);
  if (len < 0) return pos;
  pos += len;
  if (!gp || !gp->board) return pos;
  /* pos = 0; */
  for (y=1; y <= gp->height; y++) {
    for (x=1; x <= gp->width ; x++) {
      if (pos+2 >= (int)buflen) break;
      /* buff[done++] = BOARDCHARS[gp->board[mink_point(gp,x,gp->height+1-y)]]; */
      buff[pos++] = BOARDCHARS[gp->board[mink_point(gp,x,y)]];
    }
    buff[pos++] = '\n';
  }
  buff[pos] = '\0';
  return pos;
}

void mink_statusdims(struct minkgame *gp, int *width, int *height)
{
  *width = gp->width;
  *height = gp->height;
}

/* broken just like IGS protocol; only works for height==width */
void mink_boardstatus(struct minkgame *gp, twodstring buf)
{
  int x,y;

  for (y=0; y<gp->height; y++) {
    for (x=0; x < gp->width; x++)
      buf[y][x] = "201 3543"[gp->board[mink_point(gp,y+1,gp->height-x)]];
    buf[y][x] = '\0';
  }
}

static int findowner(struct minkgame *gp, int p)
{
  if (gp->board[p] == MINK_EDGE || gp->board[p] == 4+MINK_EMPTY)
    return 0;
  if (gp->board[p] != MINK_EMPTY)
    return gp->board[p];
  gp->board[p] = 4+MINK_EMPTY;
  return findowner(gp,p-VDIFF(gp)) | findowner(gp,p-1)
         | findowner(gp,p+1) | findowner(gp,p+VDIFF(gp));
}

static void setowner(struct minkgame *gp, int p, int c)
{
  if (gp->board[p] == 4+MINK_EMPTY) {
    gp->board[p] = 4 + c;
    setowner(gp,p-VDIFF(gp),c);
    setowner(gp,p-1,c);
    setowner(gp,p+1,c);
    setowner(gp,p+VDIFF(gp),c);
  }
}

void mink_getcaps(struct minkgame *gp, int *wh, int *bl)
{
  *wh = gp->caps[MINK_WHITE];
  *bl = gp->caps[MINK_BLACK];
}

void mink_countscore(struct minkgame *gp, twodstring buf, int *wt, int *bt, int *wo, int *bo)
{
  int p,own;

  *wt = *bt = *wo = *bo = 0;  /* territory and occupied */
  for (p=VDIFF(gp)+1; p<ESIZE(gp)-VDIFF(gp); p++) {
    if (gp->board[p] == MINK_EDGE)
      continue;
    if (gp->board[p] == MINK_EMPTY && (own = findowner(gp,p)))
      setowner(gp,p,own);
    if (gp->board[p] == MINK_BLACK)
      (*bo)++;
    else if (gp->board[p] == 4+MINK_BLACK)
      (*bt)++;
    else if (gp->board[p] == MINK_WHITE)
      (*wo)++;
    else if (gp->board[p] == 4+MINK_WHITE)
      (*wt)++;
  }
  mink_boardstatus(gp, buf);
  for (p=VDIFF(gp)+1; p<ESIZE(gp)-VDIFF(gp); p++)
    if (gp->board[p] >= 4)
      gp->board[p] = MINK_EMPTY;
}
