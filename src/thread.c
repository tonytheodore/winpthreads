#include <windows.h>
#include <signal.h>
#include "pthread.h"
#include "thread.h"
#include "misc.h"

static volatile long _pthread_cancelling;
static int _pthread_concur;

/* FIXME Will default to zero as needed */
static pthread_once_t _pthread_tls_once;
static DWORD _pthread_tls;

/* FIXME Note initializer is zero, so this works */
static pthread_rwlock_t _pthread_key_lock;
static unsigned long _pthread_key_max;
static unsigned long _pthread_key_sch;

static void _pthread_once_cleanup(pthread_once_t *o)
{
    *o = 0;
}

static int _pthread_once_raw(pthread_once_t *o, void (*func)(void))
{
    long state = *o;

    CHECK_PTR(o);
    CHECK_PTR(func);

    _ReadWriteBarrier();

    while (state != 1)
    {
        if (!state)
        {
            if (!InterlockedCompareExchange(o, 2, 0))
            {
                /* Success */
                func();

                /* Mark as done */
                *o = 1;

                return 0;
            }
        }

        YieldProcessor();

        _ReadWriteBarrier();

        state = *o;
    }

    /* Done */
    return 0;
}

void * pthread_timechange_handler_np(void * dummy)
{
	return 0;
}

pthread_t pthread_self(void);
int pthread_once(pthread_once_t *o, void (*func)(void))
{
    long state = *o;

    CHECK_PTR(o);
    CHECK_PTR(func);

    _ReadWriteBarrier();

    while (state != 1)
    {
        if (!state)
        {
            if (!InterlockedCompareExchange(o, 2, 0))
            {
                /* Success */
                pthread_cleanup_push(_pthread_once_cleanup, o);
                func();
                pthread_cleanup_pop(0);
                /* Mark as done */
                *o = 1;

                return 0;
            }
        }

        YieldProcessor();

        _ReadWriteBarrier();

        state = *o;
    }

    /* Done */
    return 0;

}

int pthread_key_create(pthread_key_t *key, void (* dest)(void *))
{
    unsigned int i;
    long nmax;
    void (**d)(void *);

    if (!key) return EINVAL;

    pthread_rwlock_wrlock(&_pthread_key_lock);

    for (i = _pthread_key_sch; i < _pthread_key_max; i++)
    {
        if (!_pthread_key_dest[i])
        {
            *key = i;
            if (dest)
            {
                _pthread_key_dest[i] = dest;
            }
            else
            {
                _pthread_key_dest[i] = (void(*)(void *))1;
            }
            pthread_rwlock_unlock(&_pthread_key_lock);

            return 0;
        }
    }

    for (i = 0; i < _pthread_key_sch; i++)
    {
        if (!_pthread_key_dest[i])
        {
            *key = i;
            if (dest)
            {
                _pthread_key_dest[i] = dest;
            }
            else
            {
                _pthread_key_dest[i] = (void(*)(void *))1;
            }
            pthread_rwlock_unlock(&_pthread_key_lock);

            return 0;
        }
    }

    if (!_pthread_key_max) _pthread_key_max = 1;
    if (_pthread_key_max == PTHREAD_KEYS_MAX)
    {
        pthread_rwlock_unlock(&_pthread_key_lock);

        return ENOMEM;
    }

    nmax = _pthread_key_max * 2;
    if (nmax > PTHREAD_KEYS_MAX) nmax = PTHREAD_KEYS_MAX;

    /* No spare room anywhere */
    d = (void (__cdecl **)(void *))realloc(_pthread_key_dest, nmax * sizeof(*d));
    if (!d)
    {
        pthread_rwlock_unlock(&_pthread_key_lock);

        return ENOMEM;
    }

    /* Clear new region */
    memset((void *) &d[_pthread_key_max], 0, (nmax-_pthread_key_max)*sizeof(void *));

    /* Use new region */
    _pthread_key_dest = d;
    _pthread_key_sch = _pthread_key_max + 1;
    *key = _pthread_key_max;
    _pthread_key_max = nmax;

    if (dest)
    {
        _pthread_key_dest[*key] = dest;
    }
    else
    {
        _pthread_key_dest[*key] = (void(*)(void *))1;
    }

    pthread_rwlock_unlock(&_pthread_key_lock);

    return 0;
}

