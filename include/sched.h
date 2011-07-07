#include <stddef.h>
#include <errno.h>
#include <sys/types.h>

#include <process.h>
#include <limits.h>
#include <signal.h>

#include <sys/timeb.h>

#ifndef WIN_SCHED_PTHREADS
#define WIN_SCHED_PTHREADS

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

#if defined DLL_EXPORT && !defined (WINPTHREAD_EXPORT_ALL_DEBUG)
#ifdef IN_WINPTHREAD
#define WINPTHREAD_SCHED_API __declspec(dllexport)
#else
#define WINPTHREAD_SCHED_API __declspec(dllimport)
#endif
#else
#define WINPTHREAD_SCHED_API
#endif

int WINPTHREAD_SCHED_API sched_yield(void);
int WINPTHREAD_SCHED_API sched_get_priority_min(int pol);
int WINPTHREAD_SCHED_API sched_get_priority_max(int pol);
int WINPTHREAD_SCHED_API sched_getscheduler(pid_t pid);
int WINPTHREAD_SCHED_API sched_setscheduler(pid_t pid, int pol);

#ifdef __cplusplus
}
#endif

#endif

#ifndef sched_rr_get_interval
#define sched_rr_get_interval(_p, _i) \
  ( errno = ENOTSUP, (int) -1 )
#endif

#endif/*WIN_SCHED_PTHREADS*/
