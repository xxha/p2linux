/*****************************************************************************
 * task.c - defines the wrapper functions and data structures needed
 *          to implement a Wind River pSOS+ (R) task control API 
 *          in a POSIX Threads environment.
 ****************************************************************************/

#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "p2pthread.h"

#undef DIAG_PRINTFS

#define T_NOPREEMPT  0x01
#define T_TSLICE     0x02

#define ERR_TIMEOUT  0x01
#define ERR_NODENO   0x04
#define ERR_OBJDEL   0x05
#define ERR_OBJTFULL 0x08
#define ERR_OBJNF    0x09

#define ERR_PRIOR    0x11
#define ERR_ACTIVE   0x12
#define ERR_SUSP     0x14
#define ERR_NOTSUSP  0x15
#define ERR_REGNUM   0x17

/*
**  user_sysroot is a user-defined function.  It contains all initialization
**               calls to create any tasks and other objects reqired for
**               startup of the user's RTOS system environment.  It is called
**               from (and runs in) the system initialization pthread context.
**               It may optionally wait for some condition, shut down the
**               user's RTOS system environment, clean up the resources used
**               by the various RTOS objects, and return to the initialization
**               pthread.  The system initialization pthread will then
**               terminate, as will the parent process.
*/
// extern void user_sysroot( void );

/*****************************************************************************
**  p2pthread Global Data Structures
*****************************************************************************/

/*
**  task_list is a linked list of pthread task control blocks.
**            It is used to perform en-masse operations on all p2pthread
**            tasks at once.
*/
static p2pthread_cb_t *
    task_list = (p2pthread_cb_t *)NULL;

/*
**  task_list_lock is a mutex used to serialize access to the task list
*/
static pthread_mutex_t
    task_list_lock = PTHREAD_MUTEX_INITIALIZER;

/*
**  p2pt_sched_lock is a mutex used to make sched_lock exclusive to one thread
**                  at a time.
*/
pthread_mutex_t
    p2pt_sched_lock = PTHREAD_MUTEX_INITIALIZER;

/*
**  scheduler_locked contains the pthread ID of the thread which currently
**                   has the scheduler locked (or NULL if it is unlocked).
*/
static pthread_t
    scheduler_locked = (pthread_t)NULL;

/*
**  sched_lock_level tracks recursive nesting levels of sched_lock/unlock calls
**                   so the scheduler is only unlocked at the outermost
**                   sched_unlock call.
*/
static unsigned long
    sched_lock_level = 0;

/*
**  sched_lock_change is a condition variable which signals a change from
**                    locked to unlocked or vice-versa.
*/
static pthread_cond_t
    sched_lock_change = PTHREAD_COND_INITIALIZER;

/*****************************************************************************
**  thread-safe malloc
*****************************************************************************/
void *ts_malloc( size_t blksize )
{
    void *blkaddr;
    static pthread_mutex_t
        malloc_lock = PTHREAD_MUTEX_INITIALIZER;

    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&malloc_lock );
    pthread_mutex_lock( &malloc_lock );

    blkaddr = malloc( blksize );

    pthread_mutex_unlock( &malloc_lock );
    pthread_cleanup_pop( 0 );

    return( blkaddr );
}
    
/*****************************************************************************
**  thread-safe free
*****************************************************************************/
void ts_free( void *blkaddr )

{
    static pthread_mutex_t
        free_lock = PTHREAD_MUTEX_INITIALIZER;

    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&free_lock );
    pthread_mutex_lock( &free_lock );

    free( blkaddr );

    pthread_mutex_unlock( &free_lock );
    pthread_cleanup_pop( 0 );
}
    
/*****************************************************************************
**  my_tcb - returns a pointer to the task control block for the calling task
*****************************************************************************/
p2pthread_cb_t *
   my_tcb( void )
{
    pthread_t my_pthrid;
    p2pthread_cb_t *current_tcb;

    /*
    **  Get caller's pthread ID
    */
    my_pthrid = pthread_self();

    /*
    **  If the task_list contains tasks, scan it for the tcb
    **  whose thread id matches the one to be deleted.  No locking
    **  of the task list is done here since the access is read-only.
    **  NOTE that a tcb being appended to the task_list MUST have its
    **  nxt_task member initialized to NULL before being linked into
    **  the list. 
    */
    if ( task_list != (p2pthread_cb_t *)NULL )
    {
        for ( current_tcb = task_list;
              current_tcb != (p2pthread_cb_t *)NULL;
              current_tcb = current_tcb->nxt_task )
        {
            if ( my_pthrid == current_tcb->pthrid )
            {
                /*
                **  Found the task control_block.
                */
                return( current_tcb );
            }
        }
    }

    /*
    **  No matching task found... return NULL
    */
    return( (p2pthread_cb_t *)NULL );
}

/*****************************************************************************
** tcb_for - returns the address of the task control block for the task
**           idenified by taskid
*****************************************************************************/
p2pthread_cb_t *
   tcb_for( ULONG taskid )
{
    p2pthread_cb_t *current_tcb;
    int found_taskid;

        if ( task_list != (p2pthread_cb_t *)NULL )
        {
            /*
            **  One or more tasks already exist in the task list...
            **  Scan the existing tasks for a matching ID.
            */
            found_taskid = FALSE;
            for ( current_tcb = task_list; 
                  current_tcb != (p2pthread_cb_t *)NULL;
                  current_tcb = current_tcb->nxt_task )
            {
                if ( current_tcb->taskid == taskid )
                {
                    found_taskid = TRUE;
                    break;
                }
            }
            if ( found_taskid == FALSE )
                /*
                **  No matching ID found
                */
                current_tcb = (p2pthread_cb_t *)NULL;
        }
        else
            current_tcb = (p2pthread_cb_t *)NULL;
 
    return( current_tcb );
}

