/*****************************************************************************
 * queue.c - defines the wrapper functions and data structures needed
 *           to implement a Wind River pSOS+ (R) standard queue API 
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

#define SEND  0
#define BCAST 1
#define KILLD 2

#define Q_NOWAIT     0x01
#define Q_PRIOR      0x02
#define Q_LIMIT      0x04

#define ERR_TIMEOUT  0x01
#define ERR_NODENO   0x04
#define ERR_OBJDEL   0x05
#define ERR_OBJTFULL 0x08
#define ERR_OBJNF    0x09

#define ERR_NOQCB    0x33
#define ERR_NOMGB    0x34
#define ERR_QFULL    0x35
#define ERR_QKILLD   0x36
#define ERR_NOMSG    0x37
#define ERR_TATQDEL  0x38
#define ERR_MATQDEL  0x39

/*****************************************************************************
**  p2pthread queue message type
*****************************************************************************/
typedef ULONG q_msg_t[4];

/*****************************************************************************
**  p2pthread queue extent type - this is the header for a dynamically allocated
**                           memory block of a size determined at runtime.
**                           An array of (qsize + 1) messages immediately 
**                           follows the nxt_extent pointer.  The first
**                           element of the array is included in the header
**                           to guarantee proper alignment for the additional
**                           (qsize) array elements which will be allocated
**                           and appended to it.
*****************************************************************************/
typedef struct queue_extent_header
{
    void *
        nxt_extent;      /* Points to next extent block (if any, else NULL)*/
    q_msg_t
        msgs[1];         /* Array of qsize + 1 q_msg_t messages */
} q_extent_t;

/*****************************************************************************
**  Control block for p2pthread queue
**
**  The message list for a queue is organized into a series of one or
**  more arrays called extents.  Actual send and fetch operations are
**  done using a queue_head and queue_tail pointer.  These pointers
**  must 'rotate' through the arrays in the extent list to create a
**  single large logical circular buffer.  A single extra location is added
**  to ensure room for urgent messages or broadcasts even when the queue is
**  'full' for normal messages.
**
*****************************************************************************/
typedef struct p2pt_queue
{
        /*
        ** ID for queue
        */
    ULONG
        qid;

        /*
        ** Queue Name
        */
    char
        qname[4];

        /*
        ** Option Flags for queue
        */
    ULONG
        flags;

        /*
        ** Mutex and Condition variable for queue send/pend
        */
    pthread_mutex_t
        queue_lock;
    pthread_cond_t
        queue_send;

        /*
        ** Mutex and Condition variable for queue broadcast/delete
        */
    pthread_mutex_t
        qbcst_lock;
    pthread_cond_t
        queue_bcplt;

        /*
        **  Pointer to next message pointer to be fetched from queue
        */
    q_msg_t *
        queue_head;

        /*
        **  Pointer to last message pointer sent to queue
        */
    q_msg_t *
        queue_tail;

        /*
        ** Type of send operation last performed on queue
        */
    int
        send_type;

        /*
        **  Pointer to first extent in list of extents allocated for queue
        */
    q_extent_t *
        first_extent;

        /*
        **  Pointer to last message in last extent in extent list
        */
    q_msg_t *
        last_msg_in_queue;

        /*
        **  Pointer to next queue control block in queue list.
        */
    struct p2pt_queue *
        nxt_queue;

        /*
        ** First task control block in list of tasks waiting on queue.
		** Note: i thought this member should be catagoried to 
		** q_first_susp, sem4_first_susp, etc., but it's wrong,since 
		** there is only one object can one task suspend on at one time.
        */
    p2pthread_cb_t *
        first_susp;

        /*
        **  Count of tasks awakened by q_broadcast call
        */
    ULONG
        bcst_tasks_awakened;

        /*
        ** Total number of messages currently sent to queue
        */
    int
        msg_count;

        /*
        ** Total messages per memory allocation block (extent)
        */
    int
        msgs_per_extent;

        /*
        ** Total number of extents currently allocated for queue
        */
    int
        total_extents;

        /*
        ** Task pend order (FIFO or Priority) for queue
        */
    int
        order;
} p2pt_queue_t;

/*****************************************************************************
**  External function and data references
*****************************************************************************/
extern void *ts_malloc( size_t blksize );
extern void ts_free( void *blkaddr );
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
**  queue_list is a linked list of queue control blocks.  It is used to locate
**             queues by their ID numbers.
*/
static p2pt_queue_t *
    queue_list;

/*
**  queue_list_lock is a mutex used to serialize access to the queue list
*/
static pthread_mutex_t
    queue_list_lock = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************************
** qcb_for - returns the address of the queue control block for the queue
**           idenified by qid
*****************************************************************************/
static p2pt_queue_t *
   qcb_for( ULONG qid )
{
    p2pt_queue_t *current_qcb;
    int found_qid;

        if ( queue_list != (p2pt_queue_t *)NULL )
        {
            /*
            **  One or more queues already exist in the queue list...
            **  Scan the existing queues for a matching ID.
            */
            found_qid = FALSE;
            for ( current_qcb = queue_list; 
                  current_qcb != (p2pt_queue_t *)NULL;
                  current_qcb = current_qcb->nxt_queue )
            {
                if ( current_qcb->qid == qid )
                {
                    found_qid = TRUE;
                    break;
                }
            }
            if ( found_qid == FALSE )
                /*
                **  No matching ID found
                */
                current_qcb = (p2pt_queue_t *)NULL;
        }
        else
            current_qcb = (p2pt_queue_t *)NULL;
 
    return( current_qcb );
}


