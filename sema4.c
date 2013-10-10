/*****************************************************************************
 * sema4.c - defines the wrapper functions and data structures needed
 *           to implement a Wind River pSOS+ (R) semaphore API 
 *           in a POSIX Threads environment.
 *           to implement a pSOS+ API in a POSIX Threadenvironment.
 ****************************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/time.h>
#include "p2pthread.h"

#undef DIAG_PRINTFS

#define SM_PRIOR     0x02
#define SM_NOWAIT    0x01

#define ERR_TIMEOUT  0x01
#define ERR_NODENO   0x04
#define ERR_OBJDEL   0x05
#define ERR_OBJTFULL 0x08
#define ERR_OBJNF    0x09

#define ERR_NOSCB    0x41
#define ERR_NOSEM    0x42
#define ERR_SKILLD   0x43
#define ERR_TATSDEL  0x44

#define SEND  0
#define KILLD 2

/*****************************************************************************
**  Control block for p2pthread semaphore
**
**  The basic POSIX semaphore does not provide for time-bounded waits nor
**  for selection of a thread to ready based either on FIFO or PRIORITY-based
**  waiting.  This 'wrapper' extends the POSIX pthreads semaphore to include
**  the attributes of a p2pthread semaphore.
**
*****************************************************************************/
typedef struct p2pt_sema4
{
        /*
        ** ID for semaphore
        */
    ULONG
        smid;

        /*
        ** Semaphore Name
        */
    char
        sname[4];

        /*
        ** Option Flags for semaphore
        */
    ULONG
        flags;

        /*
        ** Mutex and Condition variable for semaphore post/pend
        */
    pthread_mutex_t
        sema4_lock;
    pthread_cond_t
        sema4_send;

        /*
        ** Mutex and Condition variable for semaphore delete
        */
    pthread_mutex_t
        smdel_lock;
    pthread_cond_t
        smdel_cplt;

        /*
        **  Pthread semaphore used to maintain count.
        */
    sem_t
        pthread_sema4;

        /*
        ** Type of send operation last performed on semaphore
        */
    int
        send_type;

        /*
        **  Pointer to next semaphore control block in semaphore list.
        */
    struct p2pt_sema4 *
        nxt_sema4;

        /*
        ** First task control block in list of tasks waiting on semaphore
        */
    p2pthread_cb_t *
        first_susp;
} p2pt_sema4_t;

/*****************************************************************************
**  External function and data references
*****************************************************************************/
extern void *
    ts_malloc( size_t blksize );
extern void
    ts_free( void *blkaddr );
extern p2pthread_cb_t *
   my_tcb( void );
extern void
   sched_lock( void );
extern void
   sched_unlock( void );
extern ULONG
   tm_wkafter( ULONG interval );
extern void
   link_susp_tcb( p2pthread_cb_t **list_head, p2pthread_cb_t *new_entry );
extern void
   unlink_susp_tcb( p2pthread_cb_t **list_head, p2pthread_cb_t *entry );
extern int
   signal_for_my_task( p2pthread_cb_t **list_head, int pend_order );

/*****************************************************************************
**  p2pthread Global Data Structures
*****************************************************************************/

/*
**  sema4_list is a linked list of semaphore control blocks.  It is used to locate
**             semaphores by their ID numbers.
*/
static p2pt_sema4_t *
    sema4_list;

/*
**  sema4_list_lock is a mutex used to serialize access to the semaphore list
*/
static pthread_mutex_t
    sema4_list_lock = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************************
** smcb_for - returns the address of the semaphore control block for the semaphore
**            idenified by smid
*****************************************************************************/
static p2pt_sema4_t *
   smcb_for( ULONG smid )
{
    p2pt_sema4_t *current_smcb;
    int found_smid;

        if ( sema4_list != (p2pt_sema4_t *)NULL )
        {
            /*
            **  One or more semaphores already exist in the semaphore list...
            **  Scan the existing semaphores for a matching ID.
            */
            found_smid = FALSE;
            for ( current_smcb = sema4_list; 
                  current_smcb != (p2pt_sema4_t *)NULL;
                  current_smcb = current_smcb->nxt_sema4 )
            {
                if ( current_smcb->smid == smid )
                {
                    found_smid = TRUE;
                    break;
                }
            }
            if ( found_smid == FALSE )
                /*
                **  No matching ID found
                */
                current_smcb = (p2pt_sema4_t *)NULL;
        }
        else
            current_smcb = (p2pt_sema4_t *)NULL;
 
    return( current_smcb );
}