/*****************************************************************************
** sched_lock - 'locks the scheduler' to prevent preemption of the current task
**           by other task-level code.  Because we cannot actually lock the
**           scheduler in a pthreads environment, we temporarily set the
**           dynamic priority of the calling thread above that of any other
**           thread, thus guaranteeing that no other tasks preempt it.
*****************************************************************************/
void
   sched_lock( void )
{
    pthread_t my_pthrid;
    p2pthread_cb_t *tcb;
    int max_priority, sched_policy, got_lock;

    /*
    **  p2pt_sched_lock ensures that only one p2pthread pthread at a time gets
    **  to run at max_priority (effectively locking out all other p2pthread
    **  pthreads).  Due to the semantics of the pthread_cleanup push/pop
    **  pairs (which protect against deadlocks in the event a thread gets
    **  terminated while holding the mutex lock), we cannot safely leave
    **  the mutex itself locked until sched_unlock() is called.  Therefore,
    **  we instead use the mutex to provide 'atomic access' to a global
    **  flag indicating if the scheduler is currently locked.  We will
    **  'spin' and briefly suspend until the scheduler is unlocked, and
    **  will then lock it ourselves before proceeding.
    */
    got_lock = FALSE;
    my_pthrid = pthread_self();

    /*
    **  'Spin' here until scheduler_locked == NULL or our pthread ID
    **  This effectively prevents more than one pthread at a time from
    **  setting its priority to max_priority.
    */
    do {
        /*
        **  The pthread_cleanup_push/pop pair ensure the mutex will be
        **  unlocked if the calling thread gets killed within this loop.
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&p2pt_sched_lock );
        /*
        **  The mutex lock/unlock guarantees 'atomic' access to the
        **  scheduler_locked flag.  Locking via pthread ID allows recursive
        **  locking by the same pthread while excluding all other pthreads.
        */
        pthread_mutex_lock( &p2pt_sched_lock );
        if ( (scheduler_locked == (pthread_t)NULL) ||
             (scheduler_locked == my_pthrid) )
        {
            scheduler_locked = my_pthrid;
            sched_lock_level++;
			/* 
			**  Note: since the type of 'sched_lock_level is defined as
			**	'unsigned', so the condition below is only occur when 
			**  sched_lock_level exceed it's maxium value. It's a bug 
			**  although it is right in most of time.
            */
            if ( sched_lock_level == 0L )
                sched_lock_level--;
            got_lock = TRUE;
			/* 
			**  Note: i think maybe the statement below is useless.
			*/
			pthread_cond_broadcast( &sched_lock_change );
#ifdef DIAG_PRINTFS 
            printf( "\r\nsched_lock sched_lock_level %lu locking tid %ld",
                sched_lock_level,  scheduler_locked );
#endif
        }
        else
        {
#ifdef DIAG_PRINTFS 
            printf( "\r\nsched_lock locking tid %ld my tid %ld",
                    scheduler_locked, my_pthrid );
#endif
            pthread_cond_wait( &sched_lock_change, &p2pt_sched_lock );
        }
        pthread_mutex_unlock( &p2pt_sched_lock );

        /*
        **  Add a cancellation point to this loop, since there are no others.
        */
        pthread_testcancel();
        pthread_cleanup_pop( 0 );
    } while ( got_lock == FALSE );

    /*
    **  task_list_lock prevents other p2pthread pthreads from modifying
    **  the p2pthread pthread task list while we're searching it and modifying
    **  the calling task's priority level.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&task_list_lock );
    pthread_mutex_lock( &task_list_lock );
    tcb = my_tcb();
    if ( tcb != (p2pthread_cb_t *)NULL )
    {
        pthread_attr_getschedpolicy( &(tcb->attr), &sched_policy );
        max_priority = sched_get_priority_max( sched_policy );
        ((tcb->attr).__schedparam).sched_priority = max_priority;
		/*
		** Note: here we set the priority for the given pthread.
		*/
        pthread_setschedparam( tcb->pthrid, sched_policy,
                          (struct sched_param *)&((tcb->attr).__schedparam) );
    }
    pthread_mutex_unlock( &task_list_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** sched_unlock - 'unlocks the scheduler' to allow preemption of the current
**             task by other task-level code.  Because we cannot actually lock
**             the scheduler in a pthreads environment, the dynamic priority of
**             the calling thread was temporarily raised above that of any
**             other thread.  Therefore, we now restore the priority of the
**             calling thread to its original value to 'unlock' the task
**             scheduler.
*****************************************************************************/
void
   sched_unlock( void )
{
    p2pthread_cb_t *tcb;
    int sched_policy;

    /*
    **  scheduler_locked ensures that only one p2pthread pthread at a time gets
    **  to run at max_priority (effectively locking out all other p2pthread
    **  pthreads).  Unlock it here to complete 'unlocking' of the scheduler.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&p2pt_sched_lock );
    pthread_mutex_lock( &p2pt_sched_lock );

    if ( scheduler_locked == pthread_self() )
    {
        if ( sched_lock_level > 0L )
            sched_lock_level--;
        if ( sched_lock_level < 1L )
        {
            /*
            **  task_list_lock prevents other p2pthread pthreads from modifying
            **  the p2pthread pthread task list while we're searching it and
            **  modifying the calling task's priority level.
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&task_list_lock );
            pthread_mutex_lock( &task_list_lock );
            tcb = my_tcb();
            if ( tcb != (p2pthread_cb_t *)NULL )
            {
                pthread_attr_getschedpolicy( &(tcb->attr), &sched_policy );
                ((tcb->attr).__schedparam).sched_priority = 
                                           tcb->prv_priority.sched_priority;
                pthread_setschedparam( tcb->pthrid, sched_policy,
                          (struct sched_param *)&((tcb->attr).__schedparam) );
            }
            pthread_mutex_unlock( &task_list_lock );
            pthread_cleanup_pop( 0 );

            scheduler_locked = (pthread_t)NULL;
            pthread_cond_broadcast( &sched_lock_change );
        }
#ifdef DIAG_PRINTFS 
        printf( "\r\nsched_unlock sched_lock_level %lu locking tid %ld",
                sched_lock_level,  scheduler_locked );
#endif
    }
#ifdef DIAG_PRINTFS 
    else
        printf( "\r\nsched_unlock locking tid %ld my tid %ld", scheduler_locked,
                pthread_self() );
#endif

    pthread_mutex_unlock( &p2pt_sched_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** link_susp_tcb - appends a new tcb pointer to a linked list of tcb pointers
**                 for tasks suspended on the object owning the list.
*****************************************************************************/
void
   link_susp_tcb( p2pthread_cb_t **list_head, p2pthread_cb_t *new_entry )
{
    p2pthread_cb_t *nxt_entry;

    if ( list_head != (p2pthread_cb_t **)NULL )
    {
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&task_list_lock );
        pthread_mutex_lock( &task_list_lock );
        new_entry->nxt_susp = (p2pthread_cb_t *)NULL;
        if ( *list_head != (p2pthread_cb_t *)NULL )
        {
            for ( nxt_entry = *list_head; 
                  nxt_entry->nxt_susp != (p2pthread_cb_t *)NULL;
                  nxt_entry = nxt_entry->nxt_susp ) ;
            nxt_entry->nxt_susp = new_entry;
#ifdef DIAG_PRINTFS 
            printf( "\r\nadd susp_tcb @ %p to list @ %p", new_entry,
                    nxt_entry );
#endif
        }
        else
        {
            *list_head = new_entry;
#ifdef DIAG_PRINTFS 
            printf( "\r\nadd susp_tcb @ %p to list @ %p", new_entry,
                    list_head );
#endif
        }
        /*
        **  Initialize the suspended task's pointer back to suspend list
        **  This is used for cleanup during task deletion by t_delete().
        */
        new_entry->suspend_list = list_head;

        pthread_mutex_unlock( &task_list_lock );
        pthread_cleanup_pop( 0 );
    }
}

/*****************************************************************************
** unlink_susp_tcb - removes tcb pointer from a linked list of tcb pointers
**                   for tasks suspended on the object owning the list.
*****************************************************************************/
void
   unlink_susp_tcb( p2pthread_cb_t **list_head, p2pthread_cb_t *entry )
{
    p2pthread_cb_t *current_tcb;

    if ( list_head != (p2pthread_cb_t **)NULL )
    {
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&task_list_lock );
        pthread_mutex_lock( &task_list_lock );
        if ( *list_head == entry )
        {
            *list_head = entry->nxt_susp;
#ifdef DIAG_PRINTFS 
            printf( "\r\ndel susp_tcb @ %p from list @ %p - newlist head %p",
                    entry, list_head, *list_head );
#endif
        }
        else
        {
            for ( current_tcb = *list_head;
                  current_tcb != (p2pthread_cb_t *)NULL;
                  current_tcb = current_tcb->nxt_susp )
            {
                if ( current_tcb->nxt_susp == entry )
                {
                    current_tcb->nxt_susp = entry->nxt_susp;
#ifdef DIAG_PRINTFS 
                    printf( "\r\ndel susp_tcb @ %p from list @ %p", entry,
                    current_tcb );
#endif
                }
            }
        }
        entry->nxt_susp = (p2pthread_cb_t *)NULL;
        pthread_mutex_unlock( &task_list_lock );
        pthread_cleanup_pop( 0 );
    }

}

