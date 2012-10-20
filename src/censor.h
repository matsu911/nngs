#ifndef CENSOR_H
#define CENSOR_H

/* censor.h
** Per-Erik Martin (pem@nexus.se) 1999-05-08
**
*/

/* Creates a censored list. */
struct censor * censor_init(void);

/* Returns the number of censored in list. */
size_t censor_count(struct censor *p);

/* Returns a pointer to the censored 'c' if found, NULL otherwise. */
char *censor_lookup(const char *c, struct censor *p);

/* Adds the censored 'c'. Returns 1 if it replaced an old entry,
** or 0 if it was a new entry.
*/
int censor_add(const char *c, struct censor *p);

/* Removes censored 'c' from list. Returns 1 if it was there,
** or 0 if it wasn't found.
*/
int censor_rem(const char *c, struct censor *p);

/* Initiates an iteration through the list.
** A typical iteration (through the list 'cl') looks like this:
**
** {
**   char *c;
**
**   censor_start(cl);
**   while (censor_next(&c, cl))
**     do_something_with(c);
** }
*/
void censor_start(struct censor *p);

/* Gives the next censored in turn.
** Returns 1 if there was one, 0 if we reached the end.
*/
int censor_next(char **cp, struct censor *p);

/* Clear the list
 */
void censor_clear(struct censor *p);

/* Destroy the list
*/
void censor_free(struct censor *p);

#endif /* censor_h */
