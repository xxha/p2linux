/*****************************************************************************
 * event.c - defines the wrapper functions and data structures needed
 *           to implement a Wind River pSOS+ (R) task event flag API 
 *           in a POSIX Threads environment.
 ****************************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include "p2pthread.h"

#undef DIAG_PRINTFS

#define EV_NOWAIT    0x01
#define EV_ANY       0x02

#define ERR_TIMEOUT  0x01
#define ERR_NODENO   0x04
#define ERR_OBJDEL   0x05
#define ERR_OBJTFULL 0x08
#define ERR_OBJNF    0x09

#define ERR_NOEVS    0x3C

/*****************************************************************************
**  External function and data references
*****************************************************************************/
extern void
   sched_lock( void );
extern void
   sched_unlock( void );
extern p2pthread_cb_t *
   my_tcb( void );
extern p2pthread_cb_t *
   tcb_for( ULONG taskid );


/*****************************************************************************
** ev_send - sets the specified flag bits in a p2pthread task event group
*****************************************************************************/
ULONG
   ev_send( ULONG taskid, ULONG new_events )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pthread_cb_t *tcb;
    ULONG old_flags;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (tcb = tcb_for( taskid )) != (p2pthread_cb_t *)NULL )
    {
        /*
        **  'Lock the p2pthread scheduler' to defer any context switch to a
        **  higher priority task until after this call has completed its work.
        */
        sched_lock();

        /*
        **  Lock mutex for event post
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(tcb->event_lock));
        pthread_mutex_lock( &(tcb->event_lock) );

        /*
        **  Get the state of the flag bits prior to the send operation
        */
        old_flags = tcb->events_pending;

#ifdef DIAG_PRINTFS 
        printf( "\r\nevent flags @ tcb %p new %lx pending %lx captured %lx",
                tcb, new_events, tcb->events_pending, tcb->events_captured );
#endif
        /*
        **  If requested to set any flags which were not already set,
        */ 
        if ( (old_flags & new_events) != new_events )
        {
            /*
            **  Set the flag bits specified by the caller
            */
            tcb->events_pending = old_flags | new_events;
#ifdef DIAG_PRINTFS 
        printf( "\r\nsignalling new event flags %lx @ tcb %p",
                new_events, tcb );
#endif

            /*
            **  Signal the condition variable for the event
            */
            pthread_cond_broadcast( &(tcb->event_change) );
        }

        /*
        **  Unlock the event mutex. 
        */
        pthread_mutex_unlock( &(tcb->event_lock) );
        pthread_cleanup_pop( 0 );

        /*
        **  'Unlock the p2pthread scheduler' to enable a possible context switch
        **  to a task made runnable by this call.
        */
        sched_unlock();
#ifdef DIAG_PRINTFS 
        our_tcb = my_tcb();
        printf( "\r\ntask @ %p sent event flags %lx @ tcb %p",
                our_tcb, new_events, tcb );
#endif
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** events_match_mask - compares the specified set of event flags against
**                     the specified event combination and returns a boolean
**                     TRUE/FALSE result.
*****************************************************************************/
static int
    events_match_mask( p2pthread_cb_t *tcb, ULONG rule )
{
    int match;
    ULONG still_pending;

    /*
    **  Save a mask of events already captured when the new pending events
    **  arrived. Note: we induct events_captured member here because of we 
    **  have to test the condition of (rule & EV_ALL).
    */
    still_pending = tcb->events_captured;

    /*
    **  Capture any newly-arrived pending events which are wanted.
    **  These events will be accumulated until the match rule is satisfied.
    */
    tcb->events_captured |= (tcb->events_pending & tcb->event_mask);

    /*
    **  Clear any events just captured from the pending list.  Any events
    **  which had previously been captured and had occurred again are left
    **  pending.
    */
    tcb->events_pending &= ((~tcb->event_mask) | still_pending);

    /*
    **  Now see if the match rule has been satisfied by captured events.
    */

    /*
	**  Note: i have modified this line to suit for rule == 
	**  EV_ANY | EV_NOWAIT or EV_ANY |EV_WAIT condition.
	**  The origin statement here is "if ( rule == EV_ANY )"
	*/
	if ( rule & EV_ANY )
    // if ( rule == EV_ANY )
    {
        /* ANY rule... any event matching a bit in mask awakens task */
        if ( (tcb->events_captured & tcb->event_mask) != 0L )
            match = TRUE;
		else 
		    match = FALSE;

#ifdef DIAG_PRINTFS 
        printf( "\r\nmatch any event @ tcb %p mask %lx pending %lx captured %lx",
                tcb, tcb->event_mask, tcb->events_pending,
                tcb->events_captured );
#endif
    }
    else
    {
        /* ALL rule... all bits in mask must be matched by events */
        if ( (tcb->events_captured & tcb->event_mask) == tcb->event_mask )
            match = TRUE;
		else 
		    match = FALSE;

#ifdef DIAG_PRINTFS 
        printf( "\r\nmatch all events @ tcb %p mask %lx pending %lx captured %lx",
                tcb, tcb->event_mask, tcb->events_pending,
                tcb->events_captured );
#endif
    }

    return( match );
}


/*****************************************************************************
** ev_receive - blocks the calling task until a matching combination of events
**            occurs in the specified p2pthread event flag group.
*****************************************************************************/
ULONG
   ev_receive( ULONG mask, ULONG opt, ULONG max_wait, ULONG *captured )
{
    p2pthread_cb_t *tcb;
    struct timeval now;
    struct timespec timeout;
    int retcode;
    long sec, usec;
    ULONG error;

    error = ERR_NO_ERROR;

    /*
    **  Get tcb for task
    */
    tcb = my_tcb();

    /*
    **  Save event mask in TCB, Note: there is no locking here 
    **  since only in my own pthread i change this member.
    */
    tcb->event_mask = mask;

#ifdef DIAG_PRINTFS 
    printf( "\r\ntask @ %p pend on event flags %lx", tcb, mask );
#endif

    /*
    ** Lock mutex for event pend.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&(tcb->event_lock));
    pthread_mutex_lock( &(tcb->event_lock) );

    retcode = 0;

    if ( opt & EV_NOWAIT )
    {
        /*
        **  Caller specified no wait on events...
        **  Check the condition variable with an immediate timeout.
        */
            gettimeofday( &now, (struct timezone *)NULL );
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = now.tv_usec * 1000;
            while ( !(events_match_mask( tcb, opt )) &&
                    (retcode != ETIMEDOUT) )
            {
                retcode = pthread_cond_timedwait( &(tcb->event_change),
                                              &(tcb->event_lock), &timeout );
            }
    }
    else
    {
        /*
        **  Caller expects to wait on events, with or without a timeout.
        */
        if ( max_wait == 0L )
        {
            /*
            **  Infinite wait was specified... wait without timeout,
            **  The loop is required since the task may be awakened 
            **  by signals for events which do not match, or for
            **  signals other than from a send to the event flags.
            */
            while ( !(events_match_mask( tcb, opt )) )
            {
                pthread_cond_wait( &(tcb->event_change),
                                   &(tcb->event_lock) );
            }
        }
        else
        {
            /*
            **  Calculate timeout delay in seconds and microseconds
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
            **  Wait for an event match for the current task or for the
            **  timeout to expire.  The loop is required since the task may
            **  be awakened by signals for events which do not match, or
            **  for signals other than from a send to the event flags (some 
            **  other signals also may cause pthread_cond_timedwait() to 
            **  premature return, see p2linux.pdf).
            */
            while ( !(events_match_mask( tcb, opt )) &&
                    (retcode != ETIMEDOUT) )
            {
                retcode = pthread_cond_timedwait( &(tcb->event_change),
                                                  &(tcb->event_lock),
                                                  &timeout );
            }
        }
    }

    /*
    **  Return a list of captured events to the caller.
    */
    if ( captured != (ULONG *)NULL )
        *captured = tcb->events_captured;

    /*
    **  See if we timed out or if we got an event match
    */
    if ( retcode == ETIMEDOUT )
    {
        /*
        **  Timed out without an event match
        */
        if ( opt & EV_NOWAIT )
            error = ERR_NOEVS;
        else
            error = ERR_TIMEOUT;
#ifdef DIAG_PRINTFS 
        printf( "...timed out" );
#endif
    }
    else
    {
        /*
        **  An event match occurred... Clear the accumulated captured events.
        */
#ifdef DIAG_PRINTFS 
        printf( "\r\ntask @ %p captured events %lx", tcb,
                tcb->events_captured );
#endif

        tcb->events_captured = (ULONG)NULL;
    }

    /*
    **  Unlock the mutex for the condition variable and clean up.
    */
    pthread_mutex_unlock( &(tcb->event_lock) );
    pthread_cleanup_pop( 0 );

    return( error );
}

