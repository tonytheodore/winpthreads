#include <windows.h>
#include <stdio.h>
#include "pthread.h"
#include "thread.h"
#include "misc.h"
#include "semaphore.h"
#include "sem.h"
#include "mutex.h"
#include "ref.h"
#include "spinlock.h"

int do_sema_b_wait_intern (HANDLE sema, int nointerrupt, DWORD timeout);

/* Locking for checks if semaphore is still valid and avoid
   asynchrone deletion.  */
static spin_t spin_sem_locked = {0,LIFE_SPINLOCK,0};

typedef struct sKnownSems {
  struct sKnownSems *prev;
  struct sKnownSems *next;
  void *ptr;
} sKnownSems;

static sKnownSems root_sem;

static sKnownSems *
seek_known_sems (void *p)
{
  sKnownSems *c = root_sem.next;
  while (c != NULL && c->ptr != p)
    c = c->next;
  return c;
}

static int
enter_to_known_sems (void *p)
{
  sKnownSems *n;
  if (!p)
    return -1;
  n = (sKnownSems *) malloc (sizeof (sKnownSems));
  if (!n)
    return -1;
  n->next = root_sem.next;
  n->prev = &root_sem;
  root_sem.next = n;
  if (n->next)
    n->next->prev = n;
  n->ptr = p;
  return 0;
}

static int
remove_from_known_sems (sKnownSems *p)
{
  if (!p)
    return -1;
  p->prev->next = p->next;
  if (p->next)
    p->next->prev = p->prev;
  free (p);
  return 0;
}

static int
sem_result (int res)
{
  if (res != 0) {
    errno = res;
    return -1;
  }
  return 0;
}


int
sem_init (sem_t *sem, int pshared, unsigned int value)
{
  _sem_t *sv;

  _spin_lite_lock(&spin_sem_locked);
  if (!sem || value > (unsigned int)SEM_VALUE_MAX)
  {
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EINVAL);
  }
  if (pshared != PTHREAD_PROCESS_PRIVATE)
  {
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EPERM);
  }

  if (!(sv = (sem_t)calloc(1,sizeof(*sv))))
  {
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(ENOMEM); 
  }
  if (pthread_mutex_init(&sv->vlock, NULL) != 0)
  {
    sv->valid = DEAD_SEM;
    free(sv);
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(ENOSPC);
  }
  if ((sv->s = CreateSemaphore (NULL, 0, SEM_VALUE_MAX, NULL)) == NULL)
  {
    pthread_mutex_destroy(&sv->vlock);
    sv->valid = DEAD_SEM;
    free(sv);
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(ENOSPC); 
  }
  if (enter_to_known_sems (sv) != 0)
    {
      CloseHandle (sv->s);
      pthread_mutex_destroy(&sv->vlock);
      sv->valid = DEAD_SEM;
      free(sv);
      _spin_lite_unlock(&spin_sem_locked);
      return sem_result(ENOSPC); 
    }

  sv->value = value;
  sv->valid = LIFE_SEM;
  *sem = sv;
  _spin_lite_unlock(&spin_sem_locked);
  return 0;
}

int sem_destroy(sem_t *sem)
{
  _sem_t *sv = NULL;
  int sem_state;
  sKnownSems *hash;

  _spin_lite_lock(&spin_sem_locked);
  if (!sem || (sv = *sem) == NULL
      || (hash = seek_known_sems (sv)) == NULL)
  {
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EINVAL);
  }
  if (sem_result(pthread_mutex_lock(&sv->vlock)) != 0)
  {
    _spin_lite_unlock(&spin_sem_locked);
    return -1;
  }
  if (sv->value < 0 || sv->valid == DEAD_SEM)
  {
    pthread_mutex_unlock(&sv->vlock);
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EBUSY);
  }
  sem_state = sv->valid;
  sv->valid = DEAD_SEM;
  *sem = NULL;
  if (!CloseHandle (sv->s))
  {
    *sem = sv;
    sv->valid = sem_state;
    pthread_mutex_unlock(&sv->vlock);
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EINVAL);
  }
  sv->value = SEM_VALUE_MAX;
  *sem = NULL;
  remove_from_known_sems (hash);
  pthread_mutex_unlock(&sv->vlock);
  _spin_lite_unlock(&spin_sem_locked);
  Sleep(0);
  while (pthread_mutex_destroy (&sv->vlock) == EBUSY)
    Sleep(0);
  _spin_lite_lock(&spin_sem_locked);
  sv->valid = DEAD_SEM;
  free (sv);
  _spin_lite_unlock(&spin_sem_locked);
  return 0;
}

static int
sem_std_enter(sem_t *sem,_sem_t **svp)
{
  _sem_t *sv;

  pthread_testcancel();
  _spin_lite_lock(&spin_sem_locked);
  if (!sem || (sv = *sem) == NULL || !seek_known_sems (sv))
  {
    _spin_lite_unlock(&spin_sem_locked);
    return sem_result(EINVAL);
  }
  if (sem_result(pthread_mutex_lock(&sv->vlock)) != 0)
  {
    _spin_lite_unlock(&spin_sem_locked);
    return -1;
  }
  if (*sem == NULL || sv->valid == DEAD_SEM)
  {
     pthread_mutex_unlock(&sv->vlock);
     _spin_lite_unlock(&spin_sem_locked);
     return sem_result(EINVAL);
  }
  *svp = sv;
  _spin_lite_unlock(&spin_sem_locked);
  return 0;
}