/*****************************************************************************
** new_qid - automatically returns a valid, unused queue ID
*****************************************************************************/
static ULONG
   new_qid( void )
{
    p2pt_queue_t *current_qcb;
    ULONG new_queue_id;

    /*
    **  Protect the queue list while we examine it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&queue_list_lock );
    pthread_mutex_lock( &queue_list_lock );

    /*
    **  Get the highest previously assigned queue id and add one.
    */
    if ( queue_list != (p2pt_queue_t *)NULL )
    {
        /*
        **  One or more queues already exist in the queue list...
        **  Find the highest queue ID number in the existing list.
        */
        new_queue_id = queue_list->qid;
        for ( current_qcb = queue_list; 
              current_qcb->nxt_queue != (p2pt_queue_t *)NULL;
              current_qcb = current_qcb->nxt_queue )
        {
            if ( (current_qcb->nxt_queue)->qid > new_queue_id )
            {
                new_queue_id = (current_qcb->nxt_queue)->qid;
            }
        }

        /*
        **  Add one to the highest existing queue ID
        */
        new_queue_id++;
    }
    else
    {
        /*
        **  this is the first queue being added to the queue list.
        */
        new_queue_id = 1;
    }
 
    /*
    **  Re-enable access to the queue list by other threads.
    */
    pthread_mutex_unlock( &queue_list_lock );
    pthread_cleanup_pop( 0 );

    return( new_queue_id );
}

