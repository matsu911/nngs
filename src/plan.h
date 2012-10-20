#ifndef PLAN_H
#define PLAN_H

/* plan.h
** Per-Erik Martin (pem@nexus.se) 1999-05-08
**
*/

/* Creates a planed list. */
struct plan * plan_init(void);

/* Returns the number of info lines in list. */
size_t plan_count(struct plan *p);

/* Returns a pointer to the line 'c' if in bounds, NULL otherwise. 
** Lines are numbered 0 - n-1, where n is the current number of lines.
*/
char *plan_get(size_t i, struct plan *p);

/* Inserts the line 's' at the point 'i'.
** If 'i' is > plan_count(), blank lines are inserted.
** Lines are numbered 0 - n-1, where n is the current number of lines.
*/
void plan_insert(const char *s, size_t i, struct plan *p);

/* Replaces the line at 'i', with 's'.
** If 'i' is >= plan_count(), it is equivalent to plan_insert().
*/
void plan_set(const char *s, size_t i, struct plan *p);

/* Adds the info line 's' last.
*/
void plan_add(const char *s, struct plan *p);

/* Removes the line 'i' from list.
** The list is compacted, so this is not the same thing as replacing
** with a blank line.
*/
void plan_rem(size_t i, struct plan *p);

/* Initiates an iteration through the list.
** A typical iteration (through the list 'cl') looks like this:
**
** {
**   char *l;
**
**   plan_start(pl);
**   while (plan_next(&l, pl))
**     do_something_with(l);
** }
*/
void plan_start(struct plan *p);

/* Gives the next planed in turn.
** Returns 1 if there was one, 0 if we reached the end.
*/
int plan_next(char **cp, struct plan *p);

/* Clear the list
*/
void plan_clear(struct plan *p);

/* Destroy the list
*/
void plan_free(struct plan *p);

#endif /* PLAN_H */
