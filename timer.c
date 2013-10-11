/*****************************************************************************
 * timer.c - defines the wrapper functions and data structures needed
 *           to implement a Wind River pSOS+ (R) timer API 
 *           in a POSIX Threads environment.
 ****************************************************************************/

#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "p2pthread.h"

#undef DIAG_PRINTFS


/*****************************************************************************
**  External function and data references
*****************************************************************************/

extern void
   sched_lock( void );
extern void
   sched_unlock( void );
extern p2pthread_cb_t *
   my_tcb( void );

/*****************************************************************************
** tm_wkafter - suspends the calling task for the specified number of ticks.
**            ( one tick is currently implemented as ten milliseconds )
*****************************************************************************/
ULONG
   tm_wkafter( ULONG interval )
{
    struct timeval now, timeout;
    ULONG usec;

    /*
    **  Calculate timeout delay in seconds and microseconds
    */
    usec = interval * P2PT_TICK * 1000;

    /*
    **  Note: delay of zero means yield CPU to other tasks of same 
    **  priority.
    */
    if ( usec > 0L )
    {
        /*
        **  Establish absolute time at expiration of delay interval
        */
        gettimeofday( &now, (struct timezone *)NULL );
        timeout.tv_sec = now.tv_sec;
        timeout.tv_usec = now.tv_usec;
        timeout.tv_usec += usec;
        if ( timeout.tv_usec > 1000000 )
        {
            timeout.tv_sec += timeout.tv_usec / 1000000;
            timeout.tv_usec = timeout.tv_usec % 1000000;
        }

        /*
        **  Note: wait for the current time of day to reach the time of day 
        **  calculated after the timeout expires.  The loop is necessary since 
        **  the thread may be awakened by signals before the timeout has elapsed.
        */
        while ( usec > 0 )
        {
            /*
            **  Note: add a cancellation point to this loop,
            **  since there are no others.
            */
            pthread_testcancel();

            usleep( usec );
            gettimeofday( &now, (struct timezone *)NULL );
            if ( timeout.tv_usec > now.tv_usec )
            {
                usec = timeout.tv_usec - now.tv_usec;
                if ( timeout.tv_sec < now.tv_sec )
                    usec = 0;
                else
                    usec += ((timeout.tv_sec - now.tv_sec) * 1000000);
            }
            else
            {
                usec = (timeout.tv_usec + 1000000) - now.tv_usec;
                if ( (timeout.tv_sec - 1) < now.tv_sec )
                    usec = 0;
                else
                    usec += (((timeout.tv_sec - 1) - now.tv_sec) * 1000000);
            }
        }
    }
    else
        /*
        **  Yield to any other task of same priority without blocking.
        */
        sched_yield();

    return( (ULONG)0 );
}

