/*****************************************************************************
 * vqueue.c - defines the wrapper functions and data structures needed
 *            to implement a Wind River pSOS+ (R) variable length queue API 
 *            in a POSIX Threads environment.
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

#define ERR_MSGSIZ   0x31
#define ERR_BUFSIZ   0x32
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
typedef struct q_varlen_msg
{
    ULONG msglen;
    char *msgbuf;
} q_vmsg_t;

/*****************************************************************************
**  Control block for p2pthread queue
**
**  The message list for a queue is organized into an array called an extent.
**  Actual send and fetch operations are done using a queue_head and
**  queue_tail pointer.  These pointers must 'rotate' through the extent to
**  create a logical circular buffer.  A single extra location is added
**  to ensure room for urgent messages or broadcasts even when the queue is
**  'full' for normal messages.
**
*****************************************************************************/
typedef struct p2pt_vqueue
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
        qbcst_cmplt;

        /*
        **  Pointer to next message pointer to be fetched from queue
        */
    q_vmsg_t *
        queue_head;

        /*
        **  Pointer to last message pointer sent to queue
        */
    q_vmsg_t *
        queue_tail;

        /*
        ** Type of send operation last performed on queue
        */
    int
        send_type;

        /*
        **  Pointer to first message in queue
        */
    q_vmsg_t *
        first_msg_in_queue;

        /*
        **  Pointer to last message in queue
        */
    q_vmsg_t *
        last_msg_in_queue;

        /*
        **  Pointer to next queue control block in queue list
        */
    struct p2pt_vqueue *
        nxt_queue;

        /*
        ** First task control block in list of tasks waiting on queue
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
        ** Total messages per queue
        */
    int
        msgs_per_queue;

        /*
        ** Maximum size of messages sent to queue
        */
    ULONG
        msg_len;

        /*
        ** sizeof( each element in queue ) used for subscript incr/decr.
        */
    size_t
        vmsg_len;

        /*
        ** Task pend order (FIFO or Priority) for queue
        */
    int
        order;
} p2pt_vqueue_t;

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
**  vqueue_list is a linked list of queue control blocks.  It is used to locate
**             queues by their ID numbers.
*/
static p2pt_vqueue_t *
    vqueue_list;

/*
**  vqueue_list_lock is a mutex used to serialize access to the queue list
*/
static pthread_mutex_t
    vqueue_list_lock = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************************