/*****************************************************************************
** signal_for_my_task - searches the specified 'pended task list' for the
**                      task to be selected according to the specified
**                      pend order.  If the selected task is the currently
**                      executing task, the task is deleted from the
**                      specified pended task list and returns a non-zero
**                      result... otherwise the pended task list is not
**                      modified and a zero result is returned.
*****************************************************************************/
int
   signal_for_my_task( p2pthread_cb_t **list_head, int pend_order )
{
    p2pthread_cb_t *signalled_task;
    p2pthread_cb_t *current_tcb;
    int result;

    result = FALSE;
#ifdef DIAG_PRINTFS 
    printf( "\r\nsignal_for_my_task - list head = %p", *list_head );
#endif
    if ( list_head != (p2pthread_cb_t **)NULL )
    {
        signalled_task = *list_head;

        /*
        **  First determine which task is being signalled
        */
        if ( pend_order != 0 )
        {
            /*
            **  Tasks pend in priority order... locate the highest priority
            **  task in the pended list.
            */
            for ( current_tcb = *list_head;
                  current_tcb != (p2pthread_cb_t *)NULL;
                  current_tcb = current_tcb->nxt_susp )
            {
                if ( (current_tcb->prv_priority).sched_priority >
                     (signalled_task->prv_priority).sched_priority )
                    signalled_task = current_tcb;
#ifdef DIAG_PRINTFS 
                printf( "\r\nsignal_for_my_task - tcb @ %p priority %d",
                        current_tcb,
                        (current_tcb->prv_priority).sched_priority );
#endif
            }
        } /*
        else
            **
            ** Tasks pend in FIFO order... signal is for task at list head.
            */

        /*
        **  Signalled task located... see if it's the currently executing task.
        */
        if ( signalled_task == my_tcb() )
        {
            /*
            **  The currently executing task is being signalled...
            */
            result = TRUE;
        }
#ifdef DIAG_PRINTFS 
        printf( "\r\nsignal_for_my_task - signalled tcb @ %p my tcb @ %p",
                        signalled_task, my_tcb() );
#endif
    }

    return( result );
}

/*****************************************************************************
** new_tid - assigns the next unused task ID for the caller's task
*****************************************************************************/
static ULONG
   new_tid( void )
{
    p2pthread_cb_t *current_tcb;
    ULONG new_taskid;
    
	/*
	**  Note: since we only read the task_list, so no locking of 
	**  task_list_lock is needed, and you can't only go to the 
	**  last member in task_list to get it's task_id and plus 1.
	*/

    /*
    **  Get the highest previously assigned task id and add one.
    */
    if ( task_list != (p2pthread_cb_t *)NULL )
    {
        /*
        **  One or more tasks already exist in the task list...
        **  Find the highest task ID number in the existing list.
        */
        new_taskid = task_list->taskid;
        for ( current_tcb = task_list; 
              current_tcb->nxt_task != (p2pthread_cb_t *)NULL;
              current_tcb = current_tcb->nxt_task )
        {
            if ( (current_tcb->nxt_task)->taskid > new_taskid )
            {
                new_taskid = (current_tcb->nxt_task)->taskid;
            }
        }

        /*
        **  Add one to the highest existing task ID
        */
        new_taskid++;
    }
    else
    {
        /*
        **  this is the first task being added to the task list.
        */
        new_taskid = 1;
    }

    return( new_taskid );
}

