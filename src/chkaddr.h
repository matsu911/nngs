/* chkaddr.h
**
** Per-Erik Martin (pem@nngs.cosmic.org) 1999-11-25
** AvK: flipped polarity of returnvalue...
*/

#ifndef CHKADDR_H
#define CHKADDR_H

/* Check if an email address seems to be correct, that is, on the form
 *   name@domainname
 * where name matches [A-Za-z0-9._-]+, and domainname is a dot separated
 * sequence of [A-Za-z0-9-]+ names, with at least one dot.
 * (Note that '_' is NOT a legal character in domainnames.)
 *
 * Returns 0 if it's a likely email address,
 * 1 if it refers to user@localhost
 * < 0 otherwise.
 */
int chkaddr(const char *s);

#endif /* chkaddr_h */