** qcb_for - returns the address of the queue control block for the queue
**           idenified by qid
*****************************************************************************/
static p2pt_vqueue_t *
   qcb_for( ULONG qid )
{
    p2pt_vqueue_t *current_qcb;
    int found_qid;

        if ( vqueue_list != (p2pt_vqueue_t *)NULL )
        {
            /*
            **  One or more queues already exist in the queue list...
            **  Scan the existing queues for a matching ID.
            */
            found_qid = FALSE;
            for ( current_qcb = vqueue_list; 
                  current_qcb != (p2pt_vqueue_t *)NULL;
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
                current_qcb = (p2pt_vqueue_t *)NULL;
        }
        else
            current_qcb = (p2pt_vqueue_t *)NULL;
 
    return( current_qcb );
}

/*****************************************************************************
** new_vqid - automatically returns a valid, unused variable length queue ID
*****************************************************************************/
static ULONG
   new_vqid( void )
{
    p2pt_vqueue_t *current_qcb;
    ULONG new_queue_id;

    /*
    **  Protect the queue list while we examine it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&vqueue_list_lock );
    pthread_mutex_lock( &vqueue_list_lock );

    /*
    **  Get the highest previously assigned queue id and add one.
    */
    if ( vqueue_list != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  One or more queues already exist in the queue list...
        **  Find the highest queue ID number in the existing list.
        */
        new_queue_id = vqueue_list->qid;
        for ( current_qcb = vqueue_list; 
              current_qcb->nxt_queue != (p2pt_vqueue_t *)NULL;
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
        new_queue_id = 0;
    }
 
    /*
    **  Re-enable access to the queue list by other threads.
    */
    pthread_mutex_unlock( &vqueue_list_lock );
    pthread_cleanup_pop( 0 );

    return( new_queue_id );
}

/*****************************************************************************
** link_qcb - appends a new queue control block pointer to the vqueue_list
*****************************************************************************/
static void
   link_qcb( p2pt_vqueue_t *new_vqueue )
{
    p2pt_vqueue_t *current_qcb;

    /*
    **  Protect the queue list while we examine and modify it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&vqueue_list_lock );
    pthread_mutex_lock( &vqueue_list_lock );

    new_vqueue->nxt_queue = (p2pt_vqueue_t *)NULL;
    if ( vqueue_list != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  One or more queues already exist in the queue list...
        **  Insert the new entry in ascending numerical sequence by qid.
        */
        for ( current_qcb = vqueue_list; 
              current_qcb->nxt_queue != (p2pt_vqueue_t *)NULL;
              current_qcb = current_qcb->nxt_queue )
        {
            if ( (current_qcb->nxt_queue)->qid > new_vqueue->qid )
            {
                new_vqueue->nxt_queue = current_qcb->nxt_queue;
                break;
            }
        }
        current_qcb->nxt_queue = new_vqueue;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd queue cb @ %p to list @ %p", new_vqueue, current_qcb );
#endif
    }
    else
    {
        /*
        **  this is the first queue being added to the queue list.
        */
        vqueue_list = new_vqueue;
#ifdef DIAG_PRINTFS 
        printf( "\r\nadd queue cb @ %p to list @ %p", new_vqueue, &vqueue_list );
#endif
    }
 
    /*
    **  Re-enable access to the queue list by other threads.
    */
    pthread_mutex_unlock( &vqueue_list_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** unlink_qcb - removes a queue control block pointer from the vqueue_list
*****************************************************************************/
static p2pt_vqueue_t *
   unlink_qcb( ULONG qid )
{
    p2pt_vqueue_t *current_qcb;
    p2pt_vqueue_t *selected_qcb;

    selected_qcb =  (p2pt_vqueue_t *)NULL;

    if ( vqueue_list != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  One or more queues exist in the queue list...
        **  Protect the queue list while we examine and modify it.
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&vqueue_list_lock );
        pthread_mutex_lock( &vqueue_list_lock );

        /*
        **  Scan the queue list for a qcb with a matching queue ID
        */
        if ( vqueue_list->qid == qid )
        {
            /*
            **  The first queue in the list matches the selected queue ID
            */
            selected_qcb = vqueue_list; 
            vqueue_list = selected_qcb->nxt_queue;
#ifdef DIAG_PRINTFS 
            printf( "\r\ndel queue cb @ %p from list @ %p", selected_qcb,
                    &vqueue_list );
#endif
        }
        else
        {
            /*
            **  Scan the next qcb for a matching qid while retaining a
            **  pointer to the current qcb.  If the next qcb matches,
            **  select it and then unlink it from the queue list.
            */
            for ( current_qcb = vqueue_list; 
                  current_qcb->nxt_queue != (p2pt_vqueue_t *)NULL;
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
        pthread_mutex_unlock( &vqueue_list_lock );
        pthread_cleanup_pop( 0 );
    }

    return( selected_qcb );
}

/*****************************************************************************
** urgent_msg_to - sends a message to the front of the specified queue
*****************************************************************************/
static void
    urgent_msg_to( p2pt_vqueue_t *queue, char *msg, ULONG msglen )
{
    ULONG i;
    char *element;

    /*
    **  It is assumed when we enter this function that the queue has space
    **  to accept the message about to be sent. 
    **  Pre-decrement the queue_head (fetch) pointer, adjusting for
    **  possible wrap to the end of the queue;
    **  (Urgent messages are placed at the queue head so they will be the
    **  next message fetched from the queue - ahead of any
    **  previously-queued messages.)
    */
    element = (char *)queue->queue_head;
    element -= queue->vmsg_len;
    queue->queue_head = (q_vmsg_t *)element;

    if ( queue->queue_head < queue->first_msg_in_queue )
    {
        /*
        **  New queue_head pointer underflowed beginning of the extent...
        **  Wrap the queue_head pointer to the last message address
        **  in the extent allocated for the queue.
        */
        queue->queue_head = queue->last_msg_in_queue;
    }

#ifdef DIAG_PRINTFS 
        printf( " new queue_head @ %p", queue->queue_head );
#endif

    if ( msg != (char *)NULL )
    {
        element = (char *)&((queue->queue_head)->msgbuf);
        for ( i = 0; i < msglen; i++ )
        {
            *(element + i) = *(msg + i);
        }
    }
    (queue->queue_head)->msglen = msglen;

#ifdef DIAG_PRINTFS 
        printf( "\r\nsent urgent msg %p len %lx to queue_head @ %p",
                msg, msglen, queue->queue_head );
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
    send_msg_to( p2pt_vqueue_t *queue, char *msg, ULONG msglen )
{
    ULONG i;
    char *element;

    /*
    **  It is assumed when we enter this function that the queue has space
    **  to accept the message about to be sent.  Start by sending the
    **  message.
    */
    if ( msg != (char *)NULL )
    {
        element = (char *)&((queue->queue_tail)->msgbuf);
        for ( i = 0; i < msglen; i++ )
        {
            *(element + i) = *(msg + i);
        }
    }
    (queue->queue_tail)->msglen = msglen;

#ifdef DIAG_PRINTFS 
    printf( "\r\nsent msg %p len %lx to queue_tail @ %p",
                msg, msglen, queue->queue_tail );
#endif

    /*
    **  Now increment the queue_tail (send) pointer, adjusting for
    **  possible wrap to the beginning of the queue.
    */
    element = (char *)queue->queue_tail;
    element += queue->vmsg_len;
    queue->queue_tail = (q_vmsg_t *)element;

    if ( queue->queue_tail > queue->last_msg_in_queue )
    {
        /*
        **  Wrap the queue_tail pointer to the first message address
        **  in the queue.
        */
        queue->queue_tail = queue->first_msg_in_queue;
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
    fetch_msg_from( p2pt_vqueue_t *queue, char *msg, ULONG *msglen )
{
    char *element;
    int i;

    /*
    **  It is assumed when we enter this function that the queue contains
    **  one or more messages to be fetched.
    **  Fetch the message from the queue_head message location.
    */
    if ( msg != (char *)NULL )
    {
        element = (char *)&((queue->queue_head)->msgbuf);
        for ( i = 0; i < (queue->queue_head)->msglen; i++ )
        {
            *(msg + i) = *(element + i);
        }
    }

    if ( msglen != (ULONG *)NULL )
        *msglen = (queue->queue_head)->msglen;

#ifdef DIAG_PRINTFS 
    printf( "\r\nfetched msg of len %lx from queue_head @ %p",
            (queue->queue_head)->msglen, queue->queue_head );
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
        element = (char *)&((queue->queue_head)->msgbuf);
        *element = (char)NULL;
        (queue->queue_head)->msglen = 0L;

        /*
        **  Now increment the queue_head (send) pointer, adjusting for
        **  possible wrap to the beginning of the queue.
        */
        element = (char *)queue->queue_head;
        element += queue->vmsg_len;
        queue->queue_head = (q_vmsg_t *)element;

        if ( queue->queue_head > queue->last_msg_in_queue )
        {
            /*
            **  New queue_head pointer overflowed end of queue...
            **  Wrap the queue_head pointer to the first message address
            **  in the queue.
            */
            queue->queue_head = queue->first_msg_in_queue;
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
            pthread_cond_broadcast( &(queue->qbcst_cmplt) );

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
** data_extent_for - allocates space for queue data.  Data is allocated in
**                  a block large enough to hold (qsize + 1) messages.
*****************************************************************************/
static q_vmsg_t *
    data_extent_for( p2pt_vqueue_t *queue )
{
    char *new_extent;
    char *last_msg;
    size_t alloc_size;

    /*
    **  Calculate the number of bytes of memory needed for this extent.
    **  Start by calculating the size of each element of the extent array.
    **  Each (q_vmsg_t) element will contain a ULONG byte length followed
    **  by a character array of queue->msg_len bytes.  First get the size
    **  of the q_vmsg_t 'header' excluding the start of the data array.
    **  Then add the size of the maximum-length message data.
    */
    queue->vmsg_len = sizeof( q_vmsg_t ) - sizeof( char * );
    queue->vmsg_len += (sizeof( char ) * queue->msg_len);

    /*
    **  The size of each array element is now known...
    **  Multiply it by the number of elements to get allocation size.
    */
    alloc_size = queue->vmsg_len * (queue->msgs_per_queue + 1);

    /*
    **  Now allocate a block of memory to contain the extent.
    */
    if ( (new_extent = (char *)ts_malloc( alloc_size )) != (char *)NULL )
    {
        /*
        **  Clear the memory block.  Note that this creates a NULL pointer
        **  for the nxt_extent link as well as zeroing the message array.
        */
        bzero( (void *)new_extent, (int)alloc_size );

        /*
        **  Link new data extent into the queue control block
        */
        last_msg = new_extent + (queue->vmsg_len * queue->msgs_per_queue);
        queue->first_msg_in_queue = (q_vmsg_t *)new_extent;
        queue->last_msg_in_queue = (q_vmsg_t *)last_msg;
    }
#ifdef DIAG_PRINTFS 
    printf( "\r\nnew extent @ %p for queue @ %p vmsg_len %x", new_extent,
            queue, queue->vmsg_len );
#endif
    return( (q_vmsg_t *)new_extent );
}

/*****************************************************************************
** q_vcreate - creates a p2pthread message queue
*****************************************************************************/
ULONG
    q_vcreate( char name[4], ULONG opt, ULONG qsize, ULONG msglen, ULONG *qid )
{
    p2pt_vqueue_t *queue;
    ULONG error;
    int i;

    error = ERR_NO_ERROR;

    /*
    **  First allocate memory for the queue control block
    */
    queue = (p2pt_vqueue_t *)ts_malloc( sizeof( p2pt_vqueue_t ) );
    if ( queue != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  Ok... got a control block.
        */

        /*
        ** Total messages in memory allocation block (extent)
        ** (Extent has one extra for urgent message.)
        */
        queue->msgs_per_queue = qsize;

        /*
        ** Maximum size of messages sent to queue
        */
        queue->msg_len = msglen;

        /*
        ** Option Flags for queue
        */
        queue->flags = opt;

        /*
        **  Now allocate memory for the first queue data extent.
        */
        if ( data_extent_for( queue ) != (q_vmsg_t *)NULL )
        {
            /*
            **  Got both a control block and a data extent...
            **  Initialize the control block.
            */

            /*
            ** ID for queue
            */
            queue->qid = new_vqid();
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
            pthread_cond_init( &(queue->qbcst_cmplt),
                               (pthread_condattr_t *)NULL );

            /*
            **  Pointer to next message pointer to be fetched from queue
            */
            queue->queue_head = queue->first_msg_in_queue;

            /*
            **  Pointer to last message pointer sent to queue
            */
            queue->queue_tail = queue->first_msg_in_queue;

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
            ** Total number of messages currently sent to queue
            */
            queue->msg_count = 0;

            /*
            ** Task pend order (FIFO or Priority) for queue
            */
            if ( opt & Q_PRIOR )
                queue->order = 0;
            else
                queue->order = 1;

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
                ts_free( (void *)queue->first_msg_in_queue );
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
** q_vurgent - sends a message to the front of a p2pthread queue and awakens the
**           highest priority task pended on the queue.
*****************************************************************************/
ULONG
   q_vurgent( ULONG qid, void *msgbuf, ULONG msglen )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pt_vqueue_t *queue;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  Return with error if caller's message is larger than max message
        **  size specified for queue.
        */
        if ( msglen > queue->msg_len )
        {
           error = ERR_MSGSIZ;
           return( error );
        }

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
        if ( queue->msg_count > queue->msgs_per_queue )
        {
            /*
            **  Queue is already full... return QUEUE FULL error
            */
            error = ERR_QFULL;
        }
        else
        {
            /*
            **  Stuff the new message onto the front of the queue.
            */
            urgent_msg_to( queue, (char *)msgbuf, msglen );

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
** q_vsend - posts a message to the tail of a p2pthread queue and awakens the
**           highest priority task pended on the queue.
*****************************************************************************/
ULONG
   q_vsend( ULONG qid, void *msgbuf, ULONG msglen )
{
#ifdef DIAG_PRINTFS 
    p2pthread_cb_t *our_tcb;
#endif
    p2pt_vqueue_t *queue;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  Return with error if caller's message is larger than max message
        **  size specified for queue.
        */
        if ( msglen > queue->msg_len )
        {
           error = ERR_MSGSIZ;
           return( error );
        }

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
        ** Lock mutex for queue send
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(queue->queue_lock));
        pthread_mutex_lock( &(queue->queue_lock) );

        /*
        **  See how many messages are already sent into the queue
        */
        if ( queue->msg_count > queue->msgs_per_queue )
        {
            /*
            **  Queue is already full... return QUEUE FULL error
            */
            error = ERR_QFULL;
        }
        else
        {
            if ( queue->msg_count == queue->msgs_per_queue )
            {
                if ( (queue->msgs_per_queue == 0) &&
                     (queue->first_susp != (p2pthread_cb_t *)NULL) )
                {
                    /*
                    **  Special case... Send the new message.
                    */
                    send_msg_to( queue, (char *)msgbuf, msglen );
                }
                else
                {
                    /*
                    **  Queue is already full... return QUEUE FULL error
                    */
                    error = ERR_QFULL;
                }
            }
            else
            {
                /*
                **  Send the new message.
                */
                send_msg_to( queue, (char *)msgbuf, msglen );
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
** q_vbroadcast - sends the specified message to all tasks pending on the
**               specified p2pthread queue and awakens the tasks.
*****************************************************************************/
ULONG
   q_vbroadcast( ULONG qid, void *msgbuf, ULONG msglen, ULONG *tasks )
{
    p2pt_vqueue_t *queue;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_vqueue_t *)NULL )
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
            if ( queue->msg_count > queue->msgs_per_queue )
            {
                /*
                **  Queue is already full... return QUEUE FULL error
                */
                error = ERR_QFULL;
            }
            else
            {
                /*
                **  Stuff the new message onto the front of the queue.
                */
                urgent_msg_to( queue, (char *)msgbuf, msglen );

                /*
                **  Declare the send type
                */
                queue->send_type = BCAST;
                queue->bcst_tasks_awakened = 0;
            }
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
            {
                pthread_cond_wait( &(queue->qbcst_cmplt),
                                   &(queue->qbcst_lock) );
            }

            /*
            **  Unlock the queue broadcast completion mutex. 
            */
            pthread_mutex_unlock( &(queue->qbcst_lock) );
            pthread_cleanup_pop( 0 );

            if ( tasks != (ULONG *)NULL )
                *tasks = queue->bcst_tasks_awakened;
        }

        *tasks = queue->bcst_tasks_awakened;

        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** delete_vqueue - takes care of destroying the specified queue and freeing
**                any resources allocated for that queue
*****************************************************************************/
static void
   delete_vqueue( p2pt_vqueue_t *queue )
{
    /*
    **  First remove the queue from the queue list
    */
    unlink_qcb( queue->qid );

    /*
    **  Next delete extent allocated for queue data.
    */
    ts_free( (void *)queue->first_msg_in_queue );

    /*
    **  Finally delete the queue control block itself;
    */
    ts_free( (void *)queue );

}

/*****************************************************************************
** q_vdelete - removes the specified queue from the queue list and frees
**              the memory allocated for the queue control block and extents.
*****************************************************************************/
ULONG
   q_vdelete( ULONG qid )
{
    p2pt_vqueue_t *queue;
    ULONG error;
    static char deleted_msg[16] = "Queue Deleted!\n";

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_vqueue_t *)NULL )
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
            **  Overwrite any message previously there.
            */
            urgent_msg_to( queue, deleted_msg, 16 );

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
            {
                pthread_cond_wait( &(queue->qbcst_cmplt),
                                   &(queue->qbcst_lock) );
            }

            /*
            **  Unlock the queue broadcast completion mutex. 
            */
            pthread_mutex_unlock( &(queue->qbcst_lock) );
            pthread_cleanup_pop( 0 );
        }

        delete_vqueue( queue );
        sched_unlock();
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** waiting_on_vqueue - returns a nonzero result unless a qualifying event
**                    occurs on the specified queue which should cause the
**                    pended task to be awakened.  The qualifying events
**                    are:
**                        (1) a message is sent to the queue and the current
**                            task is selected to receive it
**                        (2) a broadcast message is sent to the queue
**                        (3) the queue is deleted
*****************************************************************************/
static int
    waiting_on_vqueue( p2pt_vqueue_t *queue, struct timespec *timeout,
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
** q_vreceive - blocks the calling task until a message is available in the
**             specified p2pthread queue.
*****************************************************************************/
ULONG
   q_vreceive( ULONG qid, ULONG opt, ULONG max_wait, void *msgbuf,
               ULONG buflen, ULONG *msglen )
{
    p2pthread_cb_t *our_tcb;
    struct timeval now;
    struct timespec timeout;
    int retcode;
    long sec, usec;
    p2pt_vqueue_t *queue;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (queue = qcb_for( qid )) != (p2pt_vqueue_t *)NULL )
    {
        /*
        **  Return with error if caller's buffer is smaller than max message
        **  size specified for queue.
        */
        if ( buflen < queue->msg_len )
        {
           error = ERR_BUFSIZ;
           return( error );
        }

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
            while ( (waiting_on_vqueue( queue, &timeout, &retcode )) &&
                    (retcode != ETIMEDOUT) )
            {
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
                while ( waiting_on_vqueue( queue, 0, &retcode ) )
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
                while ( (waiting_on_vqueue( queue, &timeout, &retcode )) &&
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
        **  See if we were awakened due to a q_vdelete on the queue.
        */
        if ( queue->send_type & KILLD )
        {
            fetch_msg_from( queue, (char *)msgbuf, msglen );
            error = ERR_QKILLD;
            *((char *)msgbuf) = (char)NULL;
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
                *((char *)msgbuf) = (char)NULL;
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
                fetch_msg_from( queue, (char *)msgbuf, msglen );
#ifdef DIAG_PRINTFS 
                printf( "...rcvd queue msg @ %p len %lx", msgbuf, *msglen );
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
        *((char *)msgbuf) = (char)NULL;
    }

    return( error );
}

/*****************************************************************************
** q_vident - identifies the specified p2pthread queue
*****************************************************************************/
ULONG
    q_vident( char name[4], ULONG node, ULONG *qid )
{
    p2pt_vqueue_t *current_qcb;
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
            for ( current_qcb = vqueue_list;
                  current_qcb != (p2pt_vqueue_t *)NULL;
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
            if ( current_qcb == (p2pt_vqueue_t *)NULL )
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