/*****************************************************************************
** translate_priority - translates a p2pthread priority into a pthreads priority
*****************************************************************************/
static int
   translate_priority( ULONG p2pt_priority, int sched_policy, ULONG *errp )
{
    int max_priority, min_priority, pthread_priority;

    /*
    **  Validate the range of the user's task priority.
    */
    if ( (p2pt_priority < MIN_P2PT_PRIORITY) | 
         (p2pt_priority > MAX_P2PT_PRIORITY) )
        *errp = ERR_PRIOR;
 
    /*
    **  Translate the p2pthread priority into a pthreads priority.
    */
    pthread_priority = (int)p2pt_priority;

    /*
    **  Next get the allowable priority range for the scheduling policy.
    */
    min_priority = sched_get_priority_min( sched_policy );
    max_priority = sched_get_priority_max( sched_policy );

    /*
    **  Now 'clip' the new priority level to within priority range.
    **  Reserve max_priority level for temporary use during system calls.
    **  NOTE that relative p2pthread priorities may not translate properly
    **  if the p2pthread priorities used span several multiples of max_priority.
    */
    pthread_priority %= max_priority;
    if ( pthread_priority < min_priority )
            pthread_priority = min_priority;

    return( pthread_priority );
}

/*****************************************************************************
** tcb_delete - deletes a pthread task control block from the task_list
**              and frees the memory allocated for the tcb
*****************************************************************************/
static void
   tcb_delete( p2pthread_cb_t *tcb )
{
    p2pthread_cb_t *current_tcb;

    /*
    **  If the task_list contains tasks, scan it for a link to the tcb
    **  being deleted.
    */
    if ( task_list != (p2pthread_cb_t *)NULL )
    {
        /*
        **  Remove the task from the suspend list for any object it
        **  is pending on.
        */
        unlink_susp_tcb( tcb->suspend_list, tcb );
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&task_list_lock );
        pthread_mutex_lock( &task_list_lock );
        if ( tcb == task_list )
        {
            task_list = tcb->nxt_task;
        }
        else
        {
            for ( current_tcb = task_list;
                  current_tcb->nxt_task != (p2pthread_cb_t *)NULL;
                  current_tcb = current_tcb->nxt_task )
            {
                if ( tcb == current_tcb->nxt_task )
                {
                    /*
                    **  Found the tcb just prior to the one being deleted.
                    **  Unlink the tcb being deleted from the task_list.
                    */
                    current_tcb->nxt_task = tcb->nxt_task;
                    break;
                }
            }
        }
        pthread_mutex_unlock( &task_list_lock );
        pthread_cleanup_pop( 0 );
    }

    /* Release the memory occupied by the tcb being deleted. */
    ts_free( (void *)tcb );
}

/*****************************************************************************
** t_delete - removes the specified task(s) from the task list,
**              frees the memory occupied by the task control block(s),
**              and kills the pthread(s) associated with the task(s).
*****************************************************************************/
ULONG
   t_delete( ULONG tid )
{
    p2pthread_cb_t *current_tcb;
    p2pthread_cb_t *self_tcb;
    ULONG error;

    error = ERR_NO_ERROR;

    sched_lock();
    self_tcb = my_tcb();

    if ( tid == 0 )
    {
        /*
        **  Delete currently executing task... get its pthread ID
        */
        if ( self_tcb != (p2pthread_cb_t *)NULL )
        {
            /*
            **  Kill the currently executing task's pthread and 
            **  then de-allocate its data structures.
            */

			/*	POSIX threads can exist in either the ¡®joinable¡¯ state 
			**  or the ¡®detached¡¯ state. A given pthread or process 
			**  can wait for a joinable pthread to terminate. At this time,
			**  the process or pthread waiting on the terminating pthread 
			**  obtains an exit status from the pthread and then the terminating 
			**  pthread¡¯s resources are released. A ¡®detached¡¯pthread has 
			**  effectively been told that no other process or pthread cares when 
			**  it terminates. This means that when the ¡®detached¡¯ pthread 
			**  terminates, its resources are released immediately and no other 
			**  process or pthread can receive termination notice or an exit status. 
			**  If the pthread is deleting itself it must be ¡®detached¡¯ in order 
			**  to free its Linux resources upon termination.
			*/  
            pthread_detach( self_tcb->pthrid );
            pthread_cleanup_push( (void(*)(void *))tcb_delete,
                                  (void *)self_tcb );
            pthread_exit( ( void *)NULL );
			/*
			**  Because pthread_exit is also a termination point func,
			**  so the arg of pthread_cleanup_pop is 0, and it's still
			**  that the function tcb_delete() will be called.
			*/
            pthread_cleanup_pop( 0 );
        }
        else
            error = ERR_OBJDEL;
    }
    else
    {
        /*
        **  Delete the task whose taskid matches tid.
        **  If the task_list contains tasks, scan it for the tcb
        **  whose task id matches the one to be deleted.
        */
        current_tcb = tcb_for( tid );
        if ( current_tcb != (p2pthread_cb_t *)NULL )
        {
            /*
            **  Found the task being deleted... delete it.
            */
            if ( current_tcb != self_tcb )
            {
                /*
                **  Task being deleted is not the current task.
                **  Kill the task pthread and wait for it to die.
                **  Then de-allocate its data structures.
                */
                pthread_cancel( current_tcb->pthrid );
                pthread_join( current_tcb->pthrid, (void **)NULL );
                tcb_delete( current_tcb );
            }
            else
            {
                /*
                **  Kill the currently executing task's pthread
                **  and then de-allocate its data structures.
                */
                pthread_detach( self_tcb->pthrid );
                pthread_cleanup_push( (void(*)(void *))tcb_delete,
                                      (void *)self_tcb );
                pthread_exit( ( void *)NULL );
                pthread_cleanup_pop( 0 );
            }
        }
        else
            error = ERR_OBJDEL;
    } 
    sched_unlock();

    return( error );
}