/*****************************************************************************
** link_qcb - appends a new queue control block pointer to the queue_list
*****************************************************************************/
static void
   link_qcb( p2pt_queue_t *new_queue )
{
    p2pt_queue_t *current_qcb;

    /*
    **  Protect the queue list while we examine and modify it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&queue_list_lock );
    pthread_mutex_lock( &queue_list_lock );

    new_queue->nxt_queue = (p2pt_queue_t *)NULL;
    if ( queue_list != (p2pt_queue_t *)NULL )
    {
        /*
        **  One or more queues already exist in the queue list...
        **  Insert the new entry in ascending numerical sequence by qid.
        */
        for ( current_qcb = queue_list; 
              current_qcb->nxt_queue != (p2pt_queue_t *)NULL;
              current_qcb = current_qcb->nxt_queue )
        {
            if ( (current_qcb->nxt_queue)->qid > new_queue->qid )
            {
                new_queue->nxt_queue = current_qcb->nxt_queue;
                break;
            }
        }
        current_qcb->nxt_queue = new_queue;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd queue cb @ %p to list @ %p", new_queue, current_qcb );
#endif
    }
    else
    {
        /*
        **  this is the first queue being added to the queue list.
        */
        queue_list = new_queue;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd queue cb @ %p to list @ %p", new_queue, &queue_list );
#endif
    }
 
    /*
    **  Re-enable access to the queue list by other threads.
    */
    pthread_mutex_unlock( &queue_list_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** unlink_qcb - removes a queue control block pointer from the queue_list
*****************************************************************************/
static p2pt_queue_t *
   unlink_qcb( ULONG qid )
{
    p2pt_queue_t *current_qcb;
    p2pt_queue_t *selected_qcb;

    selected_qcb =  (p2pt_queue_t *)NULL;

    if ( queue_list != (p2pt_queue_t *)NULL )
    {
        /*
        **  One or more queues exist in the queue list...
        **  Protect the queue list while we examine and modify it.
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&queue_list_lock );
        pthread_mutex_lock( &queue_list_lock );

        /*
        **  Scan the queue list for a qcb with a matching queue ID
        */
        if ( queue_list->qid == qid )
        {
            /*
            **  The first queue in the list matches the selected queue ID
            */
            selected_qcb = queue_list; 
            queue_list = selected_qcb->nxt_queue;
#ifdef DIAG_PRINTFS 
            printf( "\r\ndel queue cb @ %p from list @ %p", selected_qcb,
                    &queue_list );
#endif
        }
        else
        {
            /*
            **  Scan the next qcb for a matching qid while retaining a
            **  pointer to the current qcb.  If the next qcb matches,
            **  select it and then unlink it from the queue list.
            */
            for ( current_qcb = queue_list; 
                  current_qcb->nxt_queue != (p2pt_queue_t *)NULL;
                  current_qcb = current_qcb->nxt_queue )
            {
                if ( (current_qcb->nxt_queue)->qid == qid )
                {
                    /*
                    **  Queue ID of next qcb matches...
                    **  Select the qcb and then unlink it by linking
                    **  the selected qcb's next qcb into the current qcb.
                    */
                    selected_qcb = current_qcb->nxt_queue;
                    current_qcb->nxt_queue = selected_qcb->nxt_queue;
#ifdef DIAG_PRINTFS 
                    printf( "\r\ndel queue cb @ %p from list @ %p",
                            selected_qcb, current_qcb );
#endif
                    break;
                }
            }
        }

        /*
        **  Re-enable access to the queue list by other threads.
        */
        pthread_mutex_unlock( &queue_list_lock );
        pthread_cleanup_pop( 0 );
    }

    return( selected_qcb );
}

/*****************************************************************************
** urgent_msg_to - sends a message to the front of the specified queue
*****************************************************************************/
static void
    urgent_msg_to( p2pt_queue_t *queue, q_msg_t msg )
{
    q_extent_t *cur_extent;
    q_extent_t *prv_extent;
    q_msg_t *first_msg_in_extent;
    q_msg_t *last_msg_in_extent;
    q_msg_t *last_msg_in_prv_extent;
    int i, max_msg;

    /*
    **  It is assumed when we enter this function that the queue has space
    **  to accept the message about to be sent. 
    **  Locate the extent containing the queue_head (Most of the time
    **  there will be only one extent) and establish the range of
    **  valid message pointers in that extent.
    */
    prv_extent = (q_extent_t *)NULL;
    last_msg_in_prv_extent = (q_msg_t *)NULL;
    max_msg = queue->msgs_per_extent - 1;
    cur_extent = queue->first_extent;
    first_msg_in_extent = &(cur_extent->msgs[0]);
    last_msg_in_extent = &(cur_extent->msgs[max_msg + 1]);
    if ( (queue->queue_head < first_msg_in_extent) ||
         (queue->queue_head > last_msg_in_extent) )
    {
        /*
        **  queue_head is not in the first extent... find the right extent.
        */
        prv_extent = cur_extent;
        last_msg_in_prv_extent = last_msg_in_extent;
        for ( cur_extent = (q_extent_t *)(queue->first_extent)->nxt_extent;
              cur_extent != (q_extent_t *)NULL;
              cur_extent = (q_extent_t *)(cur_extent->nxt_extent) )
        {
            first_msg_in_extent = &(cur_extent->msgs[0]);
            last_msg_in_extent = &(cur_extent->msgs[max_msg]);
            if ( (queue->queue_head >= first_msg_in_extent) &&
                 (queue->queue_head <= last_msg_in_extent) )
                break;
            prv_extent = cur_extent;
            last_msg_in_prv_extent = last_msg_in_extent;
        }
    }

    /*
    **  Found the extent containing the queue_head.
    **  Now decrement the queue_head (fetch) pointer, adjusting for
    **  possible wrap either to the end of the previous extent or to
    **  the end of the last extent.  (Urgent messages are placed at
    **  the queue head so they will be the next message fetched from
    **  the queue - ahead of any previously-queued messages.)
    */
    queue->queue_head--;
    if ( queue->queue_head < first_msg_in_extent )
    {
        /*
        **  New queue_head pointer underflowed beginning of current extent...
        **  see if there's another extent preceding the current one.
        */
        if ( prv_extent != (q_extent_t *)NULL )
        {
            /*
            **  Another extent precedes this one in the extent list...
            **  Wrap the queue_head pointer to the last message address
            **  in the previous extent.
            */
            queue->queue_head = last_msg_in_prv_extent;
        }
        else
        {
            /*
            **  The current extent was the first (or only) extent in the list...
            **  Wrap the queue_head pointer to the last message address
            **  in the last extent allocated for the queue.
            */
            queue->queue_head = queue->last_msg_in_queue;
        }
    }

#ifdef DIAG_PRINTFS 
        printf( " new queue_head @ %p", queue->queue_head );
#endif

    for ( i = 0; i < 4; i++ )
         (*(queue->queue_head))[i] = msg[i];

#ifdef DIAG_PRINTFS 
        printf( "\r\nsent urgent msg %p to queue_head @ %p", msg,
                queue->queue_head );
#endif

    /*
    **  Increment the message counter for the queue
    */
    queue->msg_count++;
}

/*****************************************************************************
** send_msg_to - sends the specified message to the tail of the specified queue
*****************************************************************************/
static void
    send_msg_to( p2pt_queue_t *queue, q_msg_t msg )
{
    q_extent_t *cur_extent;
    q_msg_t *first_msg_in_extent;
    q_msg_t *last_msg_in_extent;
    int i, max_msg;

    /*
    **  It is assumed when we enter this function that the queue has space
    **  to accept the message about to be sent.  Start by sending the
    **  message.
    */
    for ( i = 0; i < 4; i++ )
        (*(queue->queue_tail))[i] = msg[i];

#ifdef DIAG_PRINTFS 
    printf( "\r\nsent msg %lx%lx%lx%lx to queue_tail @ %p",
            msg[0], msg[1], msg[2], msg[3], queue->queue_tail );
#endif

    /*
    **  Locate the extent containing the queue_tail just sent into.
    **  (Most of the time there will be only one extent.)
    **  Establish the range of valid message pointers in the current extent.
    */
    max_msg = queue->msgs_per_extent - 1;
    cur_extent = queue->first_extent;
    first_msg_in_extent = &(cur_extent->msgs[0]);
    last_msg_in_extent = &(cur_extent->msgs[max_msg + 1]);
    if ( (queue->queue_tail < first_msg_in_extent) ||
         (queue->queue_tail > last_msg_in_extent) )
    {
        /*
        **  queue_tail is not in the first extent... find the right extent.
        */
        for ( cur_extent = (q_extent_t *)(queue->first_extent)->nxt_extent;
              cur_extent != (q_extent_t *)NULL;
              cur_extent = (q_extent_t *)(cur_extent->nxt_extent) )
        {
            first_msg_in_extent = &(cur_extent->msgs[0]);
            last_msg_in_extent = &(cur_extent->msgs[max_msg]);
            if ( (queue->queue_tail >= first_msg_in_extent) &&
                 (queue->queue_tail <= last_msg_in_extent) )
                break;
        }
    }

    /*
    **  Found the extent containing the queue_tail just sent into.
    **  Now increment the queue_tail (send) pointer, adjusting for
    **  possible wrap either to the beginning of the next extent or to
    **  the beginning of the first extent.
    */
    queue->queue_tail++;
    if ( queue->queue_tail > last_msg_in_extent )
    {
        /*
        **  New queue_tail pointer overflowed end of current extent...
        **  see if there's another extent following the current one.
        */
        if ( cur_extent->nxt_extent != (void *)NULL )
        {
            /*
            **  Another extent follows in the extent list...
            **  Wrap the queue_tail pointer to the first message address
            **  in the next extent.
            */
            cur_extent = (q_extent_t *)(cur_extent->nxt_extent);
        }
        else
        {
            /*
            **  The current extent was the last (or only) extent in the list...
            **  Wrap the queue_tail pointer to the first message address
            **  in the first extent.
            */
            cur_extent = queue->first_extent;
        }
        queue->queue_tail = &(cur_extent->msgs[0]);
    }

#ifdef DIAG_PRINTFS 
        printf( " new queue_tail @ %p", queue->queue_tail );
#endif

    /*
    **  Increment the message counter for the queue
    */
    queue->msg_count++;

    /*
    **  Signal the condition variable for the queue
    */
    pthread_cond_broadcast( &(queue->queue_send) );
}

/*****************************************************************************
** fetch_msg_from - fetches the next message from the specified queue
*****************************************************************************/
static void
    fetch_msg_from( p2pt_queue_t *queue, q_msg_t msg )
{
    q_extent_t *cur_extent;
    q_msg_t *first_msg_in_extent;
    q_msg_t *last_msg_in_extent;
    int i, max_msg;

    /*
    **  It is assumed when we enter this function that the queue contains
    **  one or more messages to be fetched.
    **  Fetch the message from the queue_head message location.
    */
    if ( msg != (ULONG *)NULL )
    {
        for ( i = 0; i < 4; i++ )
            msg[i] = (*(queue->queue_head))[i];
    }

#ifdef DIAG_PRINTFS 
    printf( "\r\nfetched msg %lx%lx%lx%lx from queue_head @ %p",
            msg[0], msg[1], msg[2], msg[3], queue->queue_head );
#endif
    if ( queue->send_type == BCAST )
        queue->bcst_tasks_awakened++;
    /*
    **  For normally sent messages, we will clear the message immediately...
    **  however, for queue_broadcast messages we will only clear the
    **  message after the last pended task has been awakened.
    */
    if ( (queue->send_type == SEND) ||
         (queue->first_susp == (p2pthread_cb_t *)NULL) )
    {
        for ( i = 0; i < 4; i++ )
            (*(queue->queue_head))[i] = (ULONG)(NULL);

        /*
        **  Locate the extent containing the queue_head just fetched from.
        **  (Most of the time there will be only one extent.)
        **  Establish the range of valid message pointers in the extent.
        */
        max_msg = queue->msgs_per_extent - 1;
        cur_extent = queue->first_extent;
        first_msg_in_extent = &(cur_extent->msgs[0]);
        last_msg_in_extent = &(cur_extent->msgs[max_msg + 1]);
        if ( (queue->queue_head < first_msg_in_extent) ||
             (queue->queue_head > last_msg_in_extent) )
        {
            /*
            **  queue_head is not in the first extent... find the right extent.
            */
            for ( cur_extent = (q_extent_t *)(queue->first_extent)->nxt_extent;
                  cur_extent != (q_extent_t *)NULL;
                  cur_extent = (q_extent_t *)(cur_extent->nxt_extent) )
            {
                first_msg_in_extent = &(cur_extent->msgs[0]);
                last_msg_in_extent = &(cur_extent->msgs[max_msg]);
                if ( (queue->queue_head >= first_msg_in_extent) &&
                     (queue->queue_head <= last_msg_in_extent) )
                    break;
            }
        }

        /*
        **  Found the extent containing the queue_head just sent into.
        **  Now increment the queue_head (send) pointer, adjusting for
        **  possible wrap either to the beginning of the next extent or to
        **  the beginning of the first extent.
        */
        queue->queue_head++;
        if ( queue->queue_head > last_msg_in_extent )
        {
            /*
            **  New queue_head pointer overflowed end of current extent...
            **  see if there's another extent following the current one.
            */
            if ( cur_extent->nxt_extent != (void *)NULL )
            {
                /*
                **  Another extent follows in the extent list...
                **  Wrap the queue_head pointer to the first message address
                **  in the next extent.
                */
                cur_extent = (q_extent_t *)(cur_extent->nxt_extent);
            }
            else
            {
                /*
                **  The current extent was the last (or only) one in the list...
                **  Wrap the queue_head pointer to the first message address
                **  in the first extent.
                */
                cur_extent = queue->first_extent;
            }
            queue->queue_head = &(cur_extent->msgs[0]);
        }

#ifdef DIAG_PRINTFS 
        printf( " new queue_head @ %p", queue->queue_head );
#endif

        /*
        **  Decrement the message counter for the queue
        */
        queue->msg_count--;

        /*
        **  If the message just fetched was a broadcast message, then
        **  this was the last task pending on the queue... Signal the
        **  broadcast-complete condition variable.
        */
        if ( queue->send_type != SEND )
        {
            /*
            ** Lock mutex for queue broadcast completion
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(queue->qbcst_lock) );
            pthread_mutex_lock( &(queue->qbcst_lock) );

            /*
            **  Signal the broadcast-complete condition variable for the queue
            */
            pthread_cond_broadcast( &(queue->queue_bcplt) );

            queue->send_type = SEND;

            /*
            **  Unlock the queue broadcast completion mutex. 
            */
            pthread_mutex_unlock( &(queue->qbcst_lock) );
            pthread_cleanup_pop( 0 );
        }
    }
}

/*****************************************************************************
** new_extent_for - allocates space for queue data.  Data is allocated in
**                  blocks large enough to hold (qsize) messages plus
**                  the extent link pointer.  If a queue needs more room,
**                  additional extents of the same size are added.
*****************************************************************************/
static q_extent_t *
    new_extent_for( p2pt_queue_t *queue, ULONG qsize )
{
    q_extent_t *new_extent;
    q_extent_t *nxt_extent;
    size_t block_size;

    /*
    **  Calculate the number of bytes of memory needed for this extent.
    **  Start with  space required for an array of qsize messages.
    **  Then add the size of the link to the next extent plus one extra
    **  message for urgent queue sends.
    */

    block_size = sizeof( q_msg_t ) * qsize;

    /*
    **  The q_extent_t contains a void pointer to the next extent in the
    **  queue followed by a q_msg_t for the urgent message space.  Any data
    **  alignment spaces in the header will be taken into account here, too.
    */
    block_size += sizeof( q_extent_t );

    /*
    **  Now allocate a block of memory to contain the extent.
    */
    if ( (new_extent = (q_extent_t *)ts_malloc( block_size )) !=
         (q_extent_t *)NULL )
    {
        /*
        **  Clear the memory block.  Note that this creates a NULL pointer
        **  for the nxt_extent link as well as zeroing the message array.
        */
        bzero( (void *)new_extent, (int)block_size );

        /*
        ** Increment total number of extents currently allocated for queue
        */
        queue->total_extents++;

        /*
        **  Find the last extent in the extent list
        */
        if ( queue->first_extent == (q_extent_t *)NULL )
        {
            /* Extent list is empty... new_extent is first_extent */
            queue->first_extent = new_extent;
        }
        else
        {
            /* Find end of extent list and append new_extent onto it */
            for ( nxt_extent = queue->first_extent;
                  nxt_extent->nxt_extent != (q_extent_t *)NULL;
                  nxt_extent = (q_extent_t *)(nxt_extent->nxt_extent) );
            nxt_extent->nxt_extent = new_extent;
        }
        queue->last_msg_in_queue = &(new_extent->msgs[qsize]);
    }
#ifdef DIAG_PRINTFS 
    printf( "\r\nnew extent @ %p for queue @ %p", new_extent, queue );
#endif
    return( new_extent );
}

/*****************************************************************************
** q_create - creates a p2pthread message queue
*****************************************************************************/
ULONG
    q_create( char name[4], ULONG qsize, ULONG opt, ULONG *qid )
{
    p2pt_queue_t *queue;
    ULONG error;
    int i;

    error = ERR_NO_ERROR;

    /*
    **  First allocate memory for the queue control block
    */
    queue = (p2pt_queue_t *)ts_malloc( sizeof( p2pt_queue_t ) );
    if ( queue != (p2pt_queue_t *)NULL )
    {
        /*
        **  Ok... got a control block.
        **  Now allocate memory for the first queue data extent.
        */

        /*
        ** Option Flags for queue
        */
        queue->flags = opt;

        queue->total_extents = 0;
        if ( new_extent_for( queue, qsize ) != (q_extent_t *)NULL )
        {
            /*
            **  Got both a control block and a data extent...
            **  Initialize the control block.
            */

            /*
            ** ID for queue
            */
            queue->qid = new_qid();
            if ( qid != (ULONG *)NULL )
                *qid = queue->qid;

            /*
            **  Name for queue
            */
            for ( i = 0; i < 4; i++ )
                queue->qname[i] = name[i];

            /*
            ** Mutex and Condition variable for queue send/pend
            */
            pthread_mutex_init( &(queue->queue_lock),
                                (pthread_mutexattr_t *)NULL );
            pthread_cond_init( &(queue->queue_send),
                               (pthread_condattr_t *)NULL );

            /*
            ** Mutex and Condition variable for queue broadcast/delete
            */
            pthread_mutex_init( &(queue->qbcst_lock),
                                (pthread_mutexattr_t *)NULL );
            pthread_cond_init( &(queue->queue_bcplt),
                               (pthread_condattr_t *)NULL );

            if ( qsize > 0 )
            {
                /*
                **  Pointer to next message pointer to be fetched from queue
                */
                queue->queue_head = &(queue->first_extent->msgs[1]);

                /*
                **  Pointer to last message pointer sent to queue
                */
                queue->queue_tail = &(queue->first_extent->msgs[1]);
            }
            else
            {
                /*
                **  Pointer to next message pointer to be fetched from queue
                */
                queue->queue_head = &(queue->first_extent->msgs[0]);

                /*
                **  Pointer to last message pointer sent to queue
                */
                queue->queue_tail = &(queue->first_extent->msgs[0]);
            }

            /*
            ** Type of send operation last performed on queue
            */
            queue->send_type = SEND;

            /*
            ** First task control block in list of tasks waiting on queue
            */
            queue->first_susp = (p2pthread_cb_t *)NULL;

            /*
            **  Count of tasks awakened by q_broadcast call
            */
            queue->bcst_tasks_awakened = 0;

            /*
            ** Total messages per memory allocation block (extent)
            ** (First extent has one extra for urgent message.)
            */
            queue->msgs_per_extent = qsize;

            /*
            ** Total number of messages currently sent to queue
            */
            queue->msg_count = 0;

            /*
            **  If no errors thus far, we have a new queue ready to link into
            **  the queue list.
            */
            if ( error == ERR_NO_ERROR )
            {
                link_qcb( queue );
            }
            else
            {
                /*
                **  Oops!  Problem somewhere above.  Release control block
                **  and data memory and return.
                */
                ts_free( (void *)queue->first_extent );
                ts_free( (void *)queue );
            }
        }
        else
        {
            /*
            **  No memory for queue data... free queue control block & return
            */
            ts_free( (void *)queue );
            error = ERR_NOMGB;
        }
    }
    else
    {
        error = ERR_NOQCB;
    }

    return( error );
}

/*****************************************************************************
** q_urgent - sends a message to the front of a p2pthread queue and awakens the
**           first selected task waiting on the queue.
*****************************************************************************/
ULONG
   q_urgent( ULONG qid, q_msg_t msg )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pt_queue_t *queue;
    q_extent_t *new_extent;
    ULONG error;
    int qsize;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_queue_t *)NULL )
    {
#ifdef DIAG_PRINTFS 
        our_tcb = my_tcb();
        printf( "\r\ntask @ %p urgent send to queue list @ %p", our_tcb,
                &(queue->first_susp) );
#endif

        /*
        **  'Lock the p2pthread scheduler' to defer any context switch to a
        **  higher priority task until after this call has completed its work.
        */
        sched_lock();

        /*
        ** Lock mutex for queue send
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        /*
        **  See how many messages are already sent into the queue
        */
        if ( queue->msg_count >
             (queue->total_extents * queue->msgs_per_extent) )
        {
            /*
            **  Queue is already full... try to add another extent.
            **  (First extent has msgs_per_extent + 1 messages...
            **  any additional extents have only msgs_per_extent.  Extent
            **  header contains one message, so decrement msg count by one
            **  before allocating space for extent.)
            */
            qsize = queue->msgs_per_extent - 1;
            if ( (!(queue->flags & Q_LIMIT)) &&
                 ((new_extent = new_extent_for( queue, qsize ))
                  != (q_extent_t *)NULL) )
            {
                /*
                **  The queue_tail would have wrapped to the head of
                **  the first extent... change it to the first message
                **  in the newly-added extent.
                */
                queue->queue_tail = &(new_extent->msgs[0]);

                /*
                **  Stuff the new message onto the front of the queue.
                */
                urgent_msg_to( queue, msg );

                /*
                **  Signal the condition variable for the queue
                */
                pthread_cond_broadcast( &(queue->queue_send) );
            }
            else
                /*
                **  No memory for more extents... return QUEUE FULL error
                */
                error = ERR_QFULL;
        }
        else
        {
            /*
            **  Stuff the new message onto the front of the queue.
            */
            urgent_msg_to( queue, msg );

            /*
            **  Signal the condition variable for the queue
            */
            pthread_cond_broadcast( &(queue->queue_send) );
        }

        /*
        **  Unlock the queue mutex. 
        */
        pthread_mutex_unlock( &(queue->queue_lock) );
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
** q_send - posts a message to the tail of a p2pthread queue and awakens the
**          first selected task waiting on the queue.
*****************************************************************************/
ULONG
   q_send( ULONG qid, q_msg_t msg )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pt_queue_t *queue;
    q_extent_t *new_extent;
    int qsize;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_queue_t *)NULL )
    {
#ifdef DIAG_PRINTFS 
        our_tcb = my_tcb();
        printf( "\r\ntask @ %p send to queue list @ %p", our_tcb,
                &(queue->first_susp) );
#endif

        /*
        **  'Lock the p2pthread scheduler' to defer any context switch to a
        **  higher priority task until after this call has completed its work.
        */
        sched_lock();

        /*
        **  Lock mutex for queue send. Note: since i call pthread_cleanup_push()
        **  before pthread_mutex_lock in every thread, so even though i call
        **  sched_lock() above and get the scheduler controller, the queue->queue_lock
        **  has been unlocked by other thread. 
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        /*
        **  See how many messages are already sent into the queue
        */
        if ( queue->msg_count >
             (queue->total_extents * queue->msgs_per_extent) )
        {
            /*
            **  Queue is already full... try to add another extent.
            **  (First extent has msgs_per_extent + 1 messages...
            **  any additional extents have only msgs_per_extent.  Extent
            **  header contains one message, so decrement msg count by one
            **  before allocating space for extent.)
            */
            qsize = queue->msgs_per_extent - 1;
            if ( (!(queue->flags & Q_LIMIT)) &&
                 ((new_extent = new_extent_for( queue, qsize ))
                  != (q_extent_t *)NULL) )
            {
                /*
                **  The queue_tail would have wrapped to the head of
                **  the first extent... change it to the first message
                **  in the newly-added extent.
                */
                queue->queue_tail = &(new_extent->msgs[0]);

                /*
                **  Send the new message.
                */
                send_msg_to( queue, msg );
            }
            else
                /*
                **  No memory for more extents... return QUEUE FULL error
                */
                error = ERR_QFULL;
        }
        else
        {
            /*
            **  Message count is equal or below maximum...
            */
            if ( queue->msg_count ==
                 (queue->total_extents * queue->msgs_per_extent) )
            {
                /*
                **  Queue is already full... try to add another extent.
                */
                qsize = queue->msgs_per_extent - 1;
                if ( !(queue->flags & Q_LIMIT) )
                {
                    /*
                    **  Try to add another extent.
                    */
                    if ( (new_extent = new_extent_for( queue, qsize ))
                         != (q_extent_t *)NULL )
                    {
                        /*
                        **  The queue_tail would have wrapped to the head of
                        **  the first extent... change it to the first message
                        **  in the newly-added extent.
                        */
                        queue->queue_tail = &(new_extent->msgs[0]);

                        /*
                        **  Send the new message.
                        */
                        send_msg_to( queue, msg );
                    }
                    else
                        /*
                        **  No memory for more extents... return QUEUE FULL
                        */
                        error = ERR_QFULL;
                }
                else
                {
                    if ( (queue->msgs_per_extent == 0) &&
                         (queue->first_susp != (p2pthread_cb_t *)NULL) )
                    {
                        /*
                        **  Special case... This case represents the zero size
                        **  queue, send the new message.
                        */
                        send_msg_to( queue, msg );
                    }
                    else
                    {
                        /*
                        **  No more extents allowed... return QUEUE FULL error
                        */
                        error = ERR_QFULL;
                    }
                }
            }
            else
            {
                /*
                **  Message count below maximum... Send the new message.
                */
                send_msg_to( queue, msg );
            }
        }

        /*
        **  Unlock the queue mutex. 
        */
        pthread_mutex_unlock( &(queue->queue_lock) );
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
** q_broadcast - sends the specified message to all tasks pending on the
**               specified p2pthread queue and awakens the tasks.
*****************************************************************************/
ULONG
   q_broadcast( ULONG qid, q_msg_t msg, ULONG *count )
{
    p2pt_queue_t *queue;
    q_extent_t *new_extent;
    ULONG error;
    int qsize;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_queue_t *)NULL )
    {
        /*
        ** Lock mutex for urgent queue send
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        if ( queue->first_susp != (p2pthread_cb_t *)NULL )
        {

            /*
            **  See how many messages are already sent into the queue
            */
            if ( queue->msg_count >
                (queue->total_extents * queue->msgs_per_extent) )
            {
                /*
                **  Queue is already full... try to add another extent.
                **  (First extent has msgs_per_extent + 1 messages...
                **  any additional extents have only msgs_per_extent.  Extent
                **  header contains one message, so decrement msg count by one
                **  before allocating space for extent.)
                */
                qsize = queue->msgs_per_extent - 1;
                if ( (!(queue->flags & Q_LIMIT)) &&
                     ((new_extent = new_extent_for( queue, qsize ))
                      != (q_extent_t *)NULL) )
                {
                    /*
                    **  The queue_tail would have wrapped to the start of
                    **  the first extent... change it to the first message
                    **  in the newly-added extent.
                    */
                    queue->queue_tail = &(new_extent->msgs[0]);

                    /*
                    **  Stuff the new message onto the front of the queue.
                    */
                    urgent_msg_to( queue, msg );

                    /*
                    **  Declare the send type
                    */
                    queue->send_type = BCAST;
                }
                else
                    /*
                    **  No memory for more extents... return QUEUE FULL error
                    */
                    error = ERR_QFULL;
            }
            else
            {
                /*
                **  Stuff the new message onto the front of the queue.
                */
                urgent_msg_to( queue, msg );

                /*
                **  Declare the send type
                */
                queue->send_type = BCAST;
            }
        }

        /*
        **  Unlock the queue mutex. 
        */
        pthread_mutex_unlock( &(queue->queue_lock) );
        pthread_cleanup_pop( 0 );

        /*
        **  Send msg and block while any tasks are still pended on the queue.
        */
        queue->bcst_tasks_awakened = 0;

		/*
		**  Note: when i call pthread_cond_wait() below, the task is in it's ideal
		**  state, and give up it's scheduler control, so the task other than me 
		**  can go on the way, and send cont_t queue->queue_bcplt back. That's 
		**  why it has no effect i call sched_lock() below. */
        sched_lock();
        if ( (error == ERR_NO_ERROR) &&
                (queue->first_susp != (p2pthread_cb_t *)NULL) )
        {
            /*
            ** Lock mutex for queue broadcast completion
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(queue->qbcst_lock) );
            pthread_mutex_lock( &(queue->qbcst_lock) );

            /*
            **  Lock mutex for urgent queue send, so while i was broadcasting 
			**  the urgent message, no one can disturb me( send another message
			**  to the queue), after broadcasting, release the queue_lock, and 
			**  the pthread call pthread_cond_wait() will get the message and
			**  mutex lock queue_lock again.  
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(queue->queue_lock));
            pthread_mutex_lock( &(queue->queue_lock) );

            /*
            **  Signal the condition variable for the queue, wake up the task
			**  block on the ev_receive() call.
            */
            pthread_cond_broadcast( &(queue->queue_send) );

            /*
            **  Unlock the queue mutex. 
            */
            pthread_mutex_unlock( &(queue->queue_lock) );
            pthread_cleanup_pop( 0 );

            /*
            **  Wait for all pended tasks to receive broadcast message.
            **  The last task to receive the message will signal the
            **  broadcast-complete condition variable.
            */
            while ( queue->first_susp != (p2pthread_cb_t *)NULL )
                pthread_cond_wait( &(queue->queue_bcplt),
                                   &(queue->qbcst_lock) );

            /*
            **  Unlock the queue broadcast completion mutex. 
            */
            pthread_mutex_unlock( &(queue->qbcst_lock) );
            pthread_cleanup_pop( 0 );
        }

        *count = queue->bcst_tasks_awakened;

        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** delete_queue - takes care of destroying the specified queue and freeing