int pthread_key_delete(pthread_key_t key)
{
    if (key > _pthread_key_max) return EINVAL;
    if (!_pthread_key_dest) return EINVAL;

    pthread_rwlock_wrlock(&_pthread_key_lock);
    _pthread_key_dest[key] = NULL;

    /* Start next search from our location */
    if (_pthread_key_sch > key) _pthread_key_sch = key;

    pthread_rwlock_unlock(&_pthread_key_lock);

    return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
    pthread_t t = pthread_self();

    if (key >= t->keymax) return NULL;

    return t->keyval[key];

}

int pthread_setspecific(pthread_key_t key, const void *value)
{
    pthread_t t = pthread_self();

    if (key > t->keymax)
    {
        int keymax = (key + 1) * 2;
        void **kv = (void **)realloc(t->keyval, keymax * sizeof(void *));

        if (!kv) return ENOMEM;

        /* Clear new region */
        memset(&kv[t->keymax], 0, (keymax - t->keymax)*sizeof(void*));

        t->keyval = kv;
        t->keymax = keymax;
    }

    t->keyval[key] = (void *) value;

    return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

void pthread_tls_init(void)
{
    _pthread_tls = TlsAlloc();

    /* Cannot continue if out of indexes */
    if (_pthread_tls == TLS_OUT_OF_INDEXES) abort();
}

void _pthread_cleanup_dest(pthread_t t)
{
    unsigned int i, j;

    for (j = 0; j < PTHREAD_DESTRUCTOR_ITERATIONS; j++)
    {
        int flag = 0;

        for (i = 0; i < t->keymax; i++)
        {
            void *val = t->keyval[i];

            if (val)
            {
                pthread_rwlock_rdlock(&_pthread_key_lock);
                if ((uintptr_t) _pthread_key_dest[i] > 1)
                {
                    /* Call destructor */
                    t->keyval[i] = NULL;
                    _pthread_key_dest[i](val);
                    flag = 1;
                }
                pthread_rwlock_unlock(&_pthread_key_lock);
            }
        }

        /* Nothing to do? */
        if (!flag) return;
    }
}

pthread_t pthread_self(void)
{
    pthread_t t;

    _pthread_once_raw(&_pthread_tls_once, pthread_tls_init);

    t = (pthread_t)TlsGetValue(_pthread_tls);

    /* Main thread? */
    if (!t)
    {
        t = (pthread_t)malloc(sizeof(struct _pthread_v));

        /* If cannot initialize main thread, then the only thing we can do is abort */
        if (!t) abort();

        t->ret_arg = NULL;
        t->func = NULL;
        t->clean = NULL;
        t->cancelled = 0;
        t->p_state = PTHREAD_DEFAULT_ATTR;
        t->keymax = 0;
        t->keyval = NULL;
        t->h = GetCurrentThread();
        t->tid = GetCurrentThreadId();

        /* Save for later */
        TlsSetValue(_pthread_tls, t);

        if (setjmp(t->jb))
        {
            /* Make sure we free ourselves if we are detached */
            if (!t->h) free(t);

            /* Time to die */
            _endthreadex(0);
        }
    }

    return t;
}

int pthread_get_concurrency(int *val)
{
    *val = _pthread_concur;
    return 0;
}

int pthread_set_concurrency(int val)
{
    _pthread_concur = val;
    return 0;
}

int pthread_exit(void *res)
{
    pthread_t t = pthread_self();

    t->ret_arg = res;

    _pthread_cleanup_dest(t);

    longjmp(t->jb, 1);
}

void _pthread_invoke_cancel(void)
{
    _pthread_cleanup *pcup;

    InterlockedDecrement(&_pthread_cancelling);

    /* Call cancel queue */
    for (pcup = pthread_self()->clean; pcup; pcup = pcup->next)
    {
        pcup->func((pthread_once_t *)pcup->arg);
    }

    pthread_exit(PTHREAD_CANCELED);
}

void pthread_testcancel(void)
{
    if (_pthread_cancelling)
    {
        pthread_t t = pthread_self();

        if (t->cancelled && (t->p_state & PTHREAD_CANCEL_ENABLE))
        {
            _pthread_invoke_cancel();
        }
    }
}


int pthread_cancel(pthread_t t)
{
    CHECK_OBJECT(t, ESRCH);

    if (t->p_state & PTHREAD_CANCEL_ASYNCHRONOUS)
    {
        /* Dangerous asynchronous cancelling */
        CONTEXT ctxt;

        /* Already done? */
        if (t->cancelled) return ESRCH;

        ctxt.ContextFlags = CONTEXT_CONTROL;

        SuspendThread(t->h);
        GetThreadContext(t->h, &ctxt);
#ifdef _M_X64
        ctxt.Rip = (uintptr_t) _pthread_invoke_cancel;
#else
        ctxt.Eip = (uintptr_t) _pthread_invoke_cancel;
#endif
        SetThreadContext(t->h, &ctxt);

        /* Also try deferred Cancelling */
        t->cancelled = 1;

        /* Notify everyone to look */
        InterlockedIncrement(&_pthread_cancelling);

        ResumeThread(t->h);
    }
    else
    {
        /* Safe deferred Cancelling */
        t->cancelled = 1;

        /* Notify everyone to look */
        InterlockedIncrement(&_pthread_cancelling);
    }

    return 0;
}

/* half-stubbed version as we don't really well support signals */
int pthread_kill(pthread_t t, int sig)
{
    CHECK_OBJECT(t, ESRCH);
	if (sig < SIGINT || sig > NSIG) return EINVAL;
	return pthread_cancel(t);
}

unsigned _pthread_get_state(pthread_attr_t *attr, unsigned flag)
{
    return attr->p_state & flag;
}

int _pthread_set_state(pthread_attr_t *attr, unsigned flag, unsigned val)
{
    if (~flag & val) return EINVAL;
    attr->p_state &= ~flag;
    attr->p_state |= val;

    return 0;
}

int pthread_attr_init(pthread_attr_t *attr)
{
    attr->p_state = PTHREAD_DEFAULT_ATTR;
    attr->stack = NULL;
    attr->s_size = 0;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    /* No need to do anything */
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *a, int flag)
{
    return _pthread_set_state(a, PTHREAD_CREATE_DETACHED, flag);
}

int pthread_attr_getdetachstate(pthread_attr_t *a, int *flag)
{
    *flag = _pthread_get_state(a, PTHREAD_CREATE_DETACHED);
    return 0;
}

int pthread_attr_setinheritsched(pthread_attr_t *a, int flag)
{
    return _pthread_set_state(a, PTHREAD_INHERIT_SCHED, flag);
}

int pthread_attr_getinheritsched(pthread_attr_t *a, int *flag)
{
    *flag = _pthread_get_state(a, PTHREAD_INHERIT_SCHED);
    return 0;
}

int pthread_attr_setscope(pthread_attr_t *a, int flag)
{
    return _pthread_set_state(a, PTHREAD_SCOPE_SYSTEM, flag);
}

int pthread_attr_getscope(pthread_attr_t *a, int *flag)
{
    *flag = _pthread_get_state(a, PTHREAD_SCOPE_SYSTEM);
    return 0;
}

int pthread_attr_getstackaddr(pthread_attr_t *attr, void **stack)
{
    *stack = attr->stack;
    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stack)
{
    attr->stack = stack;
    return 0;
}

int pthread_attr_getstacksize(pthread_attr_t *attr, size_t *size)
{
    *size = attr->s_size;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size)
{
    attr->s_size = size;
    return 0;
}

int pthread_setcancelstate(int state, int *oldstate)
{
    pthread_t t = pthread_self();

    if ((state & PTHREAD_CANCEL_ENABLE) != state) return EINVAL;
    if (oldstate) *oldstate = t->p_state & PTHREAD_CANCEL_ENABLE;
    t->p_state &= ~PTHREAD_CANCEL_ENABLE;
    t->p_state |= state;

    return 0;
}

int pthread_setcanceltype(int type, int *oldtype)
{
    pthread_t t = pthread_self();

    if ((type & PTHREAD_CANCEL_ASYNCHRONOUS) != type) return EINVAL;
    if (oldtype) *oldtype = t->p_state & PTHREAD_CANCEL_ASYNCHRONOUS;
    t->p_state &= ~PTHREAD_CANCEL_ASYNCHRONOUS;
    t->p_state |= type;

    return 0;
}

int pthread_create_wrapper(void *args)
{
    struct _pthread_v *tv = (struct _pthread_v *)args;

    _pthread_once_raw(&_pthread_tls_once, pthread_tls_init);

    TlsSetValue(_pthread_tls, tv);
    tv->tid = GetCurrentThreadId();

    if (!setjmp(tv->jb))
    {
        /* Call function and save return value */
        tv->ret_arg = tv->func(tv->ret_arg);

        /* Clean up destructors */
        _pthread_cleanup_dest(tv);
    }

    /* If we exit too early, then we can race with create */
    while (tv->h == (HANDLE) -1)
    {
        YieldProcessor();
        _ReadWriteBarrier();
    }

    /* Make sure we free ourselves if we are detached */
    if (!tv->h) free(tv);

    return 0;
}

int pthread_create(pthread_t *th, pthread_attr_t *attr, void *(* func)(void *), void *arg)
{
	struct _pthread_v *tv = (struct _pthread_v *)malloc(sizeof(struct _pthread_v));
    size_t ssize = 0;

    CHECK_PTR(th);

    if (!tv) return EAGAIN;

    *th = tv;

    /* Save data in pthread_t */
    tv->ret_arg = arg;
    tv->func = func;
    tv->clean = NULL;
    tv->cancelled = 0;
    tv->p_state = PTHREAD_DEFAULT_ATTR;
    tv->keymax = 0;
    tv->keyval = NULL;
    tv->h = (HANDLE) -1;
    tv->tid = 0;

    if (attr)
    {
        tv->p_state = attr->p_state;
        ssize = attr->s_size;
    }

    /* Make sure tv->h has value of -1 */
    _ReadWriteBarrier();

    tv->h = (HANDLE) _beginthreadex(NULL, ssize, (unsigned int (__stdcall *)(void *))pthread_create_wrapper, tv, 0, NULL);

    /* Failed */
    if (!tv->h) return 1;


    if (tv->p_state & PTHREAD_CREATE_DETACHED)
    {
        CloseHandle(tv->h);
        _ReadWriteBarrier();
        tv->h = 0;
    }

    return 0;
}

int pthread_join(pthread_t t, void **res)
{
    struct _pthread_v *tv = t;

    CHECK_THREAD(tv);
    if (pthread_equal(pthread_self(), t)) return EDEADLK;

    pthread_testcancel();

    WaitForSingleObject(tv->h, INFINITE);
    CloseHandle(tv->h);

    /* Obtain return value */
    if (res) *res = tv->ret_arg;

    free(tv);

    return 0;
}

int _pthread_tryjoin(pthread_t t, void **res)
{
    struct _pthread_v *tv = t;

    CHECK_THREAD(tv);
    if (pthread_equal(pthread_self(), t)) return EDEADLK;

    pthread_testcancel();

    if(WaitForSingleObject(tv->h, 0))return EBUSY;
    CloseHandle(tv->h);

    /* Obtain return value */
    if (res) *res = tv->ret_arg;

    free(tv);

    return 0;
}

int pthread_detach(pthread_t t)
{
    struct _pthread_v *tv = t;

    /*
    * This can't race with thread exit because
    * our call would be undefined if called on a dead thread.
    */

    CHECK_THREAD(tv);
    CloseHandle(tv->h);
    _ReadWriteBarrier();
    tv->h = 0;

    return 0;
}