/*****************************************************************************
** new_smid - automatically returns a valid, unused semaphore ID
*****************************************************************************/
static ULONG
   new_smid( void )
{
    p2pt_sema4_t *current_smcb;
    ULONG new_sema4_id;

    /*
    **  Protect the semaphore list while we examine it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&sema4_list_lock );
    pthread_mutex_lock( &sema4_list_lock );

    /*
    **  Get the highest previously assigned semaphore id and add one.
    */
    if ( sema4_list != (p2pt_sema4_t *)NULL )
    {
        /*
        **  One or more semaphores already exist in the semaphore list...
        **  Find the highest semaphore ID number in the existing list.
        */
        new_sema4_id = sema4_list->smid;
        for ( current_smcb = sema4_list; 
              current_smcb->nxt_sema4 != (p2pt_sema4_t *)NULL;
              current_smcb = current_smcb->nxt_sema4 )
        {
            if ( (current_smcb->nxt_sema4)->smid > new_sema4_id )
            {
                new_sema4_id = (current_smcb->nxt_sema4)->smid;
            }
        }

        /*
        **  Add one to the highest existing semaphore ID
        */
        new_sema4_id++;
    }
    else
    {
        /*
        **  this is the first semaphore being added to the semaphore list.
        */
        new_sema4_id = 1;
    }
 
    /*
    **  Re-enable access to the semaphore list by other threads.
    */
    pthread_mutex_unlock( &sema4_list_lock );
    pthread_cleanup_pop( 0 );

    return( new_sema4_id );
}

/*****************************************************************************
** link_smcb - appends a new semaphore control block pointer to the sema4_list
*****************************************************************************/
static void
   link_smcb( p2pt_sema4_t *new_sema4 )
{
    p2pt_sema4_t *current_smcb;

    /*
    **  Protect the semaphore list while we examine and modify it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&sema4_list_lock );
    pthread_mutex_lock( &sema4_list_lock );

    new_sema4->nxt_sema4 = (p2pt_sema4_t *)NULL;
    if ( sema4_list != (p2pt_sema4_t *)NULL )
    {
        /*
        **  One or more semaphores already exist in the semaphore list...
        **  Insert the new entry in ascending numerical sequence by smid.
        */
        for ( current_smcb = sema4_list; 
              current_smcb->nxt_sema4 != (p2pt_sema4_t *)NULL;
              current_smcb = current_smcb->nxt_sema4 )
        {
            if ( (current_smcb->nxt_sema4)->smid > new_sema4->smid )
            {
                new_sema4->nxt_sema4 = current_smcb->nxt_sema4;
                break;
            }
        }
        current_smcb->nxt_sema4 = new_sema4;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd semaphore cb @ %p to list @ %p", new_sema4,
                current_smcb );
#endif
    }
    else
    {
        /*
        **  this is the first semaphore being added to the semaphore list.
        */
        sema4_list = new_sema4;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd semaphore cb @ %p to list @ %p", new_sema4,
                &sema4_list );