**                any resources allocated for that queue
*****************************************************************************/
static void
   delete_queue( p2pt_queue_t *queue )
{
    q_extent_t *
        current_extent;
    q_extent_t *
        next_extent;

    /*
    **  First remove the queue from the queue list
    */
    unlink_qcb( queue->qid );

    /*
    **  Next delete all extents allocated for queue data.
    */
    next_extent = (q_extent_t *)NULL;
    for ( current_extent = queue->first_extent;
          current_extent != (q_extent_t *)NULL;
          current_extent = next_extent )
    {
        next_extent = (q_extent_t *)current_extent->nxt_extent;
        ts_free( (void *)current_extent );
    }

    /*
    **  Finally delete the queue control block itself;
    */
    ts_free( (void *)queue );

}

/*****************************************************************************
** q_delete - removes the specified queue from the queue list and frees
**              the memory allocated for the queue control block and extents.
*****************************************************************************/
ULONG
   q_delete( ULONG qid )
{
    p2pt_queue_t *queue;
    ULONG error;
    static char deleted_msg[16] = "Queue Deleted!\n";

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_queue_t *)NULL )
    {
        /*
        ** Lock mutex for queue delete
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        if ( queue->msg_count )
            error = ERR_MATQDEL;

        if ( queue->first_susp != (p2pthread_cb_t *)NULL )
        {
            /*
            **  Stuff the new message onto the front of the queue.
            */
            urgent_msg_to( queue, (ULONG *)deleted_msg );

            /*
            **  Declare the send type
            */
            queue->send_type = KILLD;

            error = ERR_TATQDEL;
        }

        /*
        **  Unlock the queue mutex. 
        */
        pthread_mutex_unlock( &(queue->queue_lock) );
        pthread_cleanup_pop( 0 );

        /*
        **  Send msg and block while any tasks are still pended on the queue
        */
        sched_lock();
        if ( queue->first_susp != (p2pthread_cb_t *)NULL )
        {
            /*
            ** Lock mutex for queue broadcast completion
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(queue->qbcst_lock) );
            pthread_mutex_lock( &(queue->qbcst_lock) );

            /*
            ** Lock mutex for urgent queue send
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(queue->queue_lock));
            pthread_mutex_lock( &(queue->queue_lock) );

            /*
            **  Signal the condition variable for the queue
            */
            pthread_cond_broadcast( &(queue->queue_send) );

            /*
            **  Unlock the queue mutex. 
            */
            pthread_mutex_unlock( &(queue->queue_lock) );
            pthread_cleanup_pop( 0 );

            /*
            **  Wait for all pended tasks to receive broadcast message.
            **  The last task to receive the message will signal the
            **  broadcast-complete condition variable.
            */
            while ( queue->first_susp != (p2pthread_cb_t *)NULL )
                pthread_cond_wait( &(queue->queue_bcplt),
                                   &(queue->qbcst_lock) );

            /*
            **  Unlock the queue broadcast completion mutex. 
            */
            pthread_mutex_unlock( &(queue->qbcst_lock) );
            pthread_cleanup_pop( 0 );
        }
        delete_queue( queue );
        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** waiting_on_queue - returns a nonzero result unless a qualifying event
