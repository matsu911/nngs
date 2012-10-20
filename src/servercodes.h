#ifndef SERVERCODE_H
#define SERVERCODE_H 1

/* 
   These are the possible values for in parray.session.protostate.
   They are used as the first second number in the client-type numerical prompt.
   For more information on these #$%^$%## defines, look at client source
   code, such as xigc.  What a horrible way to live.
   Renamed these to CODE_ , to make them more greppable. AvK
*/

#define CODE_NONE      0   
#define CODE_PROMPT    1   
#define CODE_BEEP      2  
#define CODE_DOWN      4  
#define CODE_ERROR     5  
#define CODE_FILE      6  
#define CODE_GAMES     7  
#define CODE_HELP      8  
#define CODE_INFO      9  
#define CODE_LAST     10  
#define CODE_KIBITZ   11  
#define CODE_LOAD     12 
#define CODE_LOOK_M   13
#define CODE_MESSAGE  14   
#define CODE_MOVE     15  
#define CODE_OBSERVE  16 
#define CODE_REFRESH  17
#define CODE_SAVED    18   
#define CODE_SAY      19  
#define CODE_SCORE_M  20 
#define CODE_SHOUT    21
#define CODE_STATUS   22   
#define CODE_STORED   23  
#define CODE_TELL     24 
#define CODE_THIST    25
#define CODE_TIME     26   
#define CODE_WHO      27  
#define CODE_UNDO     28 
#define CODE_SHOW     29
#define CODE_TRANS    30   
#define CODE_YELL     32  
#define CODE_TEACH    33 
#define CODE_MVERSION 39
#define CODE_DOT      40   
#define CODE_CLIVRFY  41  
/* PEM: Removing stones. */
#define CODE_REMOVED  49
#define CODE_EMOTE    500
#define CODE_EMOTETO  501
#define CODE_PING     502
	/* AvK: Sendcodes can now be or'ed with CODE_CR1 to prepend a NL */
#define CODE_MASK     511
#define CODE_CR1      1024
	/* These are used for the second number on the protocol lines
        ** when the first number is '1' (CODE_PROMPT)
	** AvK: 
	** grabbed these (5-10) from playerdb.h
	** Added 0-4 from IGS docs
	*/
#define STAT_LOGON	0	/* Unused */
#define STAT_PASSWORD	1	/* Unused */
#define STAT_PASSWORD_NEW	2	/* Unused */
#define STAT_PASSWORD_CONFIRM	3	/* Unused */
#define STAT_REGISTER	4	/* Unused */
#define STAT_WAITING	5	
#define STAT_PLAYING_GO	6 
#define STAT_SCORING	7
#define STAT_OBSERVING	8	
#define STAT_TEACHING	9	/* Unused */
#define STAT_COMPLETE	10	/* Unused */

#endif /* SERVERCODE_H */