#endif
    }
 
    /*
    **  Re-enable access to the semaphore list by other threads.
    */
    pthread_mutex_unlock( &sema4_list_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** unlink_smcb - removes a semaphore control block pointer from the sema4_list
*****************************************************************************/
static p2pt_sema4_t *
   unlink_smcb( ULONG smid )
{
    p2pt_sema4_t *current_smcb;
    p2pt_sema4_t *selected_smcb;

    selected_smcb =  (p2pt_sema4_t *)NULL;

    if ( sema4_list != (p2pt_sema4_t *)NULL )
    {
        /*
        **  One or more semaphores exist in the semaphore list...
        **  Protect the semaphore list while we examine and modify it.
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&sema4_list_lock );
        pthread_mutex_lock( &sema4_list_lock );

        /*
        **  Scan the semaphore list for an smcb with a matching semaphore ID
        */
        if ( sema4_list->smid == smid )
        {
            /*
            **  The first semaphore in the list matches the selected ID
            */
            selected_smcb = sema4_list; 
            sema4_list = selected_smcb->nxt_sema4;
#ifdef DIAG_PRINTFS 
            printf( "\r\ndel semaphore cb @ %p from list @ %p", selected_smcb,
                    &sema4_list );
#endif
        }
        else
        {
            /*
            **  Scan the next smcb for a matching smid while retaining a
            **  pointer to the current smcb.  If the next smcb matches,
            **  select it and then unlink it from the semaphore list.
            */
            for ( current_smcb = sema4_list; 
                  current_smcb->nxt_sema4 != (p2pt_sema4_t *)NULL;
                  current_smcb = current_smcb->nxt_sema4 )
            {
                if ( (current_smcb->nxt_sema4)->smid == smid )
                {
                    /*
                    **  Semaphore ID of next smcb matches...
                    **  Select the smcb and then unlink it by linking
                    **  the selected smcb's next smcb into the current smcb.
                    */
                    selected_smcb = current_smcb->nxt_sema4;
                    current_smcb->nxt_sema4 = selected_smcb->nxt_sema4;
#ifdef DIAG_PRINTFS 
                    printf( "\r\ndel semaphore cb @ %p from list @ %p",
                            selected_smcb, current_smcb );
#endif
                    break;
                }
            }
        }

        /*
        **  Re-enable access to the semaphore list by other threads.
        */
        pthread_mutex_unlock( &sema4_list_lock );
        pthread_cleanup_pop( 0 );
    }

    return( selected_smcb );
}

/*****************************************************************************
** sm_create - creates a p2pthread message semaphore
*****************************************************************************/
ULONG
    sm_create( char name[4], ULONG count, ULONG opt, ULONG *smid )
{
    p2pt_sema4_t *semaphore;
    ULONG error;
    int i;
    char create_error[80];

    error = ERR_NO_ERROR;

    /*
    **  First allocate memory for the semaphore control block
    */
    semaphore = (p2pt_sema4_t *)ts_malloc( sizeof( p2pt_sema4_t ) );
    if ( semaphore != (p2pt_sema4_t *)NULL )
    {
        /*
        **  Ok... got a control block.  Initialize it.
        */

        /*
        ** Option Flags for semaphore
        */
        semaphore->flags = opt;

        /*
        ** ID for semaphore
        */
        semaphore->smid = new_smid();
        if ( smid != (ULONG *)NULL )
            *smid = semaphore->smid;

        /*
        **  Name for semaphore
        */
        for ( i = 0; i < 4; i++ )
            semaphore->sname[i] = name[i];

#ifdef DIAG_PRINTFS 
        printf( "\r\nCreating semaphore %c%c%c%c id %ld @ %p",
                     semaphore->sname[0], semaphore->sname[1],
                     semaphore->sname[r20], semaphore->sname[3],
                     semaphore->smid, semaphore );
#endif

        /*
        ** Mutex and Condition variable for semaphore send/pend
        */
        pthread_mutex_init( &(semaphore->sema4_lock),
                            (pthread_mutexattr_t *)NULL );
        pthread_cond_init( &(semaphore->sema4_send),
                           (pthread_condattr_t *)NULL );

        /*
        ** Mutex and Condition variable for semaphore delete/delete
        */
        pthread_mutex_init( &(semaphore->smdel_lock),
                            (pthread_mutexattr_t *)NULL );
        pthread_cond_init( &(semaphore->smdel_cplt),
                           (pthread_condattr_t *)NULL );

        /*
        **  Pthread semaphore used to maintain count. Cause Linuxthread
		**  didn't inplement the multi-thread shared sema4, so the arg
		**  'pshared' should be set to zero, any other value cause the 
		**  function return with -1.
        */
        if ( sem_init( &(semaphore->pthread_sema4), 0, (unsigned int)count ) )
        {
            sprintf( create_error,
                     "\r\nSemaphore %c%c%c%c creation returned error:",
                     semaphore->sname[0], semaphore->sname[1],
                     semaphore->sname[2], semaphore->sname[3] );
            perror( create_error );
        }

        /*
        ** Type of send operation last performed on semaphore
        */
        semaphore->send_type = SEND;

        /*
        ** First task control block in list of tasks waiting on semaphore
        */
        semaphore->first_susp = (p2pthread_cb_t *)NULL;

        /*
        **  Link the new semaphore into the semaphore list.
        */
        link_smcb( semaphore );
    }
    else
    {
        error = ERR_NOSCB;
    }

    return( error );
}

/*****************************************************************************
** sm_v - releases a p2pthread semaphore token and awakens the first selected
**        task waiting on the semaphore.
*****************************************************************************/
ULONG
   sm_v( ULONG smid )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pt_sema4_t *semaphore;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (semaphore = smcb_for( smid )) != (p2pt_sema4_t *)NULL )
    {
#ifdef DIAG_PRINTFS 
        our_tcb = my_tcb();
        printf( "\r\ntask @ %p post to semaphore list @ %p", our_tcb,
                &(semaphore->first_susp) );
#endif

        /*
        **  'Lock the p2pthread scheduler' to defer any context switch to a
        **  higher priority task until after this call has completed its work.
        */
        sched_lock();

        /*
        ** Lock mutex for semaphore send
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(semaphore->sema4_lock));
        pthread_mutex_lock( &(semaphore->sema4_lock) );

        sem_post( &(semaphore->pthread_sema4) );

        if ( semaphore->first_susp != (p2pthread_cb_t *)NULL )
            /*
            **  Signal the condition variable for the semaphore
            */
            pthread_cond_broadcast( &(semaphore->sema4_send) );

        /*
        **  Unlock the semaphore mutex. 
        */
        pthread_mutex_unlock( &(semaphore->sema4_lock) );
        pthread_cleanup_pop( 0 );

        /*
        **  'Unlock the p2pthread scheduler' to enable a possible context switch
        **  to a task made runnable by this call.
        */
        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** delete_sema4 - takes care of destroying the specified semaphore and freeing
**                any resources allocated for that semaphore
*****************************************************************************/
static void
   delete_sema4( p2pt_sema4_t *semaphore )
{
    /*
    **  First remove the semaphore from the semaphore list
    */
    unlink_smcb( semaphore->smid );

    /*
    **  Next destroy the pthreads semaphore
    */
    sem_destroy( &(semaphore->pthread_sema4) );

    /*
    **  Finally delete the semaphore control block itself;
    */
    ts_free( (void *)semaphore );

}

/*****************************************************************************
** sm_delete - removes the specified semaphore from the semaphore list and frees
**              the memory allocated for the semaphore control block and extents.
*****************************************************************************/
ULONG
   sm_delete( ULONG smid )
{
    p2pt_sema4_t *semaphore;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (semaphore = smcb_for( smid )) != (p2pt_sema4_t *)NULL )
    {
        /*
        **  Send signal and block while any tasks are still waiting
        **  on the semaphore
        */
        sched_lock();
        if ( semaphore->first_susp != (p2pthread_cb_t *)NULL )
        {
            /*
            ** Lock mutex for semaphore delete completion
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(semaphore->smdel_lock) );
            pthread_mutex_lock( &(semaphore->smdel_lock) );

            /*
            ** Lock mutex for semaphore delete
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(semaphore->sema4_lock));
            pthread_mutex_lock( &(semaphore->sema4_lock) );

            /*
            **  Declare the send type
            */
            semaphore->send_type = KILLD;

            error = ERR_TATSDEL;

            /*
            **  Signal the condition variable for the semaphore
            */
            pthread_cond_broadcast( &(semaphore->sema4_send) );

            /*
            **  Unlock the semaphore mutex. 
            */
            pthread_mutex_unlock( &(semaphore->sema4_lock) );
            pthread_cleanup_pop( 0 );

            /*
            **  Wait for all pended tasks to receive delete message.
            **  The last task to receive the message will signal the
            **  delete-complete condition variable.
            */
            while ( semaphore->first_susp != (p2pthread_cb_t *)NULL )
                pthread_cond_wait( &(semaphore->smdel_cplt),
                                   &(semaphore->smdel_lock) );

            /*
            **  Unlock the semaphore delete completion mutex. 
            */
            pthread_mutex_unlock( &(semaphore->smdel_lock) );
            pthread_cleanup_pop( 0 );
        }
        delete_sema4( semaphore );
        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** waiting_on_sema4 - returns a nonzero result unless a qualifying event
**                    occurs on the specified semaphore which should cause the
**                    pended task to be awakened.  The qualifying events
**                    are:
**                        (1) a message is sent to the semaphore and the current
**                            task is selected to receive it
**                        (2) a delete message is sent to the semaphore
**                        (3) the semaphore is deleted
*****************************************************************************/
static int
    waiting_on_sema4( p2pt_sema4_t *semaphore, struct timespec *timeout,
                      int *retcode )
{
    int result;
    struct timeval now;
    ULONG usec;

    if ( semaphore->send_type & KILLD )
    {
        /*
        **  Semaphore has been killed... waiting is over.
        */
        result = 0;
        *retcode = 0;
    }
    else
    {
        /*
        **  Semaphore still in service... check for token availability.
        **  Initially assume no token available for our task
        */
        result = 1;

        /*
        **  Multiple posts to the semaphore may be represented by only
        **  a single signal to the condition variable, so continue
        **  checking for a token for our task as long as more tokens
        **  are available.
        */
        while ( sem_trywait( &(semaphore->pthread_sema4) ) == 0 )
        {
            /*
            **  Available token arrived... see if it's for our task.
            */
            if ( (signal_for_my_task( &(semaphore->first_susp),
                                      (semaphore->flags & SM_PRIOR) )) )
            {
                /*
                **  Token was destined for our task specifically...
                **  waiting is over.
                */
                result = 0;
                *retcode = 0;
                break;
            }
            else
            {
                /*
                **  Token isn't for our task...
                **  Put it back and continue waiting.  Sleep awhile to
                **  allow other tasks ahead of ours in the queue of tasks
                **  waiting on the semaphore to get their tokens, bringing
                **  our task to the head of the list.
                */
                sem_post( &(semaphore->pthread_sema4) );
                pthread_mutex_unlock( &(semaphore->sema4_lock) );
                tm_wkafter( 1 );
                pthread_mutex_lock( &(semaphore->sema4_lock) );
            }

            /*
            **  If a timeout was specified, make sure we respect it and
            **  exit this loop if it expires.
            */
            if ( timeout != (struct timespec *)NULL )
            {
                gettimeofday( &now, (struct timezone *)NULL );
                if ( timeout->tv_nsec > (now.tv_usec * 1000) )
                {
                    usec = (timeout->tv_nsec - (now.tv_usec * 1000)) / 1000;
                    if ( timeout->tv_sec < now.tv_sec )
                        usec = 0;
                    else
                        usec += ((timeout->tv_sec - now.tv_sec) * 1000000);
                }
                else
                {
                    usec = ((timeout->tv_nsec + 1000000000) -
                            (now.tv_usec * 1000)) / 1000;
                    if ( (timeout->tv_sec - 1) < now.tv_sec )
                        usec = 0;
                    else
                        usec += (((timeout->tv_sec - 1) - now.tv_sec)
                                 * 1000000);
                }
                if ( usec == 0 )
                    break;
            }
        }
    }

    return( result );
}

/*****************************************************************************
** sm_p - blocks the calling task until a token is available on the
**             specified p2pthread semaphore.
*****************************************************************************/
ULONG
   sm_p( ULONG smid, ULONG opt, ULONG max_wait )
{
    p2pthread_cb_t *our_tcb;
    struct timeval now;
    struct timespec timeout;
    int retcode;
    long sec, usec;
    p2pt_sema4_t *semaphore;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (semaphore = smcb_for( smid )) != (p2pt_sema4_t *)NULL )
    {
        /*
        ** Lock mutex for semaphore pend
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(semaphore->sema4_lock));
        pthread_mutex_lock( &(semaphore->sema4_lock) );

        /*
        **  Add tcb for task to list of tasks waiting on semaphore
        */
        our_tcb = my_tcb();
#ifdef DIAG_PRINTFS 
        printf( "\r\ntask @ %p wait on semaphore list @ %p", our_tcb,
                &(semaphore->first_susp) );
#endif

        link_susp_tcb( &(semaphore->first_susp), our_tcb );

        retcode = 0;

        if ( opt & SM_NOWAIT )
        {
            /*
            **  Caller specified no wait on semaphore token...
            **  Check the condition variable with an immediate timeout.
            */
            gettimeofday( &now, (struct timezone *)NULL );
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = now.tv_usec * 1000;
            while ( (waiting_on_sema4( semaphore, &timeout, &retcode )) &&
                    (retcode != ETIMEDOUT) )
            {
                retcode = pthread_cond_timedwait( &(semaphore->sema4_send),
                                                  &(semaphore->sema4_lock),
                                                  &timeout );
            }
        }
        else
        {
            /*
            **  Caller expects to wait on semaphore, with or without a timeout.
            */
            if ( max_wait == 0L )
            {
                /*
                **  Infinite wait was specified... wait without timeout.
                */
                while ( waiting_on_sema4( semaphore, 0, &retcode ) )
                {
                    pthread_cond_wait( &(semaphore->sema4_send),
                                       &(semaphore->sema4_lock) );
                }
            }
            else
            {
                /*
                **  Wait on semaphore message arrival with timeout...
                **  Calculate timeout delay in seconds and microseconds.
                */
                sec = 0;
                usec = max_wait * P2PT_TICK * 1000;
                gettimeofday( &now, (struct timezone *)NULL );
                usec += now.tv_usec;
                if ( usec > 1000000 )
                {
                    sec = usec / 1000000;
                    usec = usec % 1000000;
                }
                timeout.tv_sec = now.tv_sec + sec;
                timeout.tv_nsec = usec * 1000;

                /*
                **  Wait for a semaphore message for the current task or for the
                **  timeout to expire.  The loop is required since the task
                **  may be awakened by signals for semaphore tokens which are
                **  not ours, or for signals other than from a message send.
                */
                while ( (waiting_on_sema4( semaphore, &timeout, &retcode )) &&
                        (retcode != ETIMEDOUT) )
                {
                    retcode = pthread_cond_timedwait( &(semaphore->sema4_send),
                                                      &(semaphore->sema4_lock),
                                                      &timeout );
                }
            }
        }

        /*
        **  Remove the calling task's tcb from the waiting task list
        **  for the semaphore.
        */
        unlink_susp_tcb( &(semaphore->first_susp), our_tcb );

        /*
        **  See if we were awakened due to a sm_delete on the semaphore.
        */
        if ( semaphore->send_type & KILLD )
        {
            error = ERR_SKILLD;

            if ( semaphore->first_susp == (p2pthread_cb_t *)NULL )
            {
                /*
                ** Lock mutex for semaphore delete completion
                */
                pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                      (void *)&(semaphore->smdel_lock) );
                pthread_mutex_lock( &(semaphore->smdel_lock) );

                /*
                **  Signal the delete-complete condition variable
                **  for the semaphore
                */
                pthread_cond_broadcast( &(semaphore->smdel_cplt) );

                semaphore->send_type = SEND;

                /*
                **  Unlock the semaphore delete completion mutex. 
                */
                pthread_mutex_unlock( &(semaphore->smdel_lock) );
                pthread_cleanup_pop( 0 );
            }

#ifdef DIAG_PRINTFS 
            printf( "...semaphore deleted" );
#endif
        }
        else
        {
            /*
            **  See if we timed out or if we got a token
            */
            if ( retcode == ETIMEDOUT )
            {
                /*
                **  Timed out without a token
                */
                if ( opt & SM_NOWAIT )
                {
                    error = ERR_NOSEM;
#ifdef DIAG_PRINTFS 
                    printf( "...no token available" );
#endif
                }
                else
                {
                    error = ERR_TIMEOUT;
#ifdef DIAG_PRINTFS 
                    printf( "...timed out" );
#endif
                }
            }
#ifdef DIAG_PRINTFS 
            else
            {
                printf( "...rcvd semaphore token" );
            }
#endif
        }

        /*
        **  Unlock the mutex for the condition variable and clean up.
        */
        pthread_mutex_unlock( &(semaphore->sema4_lock) );
        pthread_cleanup_pop( 0 );
    }
    else
    {
        error = ERR_OBJDEL;       /* Invalid semaphore specified */
    }

    return( error );
}

/*****************************************************************************
** sm_ident - identifies the specified p2pthread semaphore
*****************************************************************************/
ULONG
    sm_ident( char name[4], ULONG node, ULONG *smid )
{
    p2pt_sema4_t *current_smcb;
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
        **  If semaphore name string is a NULL pointer, return with error.
        **  We'll ASSUME the smid pointer isn't NULL!
        */
        if ( name == (char *)NULL )
        {
            *smid = (ULONG)NULL;
            error = ERR_OBJNF;
        }
        else
        {
            /*
            **  Scan the task list for a name matching the caller's name.
            */
            for ( current_smcb = sema4_list;
                  current_smcb != (p2pt_sema4_t *)NULL;
                  current_smcb = current_smcb->nxt_sema4 )
            {
                if ( (strncmp( name, current_smcb->sname, 4 )) == 0 )
                {
                    /*
                    **  A matching name was found... return its QID
                    */
                    *smid = current_smcb->smid;
                    break;
                }
            }
            if ( current_smcb == (p2pt_sema4_t *)NULL )
            {
                /*
                **  No matching name found... return caller's QID with error.
                */
                *smid = (ULONG)NULL;
                error = ERR_OBJNF;
            }
        }
    }

    return( error );
}
