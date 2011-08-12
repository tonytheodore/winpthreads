/*
   Copyright (c) 2011 mingw-w64 project

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

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

  if (sem)
    *sem = NULL;
  if (!sem || value > (unsigned int)SEM_VALUE_MAX)
    return sem_result (EINVAL);
  if (pshared != PTHREAD_PROCESS_PRIVATE)
    return sem_result (EPERM);

  if (!(sv = (sem_t) calloc (1,sizeof (*sv))))
    return sem_result(ENOMEM); 

  sv->value = value;
  if (pthread_mutex_init(&sv->vlock, NULL) != 0)
    {
      free (sv);
      return sem_result(ENOSPC);
    }
  if ((sv->s = CreateSemaphore (NULL, 0, SEM_VALUE_MAX, NULL)) == NULL)
    {
      pthread_mutex_destroy (&sv->vlock);
      free (sv);
      return sem_result(ENOSPC); 
    }

  _spin_lite_lock (&spin_sem_locked);
  if (enter_to_known_sems (sv) != 0)
    {
      CloseHandle (sv->s);
      pthread_mutex_destroy(&sv->vlock);
      free(sv);
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (ENOSPC); 
    }
  _spin_lite_unlock (&spin_sem_locked);

  sv->valid = LIFE_SEM;
  *sem = sv;
  return 0;
}

int
sem_destroy (sem_t *sem)
{
  int r;
  _sem_t *sv = NULL;
  sKnownSems *hash;

  _spin_lite_lock (&spin_sem_locked);
  if (!sem || (sv = *sem) == NULL
      || (hash = seek_known_sems (sv)) == NULL)
    {
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EINVAL);
    }
  if ((r = pthread_mutex_lock (&sv->vlock)) != 0)
    {
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (r);
    }
  if (sv->value < 0 || sv->valid == DEAD_SEM)
    {
      pthread_mutex_unlock (&sv->vlock);
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EBUSY);
    }
  if (!CloseHandle (sv->s))
    {
      pthread_mutex_unlock (&sv->vlock);
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EINVAL);
    }
  *sem = NULL;
  sv->value = SEM_VALUE_MAX;
  pthread_mutex_unlock(&sv->vlock);
  _spin_lite_unlock(&spin_sem_locked);
  Sleep (0);
  while (pthread_mutex_destroy (&sv->vlock) == EBUSY)
    Sleep (0);
  _spin_lite_lock (&spin_sem_locked);
  remove_from_known_sems (hash);
  sv->valid = DEAD_SEM;
  free (sv);
  _spin_lite_unlock (&spin_sem_locked);
  return 0;
}

static int
sem_std_enter (sem_t *sem,_sem_t **svp, int do_test)
{
  int r;
  _sem_t *sv;

  if (do_test)
    pthread_testcancel ();
  _spin_lite_lock (&spin_sem_locked);
  if (!sem || (sv = *sem) == NULL)
    {
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EINVAL);
    }
  if (!seek_known_sems (sv))
    {
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EINVAL);
    }
  _spin_lite_unlock (&spin_sem_locked);
  if ((r = pthread_mutex_lock (&sv->vlock)) != 0)
    return sem_result (r);

  if (*sem == NULL)
    {
      pthread_mutex_unlock(&sv->vlock);
      return sem_result (EINVAL);
    }
  *svp = sv;
  return 0;
}

int
sem_trywait (sem_t *sem)
{
  _sem_t *sv;

  if (sem_std_enter (sem, &sv, 1) != 0)
    return -1;
  if (sv->value <= 0)
    {
      pthread_mutex_unlock (&sv->vlock);
      return sem_result (EAGAIN);
    }
  sv->value--;
  pthread_mutex_unlock (&sv->vlock);

  return 0;
}

struct sSemTimedWait
{
  sem_t *p;
  int *ret;
};

static void
clean_wait_sem (void *s)
{
  struct sSemTimedWait *p = (struct sSemTimedWait *) s;
  _sem_t *sv = NULL;
  if (sem_std_enter (p->p, &sv, 0) != 0)
    return;

  _spin_lite_lock (&spin_sem_locked);
  if (WaitForSingleObject (sv->s, 0) != WAIT_OBJECT_0)
    InterlockedIncrement ((long *) &sv->value);
  else if (p->ret)
    p->ret[0] = 0;
  _spin_lite_unlock (&spin_sem_locked);
  pthread_mutex_unlock (&sv->vlock);
}

int
sem_wait (sem_t *sem)
{
  int cur_v, ret = 0;
  _sem_t *sv;
  HANDLE semh;
  struct sSemTimedWait arg;

  if (sem_std_enter (sem, &sv, 1) != 0)
    return -1;

  arg.ret = NULL;
  arg.p = sem;
  InterlockedDecrement ((long *) &sv->value);
  cur_v = sv->value;
  semh = sv->s;
  pthread_mutex_unlock (&sv->vlock);

  if (cur_v >= 0)
    return 0;
  else
    {
      pthread_cleanup_push (clean_wait_sem, (void *) &arg);
      ret = do_sema_b_wait_intern (semh, 2, INFINITE);
      pthread_cleanup_pop (ret);
      if (ret == EINVAL)
        ret = 0;
    }

  if (!ret)
    return 0;

  return sem_result (ret);
}

int
sem_timedwait (sem_t *sem, const struct timespec *t)
{
  int cur_v, ret = 0;
  DWORD dwr;
  HANDLE semh;
  _sem_t *sv;;
  struct sSemTimedWait arg;

  if (!t)
    return sem_wait(sem);
  dwr = dwMilliSecs(_pthread_rel_time_in_ms(t));

  if (sem_std_enter (sem, &sv, 1) != 0)
    return -1;

  arg.ret = &ret;
  arg.p = sem;
  InterlockedDecrement ((long *) &sv->value);
  cur_v = sv->value;
  semh = sv->s;
  pthread_mutex_unlock(&sv->vlock);

  if (cur_v >= 0)
    return 0;
  else
    {
      pthread_cleanup_push (clean_wait_sem, (void *) &arg);
      ret = do_sema_b_wait_intern (semh, 2, dwr);
      pthread_cleanup_pop (ret);
      if (ret == EINVAL)
        ret = 0;
    }
  if (!ret)
    return 0;

  return sem_result (ret);
}

int
sem_post (sem_t *sem)
{
  _sem_t *sv;;

  if (sem_std_enter (sem, &sv, 0) != 0)
    return -1;

  if (sv->value >= SEM_VALUE_MAX)
    {
      pthread_mutex_unlock (&sv->vlock);
      return sem_result (ERANGE);
    }
  InterlockedIncrement ((long *)&sv->value);
  if (sv->value > 0)
    {
      pthread_mutex_unlock (&sv->vlock);
      return 0;
    }
  if (ReleaseSemaphore (sv->s, 1, NULL))
    {
      pthread_mutex_unlock (&sv->vlock);
      return 0;
    }
  InterlockedDecrement ((long *) &sv->value);
  pthread_mutex_unlock (&sv->vlock);

  return sem_result (EINVAL);
}

int
sem_post_multiple (sem_t *sem, int count)
{
  int waiters_count;
  _sem_t *sv;;

  if (count <= 0)
    return sem_result (EINVAL);
  if (sem_std_enter (sem, &sv, 0) != 0)
    return -1;

  if (sv->value > (SEM_VALUE_MAX - count))
  {
    pthread_mutex_unlock (&sv->vlock);
    return sem_result (ERANGE);
  }
  waiters_count = -sv->value;
  InterlockedExchangeAdd((long*)&sv->value, (long) count);
  if (waiters_count <= 0)
  {
    pthread_mutex_unlock(&sv->vlock);
    return 0;
  }
  if (ReleaseSemaphore (sv->s,
			waiters_count < count ? waiters_count : count, NULL))
  {
    pthread_mutex_unlock(&sv->vlock);
    return 0;
  }
  InterlockedExchangeAdd((long*)&sv->value, -((long) count));
  pthread_mutex_unlock(&sv->vlock);
  return sem_result(EINVAL);
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
  return sem_result (ENOSYS);
}

int
sem_unlink (const char *name)
{
  return sem_result (ENOSYS);
}

int
sem_getvalue (sem_t *sem, int *sval)
{
  _sem_t *sv;;
  int r;

  if (!sval)
    return sem_result (EINVAL);

  _spin_lite_lock (&spin_sem_locked);
  if (!sem || (sv = *sem) == NULL || !seek_known_sems (sv))
    {
      _spin_lite_unlock (&spin_sem_locked);
      return sem_result (EINVAL);
    }
  _spin_lite_unlock (&spin_sem_locked);
  if ((r = pthread_mutex_lock (&sv->vlock)) != 0)
    return sem_result (r);
  if (*sem == NULL || sv->valid == DEAD_SEM)
    {
      pthread_mutex_unlock (&sv->vlock);
      return sem_result (EINVAL);
    }

  *sval = sv->value;
  pthread_mutex_unlock (&sv->vlock);
  return 0;  
}
