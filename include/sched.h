#include <stddef.h>
#include <errno.h>
#include <sys/types.h>

#include <process.h>
#include <limits.h>
#include <signal.h>

#include <sys/timeb.h>

#ifndef SCHED_OTHER
/* Some POSIX realtime extensions, mostly stubbed */
#define SCHED_OTHER     0
#define SCHED_FIFO      1
#define SCHED_RR        2
#define SCHED_MIN       SCHED_OTHER
#define SCHED_MAX       SCHED_RR

struct sched_param {
  int sched_priority;
};

#ifdef __cplusplus
extern "C" {
#endif

int sched_yield(void);
int sched_get_priority_min(int pol);
int sched_get_priority_max(int pol);
int sched_getscheduler(pid_t pid);
int sched_setscheduler(pid_t pid, int pol);

#ifdef __cplusplus
}
#endif

#endif

#ifndef sched_rr_get_interval
#define sched_rr_get_interval(_p, _i) \
  ( errno = ENOTSUP, (int) -1 )
#endif
