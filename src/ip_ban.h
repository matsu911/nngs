#ifndef IP_BAN_H
#define IP_BAN_H 1
int range_check(unsigned bot, unsigned top);
int range_add(unsigned bot, unsigned top);
int range_remove(unsigned bot, unsigned top);
void range_dump(FILE *fp);
int range_iter(unsigned *pb, unsigned *pt);

#endif /* IP_BAN_H */