**                    occurs on the specified queue which should cause the
**                    pended task to be awakened.  The qualifying events
**                    are:
**                        (1) a message is sent to the queue and the current
**                            task is selected to receive it
**                        (2) a broadcast message is sent to the queue
**                        (3) the queue is deleted
*****************************************************************************/
static int
    waiting_on_queue( p2pt_queue_t *queue, struct timespec *timeout,
                      int *retcode )
{
    int result;
    struct timeval now;
    ULONG usec;

    if ( queue->send_type & KILLD )
    {
        /*
        **  Queue has been killed... waiting is over.
        */
        result = 0;
        *retcode = 0;
    }
    else
    {
        /*
        **  Queue still in service... check for message availability.
        **  Initially assume no message for our task
        */
        result = 1;

        /*
        **  Multiple messages sent to the queue may be represented by only
        **  a single signal to the condition variable, so continue
        **  checking for a message for our task as long as more messages
        **  are available.
        */
        while ( queue->msg_count > 0 )
        {
            /*
            **  Message arrived... see if it's for our task.
            */
            if ( (queue->send_type & BCAST) ||
                 (signal_for_my_task( &(queue->first_susp),
                                      (queue->flags & Q_PRIOR) )) )
            {
                /*
                **  Message was either broadcast for all tasks or was
                **  destined for our task specifically... waiting is over.
                */
                result = 0;
                *retcode = 0;
                break;
            }
            else
            {
                /*
                **  Message isn't for our task... continue waiting.
                **  Sleep awhile to allow other tasks ahead of ours in the
                **  list of tasks waiting on the queue to get their
                **  messages, bringing our task to the head of the list.
                */
                pthread_mutex_unlock( &(queue->queue_lock) );
                tm_wkafter( 1 );
                pthread_mutex_lock( &(queue->queue_lock) );
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
** q_receive - blocks the calling task until a message is available in the
**             specified p2pthread queue.
*****************************************************************************/
ULONG
   q_receive( ULONG qid, ULONG opt, ULONG max_wait, q_msg_t msg )
{
    p2pthread_cb_t *our_tcb;
    struct timeval now;
    struct timespec timeout;
    int retcode;
    long sec, usec;
    p2pt_queue_t *queue;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_queue_t *)NULL )
    {

        /*
        ** Lock mutex for queue receive
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        /*
        **  If a broadcast is in progress, wait for it to complete
        **  before adding our TCB to the queue's waiting list.
        */
        while ( queue->send_type != SEND )
        {
            pthread_mutex_unlock( &(queue->queue_lock) );
            tm_wkafter( 1 );
            pthread_mutex_lock( &(queue->queue_lock) );
        }

        /*
        **  Add tcb for task to list of tasks waiting on queue
        */
        our_tcb = my_tcb();
#ifdef DIAG_PRINTFS 
        printf( "\r\ntask @ %p wait on queue list @ %p", our_tcb,
                &(queue->first_susp) );
#endif

        link_susp_tcb( &(queue->first_susp), our_tcb );

        retcode = 0;

        if ( opt & Q_NOWAIT )
        {
            /*
            **  Caller specified no wait on queue message...
            **  Check the condition variable with an immediate timeout.
            */
            gettimeofday( &now, (struct timezone *)NULL );
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = now.tv_usec * 1000;
            while ( (waiting_on_queue( queue, &timeout, &retcode )) &&
                    (retcode != ETIMEDOUT) )
            {
			    /*
				**  Note: the purpose waiting_on_queue() is to wait until
				**  the queue doesn't own any messages, and then it blocks
				**  on the sentence below to wait a new message.
				*/
                retcode = pthread_cond_timedwait( &(queue->queue_send),
                                                  &(queue->queue_lock),
                                                  &timeout );
            }
        }
        else
        {
            /*
            **  Caller expects to wait on queue, with or without a timeout.
            */
            if ( max_wait == 0L )
            {
                /*
                **  Infinite wait was specified... wait without timeout.
                */
                while ( waiting_on_queue( queue, 0, &retcode ) )
                {
                    pthread_cond_wait( &(queue->queue_send),
                                       &(queue->queue_lock) );
                }
            }
            else
            {
                /*
                **  Wait on queue message arrival with timeout...
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
                **  Wait for a queue message for the current task or for the
                **  timeout to expire.  The loop is required since the task
                **  may be awakened by signals for messages which are
                **  not ours, or for signals other than from a message send.
                */
                while ( (waiting_on_queue( queue, &timeout, &retcode )) &&
                        (retcode != ETIMEDOUT) )
                {
                    retcode = pthread_cond_timedwait( &(queue->queue_send),
                                                      &(queue->queue_lock),
                                                      &timeout );
                }
            }
        }

        /*
        **  Remove the calling task's tcb from the pended task list
        **  for the queue.
        */
        unlink_susp_tcb( &(queue->first_susp), our_tcb );

        /*
        **  See if we were awakened due to a q_delete on the queue.
        */
        if ( queue->send_type & KILLD )
        {
            fetch_msg_from( queue, msg );
            error = ERR_QKILLD;
            msg = (ULONG *)NULL;
#ifdef DIAG_PRINTFS 
            printf( "...queue deleted" );
#endif
        }
        else
        {
            /*
            **  See if we timed out or if we got a message
            */
            if ( retcode == ETIMEDOUT )
            {
                /*
                **  Timed out without a message
                */
                if ( opt & Q_NOWAIT )
                    error = ERR_NOMSG;
                else
                    error = ERR_TIMEOUT;
                msg = (ULONG *)NULL;
#ifdef DIAG_PRINTFS 
                printf( "...timed out" );
#endif
            }
            else
            {
                /*
                **  A message was sent to the queue for this task...
                **  Retrieve the message and clear the queue contents.
                */
                fetch_msg_from( queue, msg );
#ifdef DIAG_PRINTFS 
                printf( "...rcvd queue msg %lu%lu%lu%lu",
                         msg[0], msg[1], msg[2], msg[3] );
#endif
            }
        }

        /*
        **  Unlock the mutex for the condition variable and clean up.
        */
        pthread_mutex_unlock( &(queue->queue_lock) );
        pthread_cleanup_pop( 0 );
    }
    else
    {
        error = ERR_OBJDEL;       /* Invalid queue specified */
        msg = (ULONG *)NULL;
    }

    return( error );
}

/*****************************************************************************
** q_ident - identifies the specified p2pthread queue
*****************************************************************************/
ULONG
    q_ident( char name[4], ULONG node, ULONG *qid )
{
    p2pt_queue_t *current_qcb;
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
        **  If queue name string is a NULL pointer, return with error.
        **  We'll ASSUME the qid pointer isn't NULL!
        */
        if ( name == (char *)NULL )
        {
            *qid = (ULONG)NULL;
            error = ERR_OBJNF;
        }
        else
        {
            /*
            **  Scan the task list for a name matching the caller's name.
            */
            for ( current_qcb = queue_list;
                  current_qcb != (p2pt_queue_t *)NULL;
                  current_qcb = current_qcb->nxt_queue )
            {
                if ( (strncmp( name, current_qcb->qname, 4 )) == 0 )
                {
                    /*
                    **  A matching name was found... return its QID
                    */
                    *qid = current_qcb->qid;
                    break;
                }
            }
            if ( current_qcb == (p2pt_queue_t *)NULL )
            {
                /*
                **  No matching name found... return caller's QID with error.
                */
                *qid = (ULONG)NULL;
                error = ERR_OBJNF;
            }
        }
    }

    return( error );
}
