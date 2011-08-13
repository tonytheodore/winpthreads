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
#include "spinlock.h"
#include "misc.h"
      
static spin_t spin_locked = {0,LIFE_SPINLOCK, 1};

static int
dec_test (long volatile *p)
{
  unsigned char ret = 0;
  __asm__ __volatile__ ("lock\n\t"
    "decl %1\n"
    "setz %0"
    : "+r" (ret), "+m" (*p)
    : : "memory");
  return (ret != 0);
}

static int
inc_test (long volatile *p)
{
  while (*p < 1)
    YieldProcessor ();
}

static inline int
spinlock_static_init (pthread_spinlock_t *l)
{
  int ret;
  _spin_lite_lock(&spin_locked);
  if (PTHREAD_SPINLOCK_INITIALIZER != *l)
  {
    /* Check that somebody called destroy already.  Otherwise assume someone crept in between.  */
    ret = (*l == NULL ? EINVAL : 0);
  }
  else
  {
    ret = pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE);
  }
  _spin_lite_unlock(&spin_locked);
  return ret;
}

int pthread_spin_init(pthread_spinlock_t *l, int pshared)
{
    spin_t *_l;
    
    if (!l) return EINVAL; 
    if (pshared == PTHREAD_PROCESS_SHARED)
      return ENOSYS;
    if (pshared != PTHREAD_PROCESS_SHARED && pshared != PTHREAD_PROCESS_PRIVATE) return EINVAL;
    if (!(_l = (pthread_spinlock_t)calloc(1, sizeof(*_l))))
        return ENOMEM;

    _l->valid = LIFE_SPINLOCK;
    _l->l = 1;
    *l = _l;
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *l)
{
  spin_t *_l;
  if (!l || !*l) return EINVAL;
  _spin_lite_lock(&spin_locked);
  if (*l == PTHREAD_SPINLOCK_INITIALIZER)
  {
    *l = NULL;
    _spin_lite_unlock(&spin_locked);
    return 0;
  }
  _spin_lite_unlock(&spin_locked);
  _l = (spin_t *)*l;
  if (((spin_t *)(*l))->valid != (unsigned int)LIFE_SPINLOCK)
    return EINVAL;
  
  if (_l->l != 1)
    return EBUSY;
  *l= NULL; /* dereference first, free later */
  _l->valid  = DEAD_SPINLOCK;
  _l->l = 1;
  free(_l);
  return 0;
}

/* No-fair spinlock due to lack of knowledge of thread number.  */
int pthread_spin_lock(pthread_spinlock_t *l)
{
  spin_t *_l;

  if (!l || *l == NULL)
    return EINVAL;
  if (PTHREAD_SPINLOCK_INITIALIZER == *l)
  {
    int r = spinlock_static_init(l);
    if (r != 0)
      return r;
  }
  _l = (spin_t *)*l;
  while (*l != NULL && !dec_test (&_l->l))
    {
      inc_test (&_l->l);
    }
  return (*l != NULL ? 0 : EINVAL);
}

int pthread_spin_trylock(pthread_spinlock_t *l)
{
  spin_t *_l;
  int r = 0;
  if (!l || *l == NULL)
    return EINVAL;
  if (PTHREAD_SPINLOCK_INITIALIZER == *l)
  {
    r = spinlock_static_init(l);
    if (r != 0)
      return r;
  }

  _l = (spin_t *)*l;
  if (dec_test (&_l->l))
    return 0;
  return EBUSY;
}

int _spin_lite_trylock(spin_t *l)
{
  CHECK_SPINLOCK_LITE(l);
  if (dec_test (&l->l))
    return 0;
  inc_test (&l->l);
  return EBUSY;
}
int _spin_lite_unlock(spin_t *l)
{
    CHECK_SPINLOCK_LITE(l);
    l->l = 1;

    return 0;
}

int
_spin_lite_lock(spin_t *l)
{
  CHECK_SPINLOCK_LITE(l);

  while (!dec_test (&l->l))
    {
      inc_test (&l->l);
    }
      
  return 0;
}


int
pthread_spin_unlock (pthread_spinlock_t *l)
{
  spin_t *_l;
  int r;
  if (!l || *l == NULL)
    return EINVAL;
  if (*l == PTHREAD_SPINLOCK_INITIALIZER)
    return EPERM;
  _l = (spin_t *)*l;
  if (_l->l >= 1)
    return EPERM;
  _l->l = 1;
  return 0;
}
