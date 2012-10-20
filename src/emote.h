/* -*-C-*-

   emote.h -- just do some text substitution on looked up strings

   Copyright 1995-1997 J. Alan Eldridge.	

   %G% %W%
*/
#ifndef EMOTE_H
#define EMOTE_H

/*
   EmoteInit saves pathname off, does NOT check that we can read the file
*/

void EmoteInit(const char *pszPathName);

/*
   EmoteMkStr returns a const char *, so you're gonna have to
   cast it if you want to modify it. My advice is to make your own
   modified copy.
*/

const char *EmoteMkStr(const char *pszKey, const char *pszUsr,
		       const char *pszArgStr, int fClient);

/*
   EmoteLook returns a boolean - 1 if the key exists and 0 if it doesn't
*/

int EmoteLook(const char *pszKey);


#endif /* ifndef EMOTE_H */