/*****************************************************************************
**  cleanup_scheduler_lock ensures that a killed pthread releases the
**                         scheduler lock if it owned it.
*****************************************************************************/
static void 
    cleanup_scheduler_lock( void *tcb )
{
    p2pthread_cb_t *mytcb;

    mytcb = (p2pthread_cb_t *)tcb;
    pthread_mutex_lock( &p2pt_sched_lock );

    if ( scheduler_locked == pthread_self() )
    {
        sched_lock_level = 0;
        scheduler_locked = (pthread_t)NULL;
    }
    pthread_mutex_unlock( &p2pt_sched_lock );
}

/*****************************************************************************
**  task_wrapper is a pthread used to 'contain' a p2pthread task.
*****************************************************************************/
static void *
    task_wrapper( void *arg )
{
    p2pthread_cb_t *tcb;
    p2pthread_pb_t *parmblk;
    void (*task_ptr)( ULONG, ULONG, ULONG, ULONG );
    
    /*
    **  Make a parameter block pointer from the caller's argument
    **  Then extract the needed info from the parameter block and
    **  free its memory before beginning the p2pthread task
    */
    parmblk = (p2pthread_pb_t *)arg;
    tcb = parmblk->tcb;
    task_ptr = parmblk->task_ptr;

    /*
    **  Note: ensure that this pthread will release the scheduler lock if killed.
    */
    pthread_cleanup_push( cleanup_scheduler_lock, (void *)tcb );

    /*
    **  Call the p2pthread task.  Normally this is an endless loop and doesn't
    **  return here.
    */
#ifdef DIAG_PRINTFS 
    printf( "\r\ntask_wrapper starting task @ %p tcb @ %p:", task_ptr, tcb );
    sleep( 1 );
#endif
    (*task_ptr)( parmblk->parms[0], parmblk->parms[1], parmblk->parms[2],
                 parmblk->parms[3] );

    /*
    **  Note: here the arg is '1'.
    **  If for some reason the task above DOES return, clean up the 
    **  pthread and task resources and kill the pthread.
    */
    pthread_cleanup_pop( 1 );

    /*
    **  NOTE t_delete takes no action if the task has already been deleted.
    */
    t_delete( tcb->taskid );

    return( (void *)NULL );
}

/*****************************************************************************
** t_create - creates a pthread to contain the specified p2pthread task and
**               initializes the requisite data structures to support p2pthread 
**               task behavior not directly supported by Posix threads.
*****************************************************************************/
ULONG
    t_create( char name[4], ULONG pri, ULONG sstack, ULONG ustack, ULONG mode,
              ULONG *tid )
{
    p2pthread_cb_t *tcb;
    p2pthread_cb_t *current_tcb;
    int i, new_priority;
    ULONG error, my_tid;

    error = ERR_NO_ERROR;

    /*
    **  Establish task identifier
    */
    my_tid = new_tid();
    if ( tid != (ULONG *)NULL )
        *tid = my_tid;

    /* First allocate memory for a new pthread task control block */
    tcb = ts_malloc( sizeof( p2pthread_cb_t ) );
    if ( tcb != (p2pthread_cb_t *)NULL )
    {
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&task_list_lock );
        pthread_mutex_lock( &task_list_lock );

        /*
        **  Got a new task control block.  Initialize it.
        */
        tcb->pthrid = (pthread_t)NULL;
        tcb->taskid = my_tid;

        /*
        **  Copy the task name
        */
        for ( i = 0; i < 4; i++ )
            tcb->taskname[i] = name[i];

        /*
        **  Initialize the thread attributes to default values.
        **  Then modify the attributes to make a real-time thread.
        */
        pthread_attr_init( &(tcb->attr) );

        /*
        **  Get the default scheduling priority & init prv_priority member
        */
        pthread_attr_getschedparam( &(tcb->attr), &(tcb->prv_priority) );

        /*
        **  Translate the p2pthread priority into a pthreads priority
        **  and set the new scheduling priority.
        */
        pthread_attr_setschedpolicy( &(tcb->attr), SCHED_FIFO );
        new_priority = translate_priority( pri, SCHED_FIFO, &error );

        (tcb->prv_priority).sched_priority = new_priority;
        pthread_attr_setschedparam( &(tcb->attr), &(tcb->prv_priority) );

        /*
        ** 'Registers' for task
        */
        for ( i = 1; i < 8; i++ )
            tcb->registers[i] = (ULONG)NULL;

        /*
        ** Mutex and Condition variable for task events
        */
        pthread_mutex_init( &(tcb->event_lock), (pthread_mutexattr_t *)NULL );
        pthread_cond_init( &(tcb->event_change), (pthread_condattr_t *)NULL );

        /*
        ** Event state to awaken task (if suspended)
        */
        tcb->event_mask = (ULONG)NULL;

        /*
        ** Current state of captured event flags for task
        */
        tcb->events_captured = (ULONG)NULL;

        /*
        ** Current state of pending event flags for task
        */
        tcb->events_pending = (ULONG)NULL;

        /*
        **  The task is initially created in a suspended state
        */
        tcb->suspend_reason = WAIT_TSTRT;

        tcb->suspend_list = (p2pthread_cb_t **)NULL;
        tcb->nxt_susp = (p2pthread_cb_t *)NULL;
        tcb->nxt_task = (p2pthread_cb_t *)NULL;

        /*
        **  If everything's okay thus far, we have a valid TCB ready to go.
        */
        if ( error == ERR_NO_ERROR )
        {
            /*
            **  Insert the task control block into the task list.
            **  First see if the task list contains any tasks yet.
            */
            if ( task_list == (p2pthread_cb_t *)NULL )
            {
                task_list = tcb;
            }
            else
            {
                /*
                **  Append the new tcb to the task list.
                */
                for ( current_tcb = task_list;
                      current_tcb->nxt_task != (p2pthread_cb_t *)NULL;
                      current_tcb = current_tcb->nxt_task );
                current_tcb->nxt_task = tcb;
            }
        }
        else
        {
            /*
            **  OOPS! Something went wrong... clean up & exit.
            */
            ts_free( (void *)tcb );
        }
        pthread_mutex_unlock( &task_list_lock );
        pthread_cleanup_pop( 0 );
    }
    else /* malloc failed */
    {
       error = ERR_OBJTFULL;
    }

    return( error );
}

