
#ifndef EMOTE_H
#define EMOTE_H

/*
   EmoteInit does nothing.
*/

void EmoteInit(const char *path);

/*
   EmoteMkStr returns a const char *, so you're gonna have to
   cast it if you want to modify it. My advice is to make your own
   modified copy.
*/

const char *EmoteMkStr(char *key, char *username, char *args, int isclient);

/*
   EmoteLook returns a boolean - 1 if the key exists and 0 if it doesn't
*/

int EmoteLook(const char *key);

#endif /* EMOTE_H */
