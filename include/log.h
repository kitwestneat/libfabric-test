#ifndef LOG_H
#define LOG_H

#include <stdlib.h>

#define _GOTO(retv, label, fmt, ...) do { \
		fprintf(stderr, "%s:%d " fmt " [rc: %d]\n", __FILE__, __LINE__, __VA_ARGS__ __VA_OPT__(,) (int)(retv)); \
		goto label; \
	} while(0)
#define GOTO(...) _GOTO(rc, __VA_ARGS__)


#define FI_GOTO(label, call) \
	GOTO(label, call "(): %s", fi_strerror((int) -(rc)))

#endif
