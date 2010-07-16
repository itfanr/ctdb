#ifndef FAILTEST_H
#define FAILTEST_H
#include <stdbool.h>

bool should_i_fail_once(const char *location);
bool should_i_fail(const char *func);

bool failtest;

/* Parent never has artificial failures. */
bool am_parent(void);

void dump_failinfo(void);

#endif /* FAILTEST_H */
