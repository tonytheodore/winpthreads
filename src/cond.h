#ifndef WIN_PTHREADS_COND_H
#define WIN_PTHREADS_COND_H

#include <windows.h>

#define CHECK_COND(c)  { \
    if (!(c) || !*c || (*c == PTHREAD_COND_INITIALIZER) \
        || ( ((cond_t *)(*c))->valid != (unsigned int)LIFE_COND ) ) \
        return EINVAL; }

#define LIFE_COND 0xC0BAB1FD
#define DEAD_COND 0xC0DEADBF

#if defined USE_COND_ConditionVariable
#include "rwlock.h"
#ifndef CONDITION_VARIABLE_INIT
typedef struct _RTL_CONDITION_VARIABLE {                    
        PVOID Ptr;                                       
} RTL_CONDITION_VARIABLE, *PRTL_CONDITION_VARIABLE;      
#define RTL_CONDITION_VARIABLE_INIT {0}                 
#define RTL_CONDITION_VARIABLE_LOCKMODE_SHARED  0x1     

#define CONDITION_VARIABLE_INIT RTL_CONDITION_VARIABLE_INIT
#define CONDITION_VARIABLE_LOCKMODE_SHARED RTL_CONDITION_VARIABLE_LOCKMODE_SHARED

typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;

WINBASEAPI VOID WINAPI InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI VOID WINAPI WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI VOID WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI BOOL WINAPI SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable,PCRITICAL_SECTION CriticalSection,DWORD dwMilliseconds);
WINBASEAPI BOOL WINAPI SleepConditionVariableSRW(PCONDITION_VARIABLE ConditionVariable,PSRWLOCK SRWLock,DWORD dwMilliseconds,ULONG Flags);
#endif /* CONDITION_VARIABLE_INIT */

#endif /* USE_COND_ConditionVariable */

#define STATIC_COND_INITIALIZER(x)		((pthread_cond_t)(x) == ((pthread_cond_t)PTHREAD_COND_INITIALIZER))

typedef struct cond_t cond_t;
struct cond_t
{
    unsigned int valid;   
    int busy;
    LONG waiters_count_; /* Number of waiting threads.  */
    LONG waiters_count_unblock_; /* Number of waiting threads whitch can be unblocked.  */
    LONG waiters_count_gone_; /* Number of waiters which are gone.  */
#if defined USE_COND_ConditionVariable
    CRITICAL_SECTION waiters_count_lock_; /* Serialize access to <waiters_count_>.  */
    CONDITION_VARIABLE CV;

#else /* USE_COND_Semaphore */
    CRITICAL_SECTION waiters_count_lock_; /* Serialize access to <waiters_count_>.  */
    CRITICAL_SECTION waiters_q_lock_; /* Serialize access to sema_q.  */
    LONG value_q;
    CRITICAL_SECTION waiters_b_lock_; /* Serialize access to sema_b.  */
    LONG value_b;
    HANDLE sema_q; /* Semaphore used to queue up threads waiting for the condition to
                 become signaled.  */
    HANDLE sema_b; /* Semaphore used to queue up threads waiting for the condition which
                 became signaled.  */
#endif
};

void cond_print_set(int state, FILE *f);

void cond_print(volatile pthread_cond_t *c, char *txt);

#endif