/*****************************************************************************
** t_start - creates a pthread to contain the specified p2pthread task and
**               initializes the requisite data structures to support p2pthread 
**               task behavior not directly supported by Posix threads.
*****************************************************************************/
ULONG
    t_start( ULONG tid, ULONG mode,
             void (*task)( ULONG, ULONG, ULONG, ULONG ), ULONG parms[4] )
{
    p2pthread_cb_t *tcb;
    p2pthread_pb_t *parmblk;
    int sched_policy;
    ULONG error;

    error = ERR_NO_ERROR;

    /*
    **  'Lock the p2pthread scheduler' to defer any context switch to a higher
    **  priority task until after this call has completed its work.
    */
    sched_lock();

    tcb = tcb_for( tid );
    if ( tcb != (p2pthread_cb_t *)NULL )
    {
        /*
        **  Found our task control block.
        **  Init the parameter block for the task wrapper function and
        **  start a new real-time pthread for the task. 
        */
        if ( tcb->suspend_reason == WAIT_TSTRT )
        {
            tcb->suspend_reason = WAIT_READY;
            tcb->entry_point = task;

            /*
            ** Mode flags for task
            */
            tcb->flags = mode;
            pthread_attr_init(&(tcb->attr));

            /*
            **  Determine whether round-robin time-slicing is to be used or not
            */
            if ( mode & T_TSLICE )
                sched_policy = SCHED_RR;
            else
                sched_policy = SCHED_FIFO;
            pthread_attr_setschedpolicy( &(tcb->attr), sched_policy );

            parmblk =
                  (p2pthread_pb_t *)ts_malloc( sizeof( p2pthread_pb_t ) );
            if ( parmblk != (p2pthread_pb_t *)NULL )
            {
                parmblk->tcb = tcb;
                parmblk->task_ptr = task;
                if ( parms != (ULONG *)NULL )
                {
                    parmblk->parms[0] = parms[0];
                    parmblk->parms[1] = parms[1];
                    parmblk->parms[2] = parms[2];
                    parmblk->parms[3] = parms[3];
                }
                else
                {
                    parmblk->parms[0] = (ULONG)NULL;
                    parmblk->parms[1] = (ULONG)NULL;
                    parmblk->parms[2] = (ULONG)NULL;
                    parmblk->parms[3] = (ULONG)NULL;
                }

#ifdef DIAG_PRINTFS 
                printf( "\r\nt_start task @ %p tcb @ %p:", task, tcb );
#endif

//                if ( pthread_create( &(tcb->pthrid), &(tcb->attr),
//                                     task_wrapper, (void *)parmblk ) != 0 )
                if ( pthread_create( &(tcb->pthrid), NULL,
                                     task_wrapper, (void *)parmblk ) != 0 )
                {
#ifdef DIAG_PRINTFS 
                    perror( "\r\nt_start pthread_create returned error:" );
#endif
                    error = ERR_OBJDEL;
                    tcb_delete( tcb );
                }
            }
            else
            {
#ifdef DIAG_PRINTFS 
                printf( "\r\nt_start unable to allocate parameter block" );
#endif
                error = ERR_OBJDEL;
                tcb_delete( tcb );
            }
        }
        else
        {
            /*
            ** task already made runnable
            */
            error = ERR_ACTIVE;
#ifdef DIAG_PRINTFS 
            printf( "\r\nt_start task @ tcb %p already active", tcb );
#endif
        }
    }
    else
        error = ERR_OBJDEL;

    /*
    **  'Unlock the p2pthread scheduler' to enable a possible context switch
    **  to a task made runnable by this call.
    */
    sched_unlock();
    return( error );
}

/*****************************************************************************
** t_suspend - suspends the specified p2pthread task
*****************************************************************************/
ULONG
    t_suspend( ULONG tid )
{
    p2pthread_cb_t *current_tcb;
    p2pthread_cb_t *self_tcb;
    ULONG error;

    error = ERR_NO_ERROR;

    self_tcb = my_tcb();

    if ( tid == 0 )
    {
        /*
        **  Suspend currently executing task... get its pthread ID
        */
        if ( self_tcb != (p2pthread_cb_t *)NULL )
        {
            /*
            **  Don't suspend if currently executing task has the
            **  scheduler locked!
            */
            pthread_mutex_lock( &p2pt_sched_lock );
            if ( scheduler_locked == pthread_self() )
            {
                if ( sched_lock_level < 1L )
                {
                    /*
                    **  Suspend the currently executing task's pthread
                    */
                    pthread_mutex_unlock( &p2pt_sched_lock );
                    self_tcb->suspend_reason = WAIT_TSUSP;
                    pthread_kill( self_tcb->pthrid, SIGSTOP );
                }
                else
				    /*
					**  Note: it seems that this condition did not suspend 
					**  the task and no errno returned.
					*/
                    pthread_mutex_unlock( &p2pt_sched_lock );
            }
            else
            {
                /*
                **  Suspend the currently executing task's pthread
                */
                pthread_mutex_unlock( &p2pt_sched_lock );
                self_tcb->suspend_reason = WAIT_TSUSP;
                pthread_kill( self_tcb->pthrid, SIGSTOP );
            }
        }
    }
    else
    {
        /*
        **  Suspend the task whose taskid matches tid.
        **  If the task_list contains tasks, scan it for the tcb
        **  whose task id matches the one to be suspended.
        */
        current_tcb = tcb_for( tid );
        if ( current_tcb != (p2pthread_cb_t *)NULL )
        {
            /*
            **  Suspend task if task not already suspended.
            */
            if ( current_tcb->suspend_reason != WAIT_TSUSP )
            {
			    /* 
			    ** set this pthread as the hignest priority, so 
			       that the tid task are not locked.
				*/
                sched_lock();
                /*
                **  Found the task being suspended... suspend it.
                */
                if ( current_tcb != self_tcb )
                {
                    /*
                    **  Task being suspended is not the current task.
                    */
                    current_tcb->suspend_reason = WAIT_TSUSP;
                    pthread_kill( current_tcb->pthrid, SIGSTOP );
                    sched_unlock();
                }
                else
                {
                    /*
                    **  Suspend the currently executing task's pthread
                    **  if it doesn't have the scheduler locked.
                    */
                    sched_unlock();
                    pthread_mutex_lock( &p2pt_sched_lock );
                    if ( scheduler_locked == pthread_self() )
                    {
                        pthread_mutex_unlock( &p2pt_sched_lock );
                    }
                    else
                    {
                        /*
                        **  Suspend the currently executing pthread
                        */
                        pthread_mutex_unlock( &p2pt_sched_lock );
                        self_tcb->suspend_reason = WAIT_TSUSP;
                        pthread_kill( self_tcb->pthrid, SIGSTOP );
                    }
                }
            }
            else
            {
                error = ERR_SUSP;
            }
        }
        else
            error = ERR_OBJDEL;
    }
 
    return( error );
}

