#ifndef WIN_SEMAPHORE
#define WIN_SEMAPHORE

#ifdef __cplusplus
extern "C" {
#endif

#if defined DLL_EXPORT && !defined (WINPTHREAD_EXPORT_ALL_DEBUG)
#ifdef IN_WINPTHREAD
#define WINPTHREAD_SEMA_API __declspec(dllexport)
#else
#define WINPTHREAD_SEMA_API __declspec(dllimport)
#endif
#else
#define WINPTHREAD_SEMA_API
#endif

/* Set this to 0 to disable it */
#define USE_SEM_CriticalSection_SpinCount	100

#define SEM_VALUE_MAX   INT_MAX

#ifndef _MODE_T_
#define	_MODE_T_
typedef unsigned short mode_t;
#endif

typedef void	        *sem_t;

#define SEM_FAILED 		NULL

int WINPTHREAD_SEMA_API sem_init(sem_t * sem, int pshared, unsigned int value);

int WINPTHREAD_SEMA_API sem_destroy(sem_t *sem);

int WINPTHREAD_SEMA_API sem_trywait(sem_t *sem);

int WINPTHREAD_SEMA_API sem_wait(sem_t *sem);

int WINPTHREAD_SEMA_API sem_timedwait(sem_t * sem, const struct timespec *t);

int WINPTHREAD_SEMA_API sem_post(sem_t *sem);

int WINPTHREAD_SEMA_API sem_post_multiple(sem_t *sem, int count);

/* yes, it returns a semaphore (or SEM_FAILED) */
sem_t * WINPTHREAD_SEMA_API sem_open(const char * name, int oflag, mode_t mode, unsigned int value);

int WINPTHREAD_SEMA_API sem_close(sem_t * sem);

int WINPTHREAD_SEMA_API sem_unlink(const char * name);

int WINPTHREAD_SEMA_API sem_getvalue(sem_t * sem, int * sval);

#ifdef __cplusplus
}
#endif

#endif /* WIN_SEMAPHORE */
