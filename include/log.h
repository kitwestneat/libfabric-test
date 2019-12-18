#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

#define _GOTO(retv, label, fmt, ...)                                                               \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, "%s:%d [rc: %d] " fmt "\n", __FILE__, __LINE__, (int)(retv),               \
                ##__VA_ARGS__);                                                                    \
        goto label;                                                                                \
    } while (0)

#define GOTO(...) _GOTO(rc, __VA_ARGS__)

#define FI_GOTO(label, call) GOTO(label, call "(): %s", fi_strerror((int)-(rc)))

#endif