int sem_trywait(sem_t *sem)
{
  _sem_t *sv;
  if (sem_std_enter (sem, &sv) != 0)
    return -1;
  if (sv->value <= 0)
  {
    pthread_mutex_unlock(&sv->vlock);
    return sem_result(EAGAIN);
  }
  sv->value--;
  pthread_mutex_unlock(&sv->vlock);

  return 0;
}

int sem_wait(sem_t *sem)
{
  int cur_v;
  _sem_t *sv;

  if (sem_std_enter (sem, &sv) != 0)
    return -1;
  InterlockedDecrement((long *)&sv->value);
  cur_v = sv->value;
  pthread_mutex_unlock(&sv->vlock);

  if (cur_v >= 0)
    return 0;

  cur_v = do_sema_b_wait_intern (sv->s, 2, INFINITE);
  if (!cur_v)
    return 0;
  _spin_lite_lock(&spin_sem_locked);
  if (*sem != NULL && sv->valid != DEAD_SEM
      && pthread_mutex_lock(&sv->vlock) == 0)
  {
    _spin_lite_unlock(&spin_sem_locked);
    if (WaitForSingleObject(sv->s, 0) != WAIT_OBJECT_0)
      InterlockedIncrement((long*)&sv->value);
    else
      cur_v = 0;
    pthread_mutex_unlock(&sv->vlock);
  }
  else
    _spin_lite_unlock(&spin_sem_locked);
  pthread_testcancel();
  return sem_result(cur_v);
}

int sem_timedwait(sem_t *sem, const struct timespec *t)
{
  int cur_v;
  DWORD dwr;
  _sem_t *sv;;

  if (!t)
    return sem_wait(sem);
  dwr = dwMilliSecs(_pthread_rel_time_in_ms(t));

  if (sem_std_enter (sem, &sv) != 0)
    return -1;

  InterlockedDecrement((long*)&sv->value);
  cur_v = sv->value;
  pthread_mutex_unlock(&sv->vlock);

  if (cur_v >= 0)
    return 0;
  cur_v = do_sema_b_wait_intern (sv->s, 2, dwr);
  if (!cur_v)
    return 0;

  _spin_lite_lock(&spin_sem_locked);
  if (*sem != NULL && pthread_mutex_lock(&sv->vlock) == 0)
  {
    _spin_lite_unlock(&spin_sem_locked);
    if (WaitForSingleObject(sv->s, 0) != WAIT_OBJECT_0)
      InterlockedIncrement((long*)&sv->value);
    else
      cur_v = 0;
    pthread_mutex_unlock(&sv->vlock);
  }
  else
    _spin_lite_unlock(&spin_sem_locked);
  pthread_testcancel();
  return sem_result(cur_v);
}

int
sem_post (sem_t *sem)
{
  _sem_t *sv;;

  if (sem_std_enter (sem, &sv) != 0)
    return -1;

  if (((long long) sv->value + 1LL) > (long long) SEM_VALUE_MAX)
  {
      pthread_mutex_unlock (&sv->vlock);
      return sem_result(ERANGE);
  }
  InterlockedIncrement((long*)&sv->value);
  if (sv->value > 0)
  {
    pthread_mutex_unlock (&sv->vlock);
    return 0;
  }
  if (ReleaseSemaphore(sv->s, 1, NULL))
  {
    pthread_mutex_unlock (&sv->vlock);
    return 0;
  }
  InterlockedDecrement((long*)&sv->value);
  pthread_mutex_unlock (&sv->vlock);
  return sem_result(EINVAL);  
}

int
sem_post_multiple (sem_t *sem, int count)
{
  int waiters_count;
  _sem_t *sv;;

  if (sem_std_enter (sem, &sv) != 0)
    return -1;

  if (((long long)sv->value + (long long) count) > (long long) SEM_VALUE_MAX)
  {
    pthread_mutex_unlock(&sv->vlock);
    return sem_result(ERANGE);
  }
  waiters_count = -sv->value;
  InterlockedExchangeAdd((long*)&sv->value, (long) count);
  if (waiters_count <= 0)
  {
    pthread_mutex_unlock(&sv->vlock);
    return 0;
  }
  if (!ReleaseSemaphore(sv->s, waiters_count < count ? waiters_count : count, NULL))
  {
    InterlockedExchangeAdd((long*)&sv->value, (long) -count);
    pthread_mutex_unlock(&sv->vlock);
    return sem_result(EINVAL);
  }
  pthread_mutex_unlock(&sv->vlock);
  return 0;
}

sem_t *
sem_open (const char *name, int oflag, mode_t mode, unsigned int value)
{
  sem_result(ENOSYS);
  return NULL;
}

int
sem_close (sem_t *sem)
{
  return sem_result(ENOSYS);
}

int
sem_unlink (const char *name)
{
  return sem_result(ENOSYS);
}

int
sem_getvalue (sem_t *sem, int *sval)
{
  _sem_t *sv;;
  if (sem_std_enter (sem, &sv) != 0)
    return -1;

  *sval = sv->value;
  pthread_mutex_unlock(&sv->vlock);
  return 0;  
}
