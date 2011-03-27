#ifndef WIN_PTHREADS_MUTEX_H
#define WIN_PTHREADS_MUTEX_H

#define COND_LOCKED(m)	(m->owner != 0)
#define COND_OWNER(m)	(m->owner == GetCurrentThreadId())
#define COND_DEADLK(m)	COND_OWNER(m)
#define GET_OWNER(m)	(m->owner)
#define GET_HANDLE(m)	(m->h)
#define GET_LOCKCNT(m)	(m->count)
#define GET_RCNT(m)	(m->count) /* not accurate! */
#define SET_OWNER(m)	(m->owner = GetCurrentThreadId())
#define UNSET_OWNER(m)	{ m->owner = 0; }
#define LOCK_UNDO(m)
#define COND_DEADLK_NR(m)	((m->type != PTHREAD_MUTEX_RECURSIVE) && COND_DEADLK(m))
#define CHECK_DEADLK(m)		{ if (COND_DEADLK_NR(m)) return EDEADLK; }

#define STATIC_INITIALIZER(x)		((intptr_t)(x) >= -3 && (intptr_t)(x) <= -1)
#define MUTEX_INITIALIZER2TYPE(x)	((LONGBAG)PTHREAD_NORMAL_MUTEX_INITIALIZER - (LONGBAG)(x))

#define LIFE_MUTEX 0xBAB1F00D
#define DEAD_MUTEX 0xDEADBEEF

typedef struct mutex_t mutex_t;
struct mutex_t
{
    LONG valid;   
    volatile LONG busy;   
    int type;
    volatile LONG count;
    LONG lockOwner;
    DWORD owner;
    HANDLE h;
};

void mutex_print(volatile pthread_mutex_t *m, char *txt);
void mutex_print_set(int state);

#endif