/*****************************************************************************
** t_resume - resume the specified p2pthread task
*****************************************************************************/
ULONG
    t_resume( ULONG tid )
{
    p2pthread_cb_t *current_tcb;
    p2pthread_cb_t *self_tcb;
    ULONG error;

    error = ERR_NO_ERROR;

    sched_lock();
    self_tcb = my_tcb();

    /*
    **  Resume the task whose taskid matches tid.
    **  If the task_list contains tasks, scan it for the tcb
    **  whose task id matches the one to be resumed.
    */
    current_tcb = tcb_for( tid );
    if ( current_tcb != (p2pthread_cb_t *)NULL )
    {
        /*
        **  Make task runnable if task still suspended.
        */
        if ( current_tcb->suspend_reason == WAIT_TSUSP )
        {
            /*
            **  Found the task being resumed... resume it.
            */
            current_tcb->suspend_reason = WAIT_READY;
            pthread_kill( current_tcb->pthrid, SIGCONT );
        }
        else
        {
            error = ERR_NOTSUSP;
        }
    }
    else
        error = ERR_OBJDEL;
    sched_unlock();
 
    return( error );
}

/*****************************************************************************
** t_getreg - retrieves the contents of the specified task notepad register
*****************************************************************************/
ULONG
   t_getreg( ULONG tid, ULONG regnum, ULONG *reg_value )
{
    p2pthread_cb_t *current_tcb;
    p2pthread_cb_t *self_tcb;
    ULONG error;

    /*
    **  First ensure that the specified register is within range.
    **  ( We only support the eight user registers here. )
    */
    if ( regnum < NUM_TASK_REGS )
    {
        error = ERR_NO_ERROR;

        sched_lock();

        if ( tid == 0 )
        {
            /*
            **  Use the notepad register set for the current task.
            */
            self_tcb = my_tcb();
            if ( self_tcb != (p2pthread_cb_t *)NULL )
            {
                /*
                **  Retrieve the value from the specified notepad register.
                */
                *reg_value = self_tcb->registers[regnum];
            }
            else
                error = ERR_OBJDEL;
        }
        else
        {
            /*
            **  If the task_list contains tasks, scan it for the tcb
            **  whose task id matches the one specified.
            */
            current_tcb = tcb_for( tid );
            if ( current_tcb != (p2pthread_cb_t *)NULL )
            {
                /*
                **  Found the specified task.  Retrieve the
                **  value from the specified notepad register.
                */
                *reg_value = current_tcb->registers[regnum];
            }
            else
                error = ERR_OBJDEL;
        } 

        sched_unlock();
    }
    else
    {
        /*
        **  Unsupported register was specified.
        */
        error = ERR_REGNUM;
    }

    return( error );
}


/*****************************************************************************
** t_setreg - overwrites the contents of the specified task notepad register
**           with the specified reg_value.
*****************************************************************************/
ULONG
   t_setreg( ULONG tid, ULONG regnum, ULONG reg_value )
{
    p2pthread_cb_t *current_tcb;
    p2pthread_cb_t *self_tcb;
    ULONG error;

    /*
    **  First ensure that the specified register is within range.
    **  ( We only support the eight user registers here. )
    */
    if ( regnum < NUM_TASK_REGS )
    {
        error = ERR_NO_ERROR;

        sched_lock();

        if ( tid == 0 )
        {
            /*
            **  Use the notepad register set for the current task
            */
            self_tcb = my_tcb();
            if ( self_tcb != (p2pthread_cb_t *)NULL )
            {
                /*
                **  Write the caller's value to the specified notepad register.
                */
                self_tcb->registers[regnum] = reg_value; 
            }
            else
                error = ERR_OBJDEL;
        }
        else
        {
            /*
            **  If the task_list contains tasks, scan it for the tcb
            **  whose task id matches the one specified.
            */
            current_tcb = tcb_for( tid );
            if ( current_tcb != (p2pthread_cb_t *)NULL )
            {
                /*
                **  Found the specified task.  Write the caller's
                **  value to the specified register.
                */
                current_tcb->registers[regnum] = reg_value; 
            }
            else
                error = ERR_OBJDEL;
        } 

        sched_unlock();
    }
    else
    {
        /*
        **  Unsupported register was specified.
        */
        error = ERR_REGNUM;
    }

    return( error );
}

/*****************************************************************************
** t_setpri - sets a new priority for the specified task
*****************************************************************************/
ULONG
    t_setpri( ULONG tid, ULONG pri, ULONG *oldpri )
{
    p2pthread_cb_t *tcb;
    int new_priority, sched_policy;
    ULONG error;

    error = ERR_NO_ERROR;

    sched_lock();

    tcb = tcb_for( tid );
    if ( tcb != (p2pthread_cb_t *)NULL )
    {
        /*
        **  Save the previous priority level if the caller wants it.
        */
        if ( oldpri != (ULONG *)NULL )
            *oldpri = (ULONG)(tcb->prv_priority).sched_priority;

        /*
        **  Translate the p2pthread priority into a pthreads priority
        */
        pthread_attr_getschedpolicy( &(tcb->attr), &sched_policy );
        new_priority = translate_priority( pri, sched_policy, &error );

        /*
        **  Update the TCB with the new priority
        */
        (tcb->prv_priority).sched_priority = new_priority;

        /*
        **  If the selected task is not the currently-executing task,
        **  modify the pthread's priority now.  If the selected task
        **  IS the currently-executing task, the sched_unlock operation
        **  will restore this task to the new priority level.
        */
        if ( (tid != 0) && (tcb != my_tcb()) )
        {
            pthread_attr_setschedparam( &(tcb->attr), &(tcb->prv_priority) );
        } 
    } 
    else
        error = ERR_OBJDEL;

    sched_unlock();

    return( error );
}

