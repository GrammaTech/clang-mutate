
#include <sys/types.h>

#include <stdarg.h>

int n;

#define THREADED_VAR(name, type)                                        \
                type __get_##name(void) { return name; }                \
                void __set_##name(type arg) { name = arg; }


THREADED_VAR(n, pid_t)