/*****************************************************************************
** t_mode - sets the value of the calling task's mode flags
*****************************************************************************/
ULONG
    t_mode( ULONG mask, ULONG new_flags, ULONG *old_flags )
{
    p2pthread_cb_t *tcb;
    int sched_policy;
    ULONG error;

    error = ERR_NO_ERROR;

    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&task_list_lock );
    pthread_mutex_lock( &task_list_lock );

    tcb = my_tcb();
    if ( tcb != (p2pthread_cb_t *)NULL )
    {
        /*
        **  Save the previous priority level if the caller wants it.
        */
        if ( old_flags != (ULONG *)NULL )
            *old_flags = tcb->flags;

        /*
        **  Change the task preemptibility status if specified
        **  to either allow the task to be preempted or to prevent
        **  preemption.
        */
        pthread_mutex_unlock( &task_list_lock );
		/*
		**  Note: modified from (mask & T_NOPREEMPT).
		*/
        if  (mask & T_NOPREEMPT) 
		{
            if ( (new_flags & T_NOPREEMPT) )
            {
                if ( !(tcb->flags & T_NOPREEMPT) )
                    sched_lock();
            }
            else
            {
                if ( (tcb->flags & T_NOPREEMPT) )
                    sched_unlock();
            }
        }

        /*
        **  Determine whether round-robin time-slicing is to be used or not
        */
        pthread_mutex_lock( &task_list_lock );
		/*
		**  Note: modified from (mask & T_TSLICE).
		*/
        if (mask & T_TSLICE)
        {
            if ( new_flags & T_TSLICE )
                sched_policy = SCHED_RR;
            else
                sched_policy = SCHED_FIFO;
            pthread_attr_setschedpolicy( &(tcb->attr), sched_policy );
            pthread_setschedparam( tcb->pthrid, sched_policy,
                          (struct sched_param *)&((tcb->attr).__schedparam) );
        }

        /*
        **  Update the mode flag bits in the TCB.  First clear all the
        **  masked bits and then OR in the new bits.
        */
        tcb->flags &= (~mask);
        tcb->flags |= (new_flags & mask);
    } 
    else
        error = ERR_OBJDEL;

    pthread_mutex_unlock( &task_list_lock );
    pthread_cleanup_pop( 0 );

    return( error );
}


/*****************************************************************************
** t_ident - identifies the specified p2pthread task
*****************************************************************************/
ULONG
    t_ident( char name[4], ULONG node, ULONG *tid )
{
    p2pthread_cb_t *current_tcb;
    ULONG error;

    error = ERR_NO_ERROR;

    /*
    **  Validate the node specifier... only zero is allowed here.
    */ 
    if ( node != 0L )
        error = ERR_NODENO;
    else
    {
        /*
        **  If task name string is a NULL pointer, return TID of current task.
        */
        if ( name == (char *)NULL )
        {
            current_tcb = my_tcb();
            *tid = current_tcb->taskid;
        }
        else
        {
            /*
            **  Scan the task list for a name matching the caller's name.
            */
            for ( current_tcb = task_list;
                  current_tcb != (p2pthread_cb_t *)NULL;
                  current_tcb = current_tcb->nxt_task )
            {
                if ( (strncmp( name, current_tcb->taskname, 4 )) == 0 )
                {
                    /*
                    **  A matching name was found... return its TID
                    */
                    *tid = current_tcb->taskid;
                    break;
                }
            }
            if ( current_tcb == (p2pthread_cb_t *)NULL )
            {
                /*
                **  No matching name found... return caller's TID with error.
                */
                current_tcb = my_tcb();
                *tid = current_tcb->taskid;
                error = ERR_OBJNF;
            }
        }
    }

    return( error );
}

/*****************************************************************************
**  system initialization pthread
*****************************************************************************/
/*
void *init_system( void *dummy )
{
    user_sysroot();
    return( (void *)NULL );
}
*/    
/*****************************************************************************
**  p2pthread main program
**
**  This function serves as the entry point to the p2pthread emulation
**  environment.  It serves as the parent process to all p2pthread tasks.
**  This process creates an initialization thread and sets the priority of
**  that thread to the highest allowable value.  This allows the initialization
**  thread to complete its work without being preempted by any of the task
**  threads it creates.
*****************************************************************************/
/*
int main( int argc, char **argv )
{
    int max_priority;
    pthread_t tid_init;
    pthread_attr_t init_attr;
    struct sched_param init_priority;
*/
    /*
    **  Get the maximum permissible priority level for the current OS.
    */
//    max_priority = sched_get_priority_max( SCHED_FIFO );

    /*
    **  Lock all memory pages associated with this process to prevent delays
    **  due to process (or thread) memory being swapped out to disk and back.
    */
//    mlockall( (MCL_CURRENT | MCL_FUTURE) );

    /*
    **  Initialize the thread attributes to default values.
    **  Then modify the attributes to make a real-time thread.
    */
//    pthread_attr_init( &init_attr );
//    pthread_attr_setschedpolicy( &init_attr, SCHED_FIFO );

    /*
    **  Get the default scheduling priority & init init_priority
    **  This makes the initialization pthread the highest-priority thread
    **  in the p2pthread environment, and prevents premature preemption.
    */
/*
    pthread_attr_getschedparam( &init_attr, &init_priority );
    init_priority.sched_priority = max_priority;
    pthread_attr_setschedparam( &init_attr, &init_priority );

    printf( "\r\nStarting System Initialization Pthread" );
    pthread_create( &tid_init, &init_attr,
                    init_system, (void *)NULL );
*/
    /*
    **  Wait for the initialization thread to terminate and return.
    */    
//    pthread_join( tid_init, (void **)NULL );

//    exit( 0 );
//}
