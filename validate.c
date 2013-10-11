/*****************************************************************************
 * validate.c -  validation suite for testing the implementation of a Wind
 *               River pSOS+ (R) kernel API in a POSIX Threads environment.
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "not_quite_p_os.h"
#include "p2pthread.h"

typedef union
{
    ULONG blk;
    char  name[4];
} objname_t;

typedef struct qmessage
{
    objname_t qname;
    ULONG     nullterm; 
    ULONG     t_cycle;
    ULONG     msg_no; 
} my_qmsg_t;

typedef union
{
    ULONG     blk[4];
    my_qmsg_t msg;
} msgblk_t;

/*
**  One event bit per consumer task
*/
#define EVENT2  0x001
#define EVENT3  0x002
#define EVENT4  0x004
#define EVENT5  0x008
#define EVENT6  0x010
#define EVENT7  0x020
#define EVENT8  0x040
#define EVENT9  0x080
#define EVENT10 0x100

extern p2pthread_cb_t *
   my_tcb( void );
extern p2pthread_cb_t *
   tcb_for( ULONG taskid );

#undef errno
extern int errno;

/*****************************************************************************
**  demo program global data structures
*****************************************************************************/
static char partition_1[512];
static ULONG part1_numblks;

static char partition_2[1024];
static ULONG part2_numblks;

static char partition_3[2048];
static ULONG part3_numblks;

static ULONG task1_id;
static ULONG task2_id;
static ULONG task3_id;
static ULONG task4_id;
static ULONG task5_id;
static ULONG task6_id;
static ULONG task7_id;
static ULONG task8_id;
static ULONG task9_id;
static ULONG task10_id;

static ULONG queue1_id;
static ULONG queue2_id;
static ULONG queue3_id;

static ULONG vqueue1_id;
static ULONG vqueue2_id;
static ULONG vqueue3_id;

static ULONG partn1_id;
static ULONG partn2_id;
static ULONG partn3_id;

static ULONG sema41_id;
static ULONG sema42_id;
static ULONG sema43_id;

static ULONG test_cycle;

/*****************************************************************************
**  display_tcb
*****************************************************************************/
void display_tcb( ULONG tid )
{
    
    int policy;
    p2pthread_cb_t *cur_tcb;

    cur_tcb = tcb_for( tid );

    if ( cur_tcb == (p2pthread_cb_t *)NULL )
        return;

    printf( "\r\nTask ID: %ld  Thread ID: %ld", cur_tcb->taskid,
            cur_tcb->pthrid  );

    policy = (cur_tcb->attr).__schedpolicy;
    switch (policy )
    {
        case SCHED_FIFO:
            printf( "\r\n    schedpolicy: SCHED_FIFO " );
            break;
        case SCHED_RR:
            printf( "\r\n    schedpolicy: SCHED_RR " );
            break;
        case SCHED_OTHER:
            printf( "\r\n    schedpolicy: SCHED_OTHER " );
            break;
        default :
            printf( "\r\n    schedpolicy: %d ", policy );
    }
    printf( " priority %d ", ((cur_tcb->attr).__schedparam).sched_priority );
    printf( " prv_priority %d ", (cur_tcb->prv_priority).sched_priority );
    printf( " detachstate %d ", (cur_tcb->attr).__detachstate );
}

/*****************************************************************************
**  validate_events
**         This function sequences through a series of actions to exercise
**         the various features and characteristics of p2pthread events
**
*****************************************************************************/
void validate_events( void )
{
    ULONG err;

    puts( "\r\n********** Event validation:" );

    puts( "\r\n.......... Now we send a sequence of EVENTS to consumer tasks" );
    puts( "           which will begin consuming queue messages." );
    puts( "           The consumer tasks are each waiting on a single EVENT," );
    puts( "           while Task 1 waits on any EVENT from a consumer task." );
    puts( "           This tests most of the event flag logic." );
    puts( "           Since Task 1 is at the highest priority level, the" );
    puts( "           other tasks will not execute until Task 1 blocks.\r\n" );

    puts( "Task 1 enabling Task 2 (priority 10) to consume QUE1 messages.");
    err = ev_send( task2_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 2..." );
    puts( "Task 1 waiting to receive ANY of EVENT2 | EVENT5 | EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    puts( "Task 1 enabling Task 5 (priority 15) to consume QUE1 messages.");
    err = ev_send( task5_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 5..." );
    puts( "Task 1 waiting to receive ANY of EVENT2 | EVENT5 | EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    puts( "Task 1 enabling Task 8 (priority 20) to consume QUE1 messages.");
    err = ev_send( task8_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 8..." );
    puts( "Task 1 waiting to receive ANY of EVENT2 | EVENT5 | EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    puts( "Task 1 enabling Task 3 (priority 10) to consume VLQ1 messages.");
    err = ev_send( task3_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 3..." );
    puts( "Task 1 waiting to receive ANY of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    puts( "Task 1 enabling Task 6 (priority 15) to consume VLQ1 messages.");
    err = ev_send( task6_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 6..." );
    puts( "Task 1 waiting to receive ANY of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    puts( "Task 1 enabling Task 9 (priority 20) to consume VLQ1 messages.");
    err = ev_send( task9_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 9..." );
    puts( "Task 1 waiting to receive ANY of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
}

/*****************************************************************************
**  validate_queues
**         This function sequences through a series of actions to exercise
**         the various features of p2pthread standard queues.
**
*****************************************************************************/
void validate_queues( void )
{
    ULONG err;
    ULONG message_num;
    ULONG task_count;
    ULONG my_queue_id;
    my_qmsg_t msg;
    msgblk_t rcvd_msg;

    puts( "\r\n********** Queue validation:" );
    /************************************************************************
    **  Queue-full / Queue Extensibility Test
    ************************************************************************/
    puts( "\n.......... First we created three standard queues" );
    puts( "\n.......... Next we attempt to send nine messages to each queue" );
    puts( "           This tests queue full / queue extensibility logic." );
    puts( "           The extensible standard QUE1 should return no errors" );
    puts( "           but QUE2 should return five 0x35 errs" );
    puts( "           and QUE3 should return nine 0x35 errs" );

    /*
    **  This is a 'sneaky trick' to null-terminate the object name string.
    */
    msg.nullterm = (ULONG)NULL; 

    /*
    **  
    */

    msg.qname.name[0] = 'Q';
    msg.qname.name[1] = 'U';
    msg.qname.name[2] = 'E';
    for ( message_num = 1; message_num < 10; message_num++ )
    { 
        /*
        */
        msg.t_cycle = test_cycle;
        msg.msg_no = message_num;
            
        msg.qname.name[3] = '1';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_send( queue1_id, (ULONG *)&msg );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
            
        msg.qname.name[3] = '2';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_send( queue2_id, (ULONG *)&msg );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
            
        msg.qname.name[3] = '3';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_send( queue3_id, (ULONG *)&msg );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
    }
    /************************************************************************
    **  Waiting Task 'Queuing Order' (FIFO vs. PRIORITY) Test
    ************************************************************************/
    puts( "\n.......... During the EVENT tests above, tasks 2, 5, and 8" );
    puts( "           were forced by EVENTs to wait on QUE1 in that order." );
    puts( "           The events were sent to lowest-priority tasks first." );
    puts( "           Since the queues awaken tasks in FIFO order, this" );
    puts( "           tests the task queueing order logic." );
    puts( "           Since Task 1 is at the highest priority level, the" );
    puts( "           other tasks will not execute until Task 1 blocks.\r\n" );
    puts( "           Tasks 2, 5, and 8 - in that order - should each" );
    puts( "           receive 3 messages from QUE1" );

    puts( "\r\nTask 1 blocking while messages are consumed..." );
    puts( "Task 1 waiting to receive ALL of EVENT2 | EVENT5 | EVENT8." );
    puts( "\n.......... Task1 should re-awaken only after ALL events received." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "\n.......... Next we send a message to zero-length QUE3 with" );
    puts( "           Task 8 waiting on QUE3... This should succeed." );
    puts( "           This tests the zero-length queue send logic." );

    puts( "Task 1 enabling Task 8 (priority 20) to consume QUE3 messages.");
    err = ev_send( task8_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Task 8..." );
    puts( "Task 1 waiting to receive ANY of EVENT8." );
    err = ev_receive( EVENT8, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    printf( "Task 1 Sending msg %ld to %s", message_num, msg.qname.name );
    msg.msg_no = message_num;
    err = q_send( queue3_id, (ULONG *)&msg );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "\r\nTask 1 blocking while message is consumed..." );
    puts( "Task 1 waiting to receive ANY of EVENT8." );
    err = ev_receive( EVENT8, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Queue-Broadcast Test
    ************************************************************************/
    puts( "\n.......... Next we enable Tasks 2, 5, and 8 to wait for" );
    puts( "           a message on QUE1.  Then we send a broadcast" );
    puts( "           message to QUE1.  This should wake each of Tasks 2," );
    puts( "           5, and 8.   This tests the queue broadcast logic." );

    puts( "Task 1 enabling Tasks 2, 5, and 8 to consume QUE1 messages.");
    err = ev_send( task2_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task5_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task8_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Tasks 2, 5, and 8..." );
    puts( "Task 1 waiting to receive ALL of EVENT2, EVENT5 and EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    msg.msg_no = ++message_num;
    msg.qname.name[3] = '1';
    printf( "Task 1 broadcasting msg %ld to %s", message_num, msg.qname.name );
    err = q_broadcast( queue1_id, (ULONG *)&msg, &task_count );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "Task 1 queue broadcast awakened %ld tasks\r\n", task_count );

    puts( "\r\nTask 1 blocking while message is consumed..." );
    puts( "Task 1 waiting to receive ALL of EVENT2, EVENT5, and EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Queue-Delete Test
    ************************************************************************/
    puts( "\n.......... Next we enable Tasks 2, 5, and 8 to wait for" );
    puts( "           a message on QUE1.  Then we delete QUE1." );
    puts( "           This should wake each of Tasks 2, 5, and 8," );
    puts( "           and they should each return an error 0x36." );
    puts( "           The q_delete should return an error 0x38." );
    puts( "           This tests the queue delete logic." );

    puts( "Task 1 enabling Tasks 2, 5, and 8 to consume QUE1 messages.");
    err = ev_send( task2_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task5_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task8_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Tasks 2, 5, and 8..." );
    puts( "Task 1 waiting to receive ALL of EVENT2, EVENT5 and EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    printf( "Task 1 deleting %s", msg.qname.name );
    err = q_delete( queue1_id );
    if ( err != ERR_NO_ERROR )
        printf( "Task 1 q_delete on QUE1 returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "\r\nTask 1 blocking until consumer tasks acknowledge deletion..." );
    puts( "Task 1 waiting to receive ALL of EVENT2, EVENT5, and EVENT8." );
    err = ev_receive( EVENT2 | EVENT5 | EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
            
    printf( "Task 1 deleting QUE3 with no tasks waiting" );
    err = q_delete( queue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "Task 1 q_delete on QUE3 returned error %lx\r\n", err );
    else
        printf( "\r\n" );


    /************************************************************************
    **  Queue Urgent Message Test
    ************************************************************************/
    puts( "\n.......... During the queue-full tests above, four messages" );
    puts( "           were sent, filling non-extensible queue QUE2." );
    puts( "           Now we will send an urgent message and then enable" );
    puts( "           a consumer task to receive all the messages in QUE2." );
    puts( "           The consumer task should receive five messages in all" );
    puts( "           from QUE2, starting with the urgent message." );
    puts( "           NOTE: This behavior is slightly more generous than" );
    puts( "           real pSOS+ (R) would be - it would return a QFULL error." );
    puts( "           However, this is a side effect of the 'extra' message" );
    puts( "           buffer added to support 'zero-length' behavior." );
    puts( "           (It also happens to model VRTXxx queue behavior.)" );
    puts( "           With the default (Q_NOLIMIT) queues it's a moot point." );

    msg.msg_no = ++message_num;
    msg.qname.name[3] = '2';
    printf( "Task 1 Sending urgent msg %ld to %s", message_num, msg.qname.name );
    err = q_urgent( queue2_id, (ULONG *)&msg );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "Task 1 enabling Task 5 to consume QUE2 messages.");
    err = ev_send( task5_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 5..." );
    puts( "Task 1 waiting to receive ANY of EVENT5." );
    err = ev_receive( EVENT5, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    puts( "\r\nTask 1 blocking while messages are consumed..." );
    puts( "Task 1 waiting to receive ANY of EVENT5." );
    err = ev_receive( EVENT5, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Queue-Ident and Queue-Not_Found Test
    ************************************************************************/
    puts( "\n.......... Finally, we test the q_ident logic..." );
    puts( "           Then we verify the error codes returned when" );
    puts( "           a non-existent queue is specified." );

    err = q_ident( "QUE2", 0, &my_queue_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nq_ident for QUE2 returned error %lx\r\n", err );
    else
        printf( "\nq_ident for QUE2 returned ID %lx... queue2_id == %lx\r\n",
                 my_queue_id, queue2_id );

    err = q_ident( "QUE1", 0, &my_queue_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nq_ident for QUE1 returned error %lx\r\n", err );
    else
        printf( "\nq_ident for QUE1 returned ID %lx queue2_id %lx\r\n",
                  my_queue_id, queue1_id );

    err = q_send( queue1_id, (ULONG *)&msg );
    printf( "\nq_send for QUE1 returned error %lx\r\n", err );

    err = q_receive( queue1_id, Q_NOWAIT, 0L, rcvd_msg.blk );
    printf( "\nq_receive for QUE1 (no waiting) returned error %lx\r\n", err );

    err = q_receive( queue1_id, Q_WAIT, 0L, rcvd_msg.blk );
    printf( "\nq_receive for QUE1 (wait forever) returned error %lx\r\n", err );

    err = q_delete( queue1_id );
    printf( "\nq_delete for QUE1 returned error %lx\r\n", err );
}

/*****************************************************************************
**  validate_vqueues
**         This function sequences through a series of actions to exercise
**         the various features of p2pthread variable-length queues.
**
*****************************************************************************/
void validate_vqueues( void )
{
    ULONG err;
    ULONG message_num;
    ULONG task_count;
    ULONG my_vqueue_id;
    ULONG my_msglen;
    my_qmsg_t msg;
    msgblk_t rcvd_msg;
    char msg_string[80];

    puts( "\r\n********** Variable-Length Queue validation:" );
    /************************************************************************
    **  Variable-Length Queue-full / Queue Extensibility Test
    ************************************************************************/
    puts( "\n.......... First we created three variable-length queues" );
    puts( "\n.......... Next we attempt to send nine messages to each queue" );
    puts( "           This tests variable-length queue full logic." );
    puts( "           The variable length VLQ1 should return no errors" );
    puts( "           but VLQ2 should return five 0x35 errs" );
    puts( "           and VLQ3 should return nine 0x35 errs" );

    /*
    **  This is a 'sneaky trick' to null-terminate the object name string.
    */
    msg.nullterm = (ULONG)NULL; 

    /*
    **  
    */

    msg.qname.name[0] = 'V';
    msg.qname.name[1] = 'L';
    msg.qname.name[2] = 'Q';
    for ( message_num = 1; message_num < 10; message_num++ )
    { 
        /*
        */
        msg.t_cycle = test_cycle;
        msg.msg_no = message_num;
            
        msg.qname.name[3] = '1';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_vsend( vqueue1_id, (ULONG *)&msg, 16 );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
            
        msg.qname.name[3] = '2';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_vsend( vqueue2_id, (ULONG *)&msg, 16 );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
            
        msg.qname.name[3] = '3';
        printf( "Task 1 sending msg %ld to %s", message_num, msg.qname.name );
        err = q_vsend( vqueue3_id, (ULONG *)&msg, 16 );
        if ( err != ERR_NO_ERROR )
            printf( " returned error %lx\r\n", err );
        else
            printf( "\r\n" );
    }

    puts( "\n.......... Sending a message to a variable-length queue which" );
    puts( "           is larger than the queue's maximum message size would" );
    puts( "           either have to truncate the message or cause buffer" );
    puts( "           overflow - neither of which is desirable.  For this" );
    puts( "           reason, an attempt to do this generates an error 0x31." );
    puts( "           This tests the overlength message detection logic." );
    err = q_vsend( vqueue1_id, (void *)msg_string, 80 );
    printf( "\nq_vsend 80-byte msg for 16-byte VLQ1 returned error %lx\r\n", err );

    puts( "\n.......... Receiving a message from a variable-length queue which" );
    puts( "           is larger than the caller's message buffer size would" );
    puts( "           either have to truncate the message or cause buffer" );
    puts( "           overflow - neither of which is desirable.  For this" );
    puts( "           reason, an attempt to do this generates an error 0x32." );
    puts( "           This tests the underlength buffer detection logic." );

    err = q_vreceive( vqueue2_id, Q_NOWAIT, 0L, rcvd_msg.blk, 16, &my_msglen );
    printf( "\n16-byte q_vreceive for 128-byte VLQ2 returned error %lx\r\n", err );
    /************************************************************************
    **  Waiting Task 'Queuing Order' (FIFO vs. PRIORITY) Test
    ************************************************************************/
    puts( "\n.......... During the EVENT tests above, tasks 3, 6, and 9" );
    puts( "           were forced by EVENTs to wait on VLQ1 in that order." );
    puts( "           The events were sent to lowest-priority tasks first." );
    puts( "           Since the queues awaken tasks in PRIORITY order, this" );
    puts( "           tests the task queueing order logic." );
    puts( "           Since Task 1 is at the highest priority level, the" );
    puts( "           other tasks will not execute until Task 1 blocks.\r\n" );
    puts( "           Tasks 9, 6, and 3 - in that order - should each" );
    puts( "           receive 3 messages from VLQ1" );

    puts( "\r\nTask 1 blocking while messages are consumed..." );
    puts( "Task 1 waiting to receive ALL of EVENT3 | EVENT6 | EVENT9." );
    puts( "\n.......... Task1 should re-awaken only after ALL events received." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "\n.......... Next we send a message to zero-length VLQ3 with" );
    puts( "           Task 9 waiting on VLQ3... This should succeed." );
    puts( "           This tests the zero-length queue send logic." );

    puts( "Task 1 enabling Task 9 (priority 20) to consume VLQ3 messages.");
    err = ev_send( task9_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Task 9..." );
    puts( "Task 1 waiting to receive ANY of EVENT9." );
    err = ev_receive( EVENT9, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    printf( "Task 1 Sending msg %ld to %s", message_num, msg.qname.name );
    msg.msg_no = message_num;
    err = q_vsend( vqueue3_id, (ULONG *)&msg, 16 );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "\r\nTask 1 blocking while message is consumed..." );
    puts( "Task 1 waiting to receive ANY of EVENT9." );
    err = ev_receive( EVENT9, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Variable-Length Queue-Broadcast Test
    ************************************************************************/
    puts( "\n.......... Next we enable Tasks 3, 6, and 9 to wait for" );
    puts( "           a message on VLQ1.  Then we send a broadcast" );
    puts( "           message to VLQ1.  This should wake each of Tasks 3," );
    puts( "           6, and 9.   This tests the queue broadcast logic." );

    puts( "Task 1 enabling Tasks 3, 6, and 9 to consume VLQ1 messages.");
    err = ev_send( task3_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task6_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task9_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Tasks 3, 6, and 9..." );
    puts( "Task 1 waiting to receive ALL of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    msg.msg_no = ++message_num;
    msg.qname.name[3] = '1';
    printf( "Task 1 broadcasting msg %ld to %s", message_num, msg.qname.name );
    err = q_vbroadcast( vqueue1_id, (ULONG *)&msg, 16, &task_count );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "Task 1 vqueue broadcast awakened %ld tasks\r\n", task_count );

    puts( "\r\nTask 1 blocking while message is consumed..." );
    puts( "Task 1 waiting to receive ALL of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Variable-Length Queue-Delete Test
    ************************************************************************/
    puts( "\n.......... Next we enable Tasks 3, 6, and 9 to wait for" );
    puts( "           a message on VLQ1.  Then we delete VLQ1." );
    puts( "           This should wake each of Tasks 3, 6, and 9," );
    puts( "           and they should each return an error 0x36." );
    puts( "           The q_vdelete should return an error 0x38." );
    puts( "           This tests the queue delete logic." );

    puts( "Task 1 enabling Tasks 3, 6, and 9 to consume VLQ1 messages.");
    err = ev_send( task3_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task6_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    err = ev_send( task9_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 1 blocking for handshake from Tasks 3, 6, and 9..." );
    puts( "Task 1 waiting to receive ALL of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    printf( "Task 1 deleting %s", msg.qname.name );
    err = q_vdelete( vqueue1_id );
    if ( err != ERR_NO_ERROR )
        printf( "Task 1 q_vdelete on VLQ1 returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "\r\nTask 1 blocking until consumer tasks acknowledge deletion..." );
    puts( "Task 1 waiting to receive ALL of EVENT3 | EVENT6 | EVENT9." );
    err = ev_receive( EVENT3 | EVENT6 | EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
            
    printf( "Task 1 deleting VLQ3 with no tasks waiting" );
    err = q_vdelete( vqueue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "Task 1 q_vdelete on VLQ3 returned error %lx\r\n", err );
    else
        printf( "\r\n" );


    /************************************************************************
    **  Variable-Length Queue Urgent Message Test
    ************************************************************************/
    puts( "\n.......... During the queue-full tests above, four messages" );
    puts( "           were sent, filling variable-length queue VLQ2." );
    puts( "           Now we will send an urgent message and then enable" );
    puts( "           a consumer task to receive all the messages in VLQ2." );
    puts( "           The consumer task should receive five messages in all" );
    puts( "           from VLQ2, starting with the urgent message." );
    puts( "           NOTE: This behavior is slightly more generous than" );
    puts( "           real pSOS+ (R) would be - it would return a QFULL error." );
    puts( "           However, this is a side effect of the 'extra' message" );
    puts( "           buffer added to support 'zero-length' behavior." );

    msg.msg_no = ++message_num;
    msg.qname.name[3] = '2';
    printf( "Task 1 Sending urgent msg %ld to %s", message_num, msg.qname.name );
    err = q_vurgent( vqueue2_id, (ULONG *)&msg, 16 );
    if ( err != ERR_NO_ERROR )
        printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "Task 1 enabling Task 6 to consume VLQ2 messages.");
    err = ev_send( task6_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    puts( "Task 1 blocking for handshake from Task 6..." );
    puts( "Task 1 waiting to receive ANY of EVENT6." );
    err = ev_receive( EVENT6, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    puts( "\r\nTask 1 blocking while messages are consumed..." );
    puts( "Task 1 waiting to receive ANY of EVENT6." );
    err = ev_receive( EVENT6, EV_ANY, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Variable-Length Queue-Ident and Queue-Not_Found Test
    ************************************************************************/
    puts( "\n.......... Finally, we test the q_vident logic..." );
    puts( "           Then we verify the error codes returned when" );
    puts( "           a non-existent queue is specified." );

    err = q_vident( "VLQ2", 0, &my_vqueue_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nq_vident for VLQ2 returned error %lx\r\n", err );
    else
        printf( "\nq_vident for VLQ2 returned ID %lx... vqueue2_id == %lx\r\n",
                 my_vqueue_id, vqueue2_id );

    err = q_vident( "VLQ1", 0, &my_vqueue_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nq_vident for VLQ1 returned error %lx\r\n", err );
    else
        printf( "\nq_vident for VLQ1 returned ID %lx vqueue2_id %lx\r\n",
                  my_vqueue_id, vqueue1_id );

    err = q_vsend( vqueue1_id, (void *)&msg, 16 );
    printf( "\nq_vsend for VLQ1 returned error %lx\r\n", err );

    err = q_vreceive( vqueue1_id, Q_NOWAIT, 0L, rcvd_msg.blk, 16, &my_msglen );
    printf( "\nq_vreceive for VLQ1 (no waiting) returned error %lx\r\n", err );

    err = q_vreceive( vqueue1_id, Q_WAIT, 0L, rcvd_msg.blk, 16, &my_msglen );
    printf( "\nq_vreceive for VLQ1 (wait forever) returned error %lx\r\n", err );

    err = q_vdelete( vqueue1_id );
    printf( "\nq_vdelete for VLQ1 returned error %lx\r\n", err );
}

/*****************************************************************************
**  validate_semaphores
**         This function sequences through a series of actions to exercise
**         the various features and characteristics of p2pthread semaphores
**
*****************************************************************************/
void validate_semaphores( void )
{
    ULONG err;
    ULONG my_sema4_id;
    int i;

    puts( "\r\n********** Semaphore validation:" );

    /************************************************************************
    **  Semaphore Creation Test
    ************************************************************************/
    puts( "\n.......... First we create three semaphores:" );
 
    puts( "\nCreating Semaphore 1, FIFO queuing and initially 'locked'" );
    err = sm_create( "SEM1", 0, SM_FIFO, &sema41_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating Semaphore 2, FIFO queuing with 2 tokens initially" );
    err = sm_create( "SEM2", 2, SM_FIFO, &sema42_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating Semaphore 3, PRIORITY queuing and initially 'locked'" );
    err = sm_create( "SEM3", 0, SM_PRIOR, &sema43_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );

    /************************************************************************
    **  Semaphore Waiting and Task Queueing Order Test
    ************************************************************************/
 
    puts( "\n.......... Next we enable Tasks 4, 7, and 10 to wait for" );
    puts( "           a token from SEM1 in reverse-priority order.  Then" );
    puts( "           Then we send three tokens to SEM1, waiting between" );
    puts( "           each token posting to see which task gets the token." );
    puts( "           This tests the semaphore post and queueing logic." );
    puts( "           The token should be acquired by Task 4, 7, and 10" );
    puts( "           in that order." );

    puts( "Task 1 enabling Tasks 4, 7, and 10 to consume SEM1 tokens.");
    err = ev_send( task4_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );
    err = ev_send( task7_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );
    err = ev_send( task10_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );

    puts( "Task 1 blocking for handshake from Tasks 4, 7, and 10..." );
    puts( "Task 1 waiting to receive ALL of EVENT4 | EVENT7 | EVENT10." );
    err = ev_receive( EVENT4 | EVENT7 | EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );

    for ( i = 0; i < 3; i++ )
    {
        puts( "Task 1 sending token to semaphore SEM1." );
        err = sm_v( sema41_id );
        if ( err != ERR_NO_ERROR )
            printf( "\nTask 1 send token to SEM1 returned error %lx\r\n", err );
    }

    puts( "Task 1 blocking for handshake from Tasks 4, 7, and 10..." );
    puts( "Task 1 waiting to receive ALL of EVENT4 | EVENT7 | EVENT10." );
 
    puts( "\n.......... Next Tasks 4, 7, and 10 look for tokens from SEM2" );
    puts( "           in reverse-priority order.  However, SEM2 has only two" );
    puts( "           tokens available, so one task will fail to acquire one.");
    puts( "           Since the tasks did not wait on the semaphore, the");
    puts( "           loser of the race will return an error 0x42");

    err = ev_receive( EVENT4 | EVENT7 | EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );
    tm_wkafter( 2 );
            
    puts( "\n.......... Next Tasks 4, 7, and 10 look for tokens from SEM3" );
    puts( "           in reverse-priority order.  However, SEM3 has only two" );
    puts( "           tokens available, so one task will fail to acquire one.");
    puts( "           Since the tasks do wait on the semaphore, the lowest");
    puts( "           priority task will return an error 0x01");

    puts( "Task 1 enabling Tasks 4, 7, and 10 to consume SEM3 tokens.");
    err = ev_send( task4_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );
    err = ev_send( task7_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );
    err = ev_send( task10_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    tm_wkafter( 2 );

    puts( "Task 1 blocking for handshake from Tasks 4, 7, and 10..." );
    puts( "Task 1 waiting to receive ALL of EVENT4 | EVENT7 | EVENT10." );
    err = ev_receive( EVENT4 | EVENT7 | EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    for ( i = 0; i < 2; i++ )
    {
        puts( "Task 1 sending token to semaphore SEM3." );
        err = sm_v( sema43_id );
        if ( err != ERR_NO_ERROR )
            printf( "\nTask 1 send token to SEM3 returned error %lx\r\n", err );
    }
    puts( "Task 1 blocking until Tasks 4, 7, and 10 consume SEM3 tokens." );
    puts( "Task 1 waiting to receive ALL of EVENT4 | EVENT7 | EVENT10." );
    err = ev_receive( EVENT4 | EVENT7 | EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Semaphore Deletion Test
    ************************************************************************/

    puts( "\n.......... Next Tasks 4, 7, and 10 look for tokens from SEM2" );
    puts( "           in priority order.  Task 1 will delete SEM1 before any" );
    puts( "           tokens become available.  Tasks 4, 7, and 10 should be" );
    puts( "           awakened and return error 0x43.  sm_delete of SEM1" );
    puts( "           should return error 0x44.  SEM2 will be deleted" );
    puts( "           with no tasks waiting, and should return no error." );
    puts( "           This tests the sm_delete logic." );
    tm_wkafter( 2 );
    puts( "Task 1 deleting semaphore SEM1." );
    err = sm_delete( sema41_id );
        if ( err != ERR_NO_ERROR )
            printf( "\nTask 1 delete of SEM1 returned error %lx\r\n", err );
        else
            printf( "\r\n" );
    puts( "Task 1 deleting semaphore SEM2." );
    err = sm_delete( sema42_id );
        if ( err != ERR_NO_ERROR )
            printf( "\nTask 1 delete of SEM2 returned error %lx\r\n", err );
        else
            printf( "\r\n" );

    puts( "Task 1 blocking until Tasks 4, 7, and 10 complete sm_delete test." );
    puts( "Task 1 waiting to receive ALL of EVENT4 | EVENT7 | EVENT10." );
    err = ev_receive( EVENT4 | EVENT7 | EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    /************************************************************************
    **  Semaphore Identification and Semaphore-Not-Found Test
    ************************************************************************/

    puts( "\n.......... Finally, we test the sm_ident logic..." );
    puts( "           Then we verify the error codes returned when" );
    puts( "           a non-existent semaphore is specified." );

    err = sm_ident( "SEM3", 0, &my_sema4_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nsm_ident for SEM3 returned error %lx\r\n", err );
    else
        printf( "\nsm_ident for SEM3 returned ID %lx... sema43_id == %lx\r\n",
                 my_sema4_id, sema43_id );

    err = sm_ident( "SEM1", 0, &my_sema4_id );
    if ( err != ERR_NO_ERROR )
        printf( "\nsm_ident for SEM1 returned error %lx\r\n", err );
    else
        printf( "\nsm_ident for SEM1 returned ID %lx sema41_id %lx\r\n",
                  my_sema4_id, sema41_id );

    err = sm_v( sema41_id );
    printf( "\nsm_v for SEM1 returned error %lx\r\n", err );

    err = sm_p( sema41_id, Q_NOWAIT, 0L );
    printf( "\nsm_p for SEM1 (no waiting) returned error %lx\r\n", err );

    err = sm_p( sema41_id, Q_WAIT, 0L );
    printf( "\nsm_p for SEM1 (wait forever) returned error %lx\r\n", err );

    err = sm_delete( sema41_id );
    printf( "\nsm_delete for SEM1 returned error %lx\r\n", err );
}

/*****************************************************************************
**  validate_partitions
**         This function sequences through a series of actions to exercise
**         the various features and characteristics of p2pthread partitions
**
*****************************************************************************/
void validate_partitions( void )
{
    ULONG err;
    ULONG my_partn_id;
    int i;

    void *buffer;
    char *buf1addr[32];
    char *buf2addr[32];
    char *buf3addr[32];

    puts( "\r\n********** Partition validation:" );

    /************************************************************************
    **  Partition Creation Test
    ************************************************************************/
    puts( "\n.......... First we create three partitions:" );
    puts( "           Test the block size restrictions first." );
    puts( "           Block size must be >= 4 bytes and a power of two." );
  
    printf( "\r\nCreating Partition 1 with block size not a power of two." );
    err = pt_create( "PRT1", partition_1, partition_1, 512,
                     15, PT_DEL, &partn1_id, &part1_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_create returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    printf( "\r\nCreating Partition 1 with block size < 4 bytes." );
    err = pt_create( "PRT1", partition_1, partition_1, 512,
                     2, PT_DEL, &partn1_id, &part1_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_create returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    printf( "\r\nCreating Partition 1 with 32 16-byte buffers" );
    err = pt_create( "PRT1", partition_1, partition_1, 512,
                     16, PT_DEL, &partn1_id, &part1_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_create returned error %lx\r\n", err );
    else
        printf( "\npt_create created %ld 16-byte buffers\r\n", part1_numblks );

    printf( "\r\nCreating Partition 2 with 32 32-byte buffers" );
    err = pt_create( "PRT2", partition_2, partition_2, 1024,
                     32, PT_NODEL, &partn2_id, &part2_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_create returned error %lx\r\n", err );
    else
        printf( "\npt_create created %ld 32-byte buffers\r\n", part2_numblks );

    printf( "\r\nCreating Partition 3 with 16 128-byte buffers" );
    err = pt_create( "PRT3", partition_3, partition_3, 2048,
                     128, PT_DEL, &partn3_id, &part3_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_create returned error %lx\r\n", err );
    else
        printf( "\npt_create created %ld 128-byte buffers\r\n", part3_numblks );

    /************************************************************************
    **  Partition Buffer Allocation, Use, and Release Test
    ************************************************************************/

    puts( "\n.......... Next we allocate 32 buffers from each partition:" );
    puts( "           PRT3 should return error 0x2c for the last 16 buffers." );
    puts( "           An ID string is written into each buffer obtained." );
    for ( i = 0; i < 32; i++ )
    {
        printf( "\r\nAllocating buffer %d from PRT1", i + 1 );
        err = pt_getbuf( partn1_id, (void **)&(buf1addr[i]) );
        if ( err != ERR_NO_ERROR )
            printf( "\npt_getbuf on PRT1 returned error %lx\r\n", err );
        else
            sprintf( buf1addr[i], "PRT1 buffer %d", i + 1 );

        printf( "\r\nAllocating buffer %d from PRT2", i + 1 );
        err = pt_getbuf( partn2_id, (void **)&(buf2addr[i]) );
        if ( err != ERR_NO_ERROR )
            printf( "\npt_getbuf on PRT2 returned error %lx\r\n", err );
        else
            sprintf( buf2addr[i], "PRT2 buffer %d", i + 1 );

        printf( "\r\nAllocating buffer %d from PRT3", i + 1 );
        err = pt_getbuf( partn3_id, (void **)&(buf3addr[i]) );
        if ( err != ERR_NO_ERROR )
            printf( "\npt_getbuf on PRT3 returned error %lx\r\n", err );
        else
            sprintf( buf3addr[i], "PRT3 buffer %d", i + 1 );
    }

    puts( "           Next print the ID strings from the first and last " );
    puts( "           buffers allocated from each partition.  This proves" );
    puts( "           that the buffers and partitions are unique." );
    puts( buf1addr[0] );
    puts( buf1addr[31] );
    puts( buf2addr[0] );
    puts( buf2addr[31] );
    puts( buf3addr[0] );
    puts( buf3addr[15] );

    puts( "           Now try to return a buffer from PRT2 to PRT1." );
    puts( "           This should return error 0x2d." );
    err = pt_retbuf( partn1_id, (void *)buf2addr[0] );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_retbuf on PRT1 returned error %lx\r\n", err );
    else
        printf( "\nReturned buffer %d  @ %p to PRT1\r\n", 1, buf2addr[0] );

    puts( "           Now try to return a buffer from PRT2 more than once." );
    puts( "           This should return error 0x2f on the second pt_retbuf." );
    err = pt_retbuf( partn2_id, (void *)buf2addr[0] );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_retbuf on PRT2 returned error %lx\r\n", err );
    else
        printf( "\nReturned buffer %d  @ %p to PRT2\r\n", 1, buf2addr[0] );

    err = pt_retbuf( partn2_id, (void *)buf2addr[0] );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_retbuf on PRT2 returned error %lx\r\n", err );
    else
        printf( "\nReturned buffer %d  @ %p to PRT2\r\n", 1, buf2addr[0] );

    /************************************************************************
    **  Partition Deletion Test
    ************************************************************************/
    puts( "\n.......... Now we delete partitions 1 and 2:" );
    puts( "           Partitions 1 and 3 were created with the PT_DEL option.");
    puts( "           They can be deleted even with buffers still allocated.");
    puts( "           Partition 2 was created with the PT_NODEL option.");
    puts( "           It cannot be deleted while buffers are still allocated.");

    printf( "\r\nDeleting Partition 1 with buffers allocated" );
    err = pt_delete( partn1_id );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_delete of PRT1 returned error %lx\r\n", err );
    else
        printf( "\r\n" );
  
    printf( "\r\nDeleting Partition 2 with buffers allocated" );
    err = pt_delete( partn2_id );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_delete of PRT2 returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    puts( "Returning all buffers to PRT2" );
    for ( i = 1; i < 32; i++ )
    {
        err = pt_retbuf( partn2_id, (void *)buf2addr[i] );
        if ( err != ERR_NO_ERROR )
            printf( "\npt_retbuf on PRT2 returned error %lx\r\n", err );
    }
  
    printf( "\r\nDeleting Partition 2 with no buffers allocated" );
    err = pt_delete( partn2_id );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_delete of PRT2 returned error %lx\r\n", err );
    else
        printf( "\r\n" );
  
    /************************************************************************
    **  Partition Identification and Partition-Not-Found Test
    ************************************************************************/

    puts( "\n.......... Finally, we test the pt_ident logic..." );
    puts( "           Then we verify the error codes returned when" );
    puts( "           a non-existent partition is specified." );

    err = pt_ident( "PRT3", 0, &my_partn_id );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_ident for PRT3 returned error %lx\r\n", err );
    else
        printf( "\npt_ident for PRT3 returned ID %lx... partn3_id == %lx\r\n",
                 my_partn_id, partn3_id );

    err = pt_ident( "PRT1", 0, &my_partn_id );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_ident for PRT1 returned error %lx\r\n", err );
    else
        printf( "\npt_ident for PRT1 returned ID %lx partn1_id %lx\r\n",
                  my_partn_id, partn1_id );

    err = pt_getbuf( partn2_id, (void **)&buffer );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_getbuf on PRT2 returned error %lx\r\n", err );
    else
        printf( "\npt_getbuf on PRT2 returned buffer @ %p\r\n", buffer );

    err = pt_retbuf( partn1_id, buffer );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_retbuf on PRT1 returned error %lx\r\n", err );
    else
        printf( "\r\n" );

    err = sm_delete( partn1_id );
    printf( "\nsm_delete for PRT1 returned error %lx\r\n", err );
}

/*****************************************************************************
**  task10
*****************************************************************************/
void task10( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    int i;

    for ( i = 0; i < 10; i++ )
    {
        tm_wkafter( 50 );
        puts( "\n Task 10 Not Suspended." );
    }

    /************************************************************************
    **  First wait on empty SEM1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 10 waiting on EVENT10 to begin acquiring token from SEM1" );
    err = ev_receive( EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 10 signalling EVENT10 to Task 1 to indicate Task 10 ready." );
    err = ev_send( task1_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 10 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 10 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 10 sm_p on SEM1 returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM2 to demonstrate sm_p without wait.
    ************************************************************************/
    /*
    **  Consume a token from SEM2 without waiting.
    */
    puts( "\nTask 10 attempting to acquire token from SEM2 without waiting." );
    if ( (err = sm_p( sema42_id, SM_NOWAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 10 acquired token from SEM2\r\n" );
    else
        printf( "\nTask 10 sm_p on SEM2 returned error %lx\r\n", err );
    puts( "Signalling EVENT10 to Task 1 - Task 10 ready to test SM_PRIOR." );
    err = ev_send( task1_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM3 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 10 waiting on EVENT10 to begin acquiring token from SEM3" );
    err = ev_receive( EVENT10, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 10 signalling EVENT10 to Task 1 to indicate Task 10 ready." );
    err = ev_send( task1_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM3.
    */
    puts( "\nTask 10 waiting up to 1 second to acquire token from SEM3" );
    if ( (err = sm_p( sema43_id, SM_WAIT, 100L )) == ERR_NO_ERROR )
        printf( "\r\nTask 10 acquired token from SEM3\r\n" );
    else
        printf( "\nTask 10 sm_p on SEM3 returned error %lx\r\n", err );

    puts( "Signalling EVENT10 to Task 1 - Task 10 finished SM_PRIOR test." );
    err = ev_send( task1_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 10 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 10 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 10 sm_p on SEM1 returned error %lx\r\n", err );

    puts( "Signalling EVENT10 to Task 1 - Task 10 finished sm_delete test." );
    err = ev_send( task1_id, EVENT10 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 10 deleting itself." );
    err = t_delete( 0 );
}


/*****************************************************************************
**  task9
*****************************************************************************/
void task9( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;
    int i;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty VLQ1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 9 waiting on EVENT9 to begin receive on VLQ1" );
    err = ev_receive( EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 9 signalling EVENT9 to Task 1 to indicate Task 9 ready." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume 3 messages from VLQ1.
    */
    i = 0;
    puts( "\nTask 9 waiting indefinitely to receive 3 msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 0L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 9 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
        if ( ++i == 3 )
            break;
    }
    printf( "\nTask 9 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT9 to Task 1 - Task 9 finished queuing order test." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on empty VLQ3 to demonstrate q_send to zero-length queue.
    ************************************************************************/
    puts( "\nTask 9 waiting on EVENT9 to begin receive on VLQ3" );
    err = ev_receive( EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 9 signalling EVENT9 to Task 1 to indicate Task 9 ready." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 9 waiting up to 1 sec to receive msgs on VLQ3" );
    while ( (err = q_vreceive( vqueue3_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 9 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 9 q_vreceive on VLQ3 returned error %lx\r\n", err );
    puts( "Signalling EVENT9 to Task 1 - Task 9 finished zero-length test." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 9 waiting on EVENT9 to begin receive on VLQ1" );
    err = ev_receive( EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 9 signalling EVENT9 to Task 1 to indicate Task 9 ready." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 9 waiting up to 1 sec to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 9 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 9 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT9 to Task 1 - Task 9 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 9 waiting on EVENT9 to begin receive on VLQ1" );
    err = ev_receive( EVENT9, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 9 signalling EVENT9 to Task 1 to indicate Task 9 ready." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages indefinitely.
    */
    puts( "\nTask 9 waiting indefinitely to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 0L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 9 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 9 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT9 to Task 1 - Task 9 finished q_delete test." );
    err = ev_send( task1_id, EVENT9 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 9 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  task8
*****************************************************************************/
void task8( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty QUE1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 8 waiting on EVENT8 to begin receive on QUE1" );
    err = ev_receive( EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 8 signalling EVENT8 to Task 1 to indicate Task 8 ready." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 8 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 8 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 8 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT8 to Task 1 - Task 8 finished queuing order test." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on empty QUE3 to demonstrate q_send to zero-length queue.
    ************************************************************************/
    puts( "\nTask 8 waiting on EVENT8 to begin receive on QUE3" );
    err = ev_receive( EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 8 signalling EVENT8 to Task 1 to indicate Task 8 ready." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 8 waiting up to 1 sec to receive msgs on QUE3" );
    while ( (err = q_receive( queue3_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 8 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 8 q_receive on QUE3 returned error %lx\r\n", err );
    puts( "Signalling EVENT8 to Task 1 - Task 8 finished zero-length test." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 8 waiting on EVENT8 to begin receive on QUE1" );
    err = ev_receive( EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 8 signalling EVENT8 to Task 1 to indicate Task 8 ready." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 8 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 8 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 8 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT8 to Task 1 - Task 8 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 8 waiting on EVENT8 to begin receive on QUE1" );
    err = ev_receive( EVENT8, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 8 signalling EVENT8 to Task 1 to indicate Task 8 ready." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 8 waiting indefinitely to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 0L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 8 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 8 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT8 to Task 1 - Task 8 finished q_delete test." );
    err = ev_send( task1_id, EVENT8 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 8 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  task7
*****************************************************************************/
void task7( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty SEM1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 7 waiting on EVENT7 to begin acquiring token from SEM1" );
    err = ev_receive( EVENT7, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 7 signalling EVENT7 to Task 1 to indicate Task 7 ready." );
    err = ev_send( task1_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 7 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 7 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 7 sm_p on SEM1 returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM2 to demonstrate sm_p without wait.
    ************************************************************************/
    /*
    **  Consume a token from SEM2 without waiting.
    */
    puts( "\nTask 7 attempting to acquire token from SEM2 without waiting." );
    if ( (err = sm_p( sema42_id, SM_NOWAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 7 acquired token from SEM2\r\n" );
    else
        printf( "\nTask 7 sm_p on SEM2 returned error %lx\r\n", err );
    puts( "Signalling EVENT7 to Task 1 - Task 7 ready to test SM_PRIOR." );
    err = ev_send( task1_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM3 in pre-determined task order to test priority-based
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 7 waiting on EVENT7 to begin acquiring token from SEM3" );
    err = ev_receive( EVENT7, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 7 signalling EVENT7 to Task 1 to indicate Task 7 ready." );
    err = ev_send( task1_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM3.
    */
    puts( "\nTask 7 waiting up to 1 second to acquire token from SEM3" );
    if ( (err = sm_p( sema43_id, SM_WAIT, 100L )) == ERR_NO_ERROR )
        printf( "\r\nTask 7 acquired token from SEM3\r\n" );
    else
        printf( "\nTask 7 sm_p on SEM3 returned error %lx\r\n", err );

    puts( "Signalling EVENT7 to Task 1 - Task 7 finished SM_PRIOR test." );
    err = ev_send( task1_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 7 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 7 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 7 sm_p on SEM1 returned error %lx\r\n", err );


    puts( "Signalling EVENT7 to Task 1 - Task 7 finished sm_delete test." );
    err = ev_send( task1_id, EVENT7 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 7 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  task6
*****************************************************************************/
void task6( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;
    union
    {
        char     blk[128];
        my_qmsg_t msg;
    } bigmsg;
    int i;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty VLQ1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 6 waiting on EVENT6 to begin receive on VLQ1" );
    err = ev_receive( EVENT6, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 6 signalling EVENT6 to Task 1 to indicate Task 6 ready." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume 3 messages from VLQ1.
    */
    i = 0;
    puts( "\nTask 6 waiting indefinitely to receive 3 msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 0L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 6 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
        if ( ++i == 3 )
            break;
    }
    printf( "\nTask 6 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT6 to Task 1 - Task 6 finished queuing order test." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 6 waiting on EVENT6 to begin receive on VLQ1" );
    err = ev_receive( EVENT6, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 6 signalling EVENT6 to Task 1 to indicate Task 6 ready." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 6 waiting up to 1 sec to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 6 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 6 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT6 to Task 1 - Task 6 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 6 waiting on EVENT6 to begin receive on VLQ1" );
    err = ev_receive( EVENT6, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 6 signalling EVENT6 to Task 1 to indicate Task 6 ready." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 6 waiting up to 1 sec to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 6 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 6 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT6 to Task 1 - Task 6 finished q_delete test." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait for messages on VLQ2 to demonstrate urgent message send
    **  behavior.
    ************************************************************************/
    puts( "\nTask 6 waiting on EVENT6 to begin receive on VLQ2" );
    err = ev_receive( EVENT6, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 6 signalling EVENT6 to Task 1 to indicate Task 6 ready." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until no available messages remain.
    */
    puts( "\nTask 6 receiving msgs without waiting on VLQ2" );
    while ( (err = q_vreceive( vqueue2_id, Q_NOWAIT, 0L, bigmsg.blk, 128, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 6 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                bigmsg.msg.t_cycle, bigmsg.msg.msg_no, bigmsg.msg.qname.name );
    }
    printf( "\nTask 6 q_vreceive on VLQ2 returned error %lx\r\n", err );
    puts( "Signalling EVENT6 to Task 1 - Task 6 finished q_urgent test." );
    err = ev_send( task1_id, EVENT6 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 6 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  task5
*****************************************************************************/
void task5( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty QUE1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 5 waiting on EVENT5 to begin receive on QUE1" );
    err = ev_receive( EVENT5, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 5 signalling EVENT5 to Task 1 to indicate Task 5 ready." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 5 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 5 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 5 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT5 to Task 1 - Task 5 finished queuing order test." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 5 waiting on EVENT5 to begin receive on QUE1" );
    err = ev_receive( EVENT5, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 5 signalling EVENT5 to Task 1 to indicate Task 5 ready." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 5 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 5 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 5 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT5 to Task 1 - Task 5 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 5 waiting on EVENT5 to begin receive on QUE1" );
    err = ev_receive( EVENT5, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 5 signalling EVENT5 to Task 1 to indicate Task 5 ready." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 5 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 5 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 5 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT5 to Task 1 - Task 5 finished q_delete test." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait for messages on QUE2 to demonstrate urgent message send
    **  behavior.
    ************************************************************************/
    puts( "\nTask 5 waiting on EVENT5 to begin receive on QUE2" );
    err = ev_receive( EVENT5, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 5 signalling EVENT5 to Task 1 to indicate Task 5 ready." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until no available messages remain.
    */
    puts( "\nTask 5 receiving msgs without waiting on QUE2" );
    while ( (err = q_receive( queue2_id, Q_NOWAIT, 0L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 5 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 5 q_receive on QUE2 returned error %lx\r\n", err );
    puts( "Signalling EVENT5 to Task 1 - Task 5 finished q_urgent test." );
    err = ev_send( task1_id, EVENT5 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 5 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  task4
*****************************************************************************/
void task4( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty SEM1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 4 waiting on EVENT4 to begin acquiring token from SEM1" );
    err = ev_receive( EVENT4, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 4 signalling EVENT4 to Task 1 to indicate Task 4 ready." );
    err = ev_send( task1_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 4 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 4 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 4 sm_p on SEM1 returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM2 to demonstrate sm_p without wait.
    ************************************************************************/
    /*
    **  Consume a token from SEM2 without waiting.
    */
    puts( "\nTask 4 attempting to acquire token from SEM2 without waiting." );
    if ( (err = sm_p( sema42_id, SM_NOWAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 4 acquired token from SEM2\r\n" );
    else
        printf( "\nTask 4 sm_p on SEM2 returned error %lx\r\n", err );
    puts( "Signalling EVENT4 to Task 1 - Task 4 ready to test SM_PRIOR." );
    err = ev_send( task1_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Next wait on SEM3 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 4 waiting on EVENT4 to begin acquiring token from SEM3" );
    err = ev_receive( EVENT4, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 4 signalling EVENT4 to Task 1 to indicate Task 4 ready." );
    err = ev_send( task1_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM3.
    */
    puts( "\nTask 4 waiting up to 1 second to acquire token from SEM3" );
    if ( (err = sm_p( sema43_id, SM_WAIT, 100L )) == ERR_NO_ERROR )
        printf( "\r\nTask 4 acquired token from SEM3\r\n" );
    else
        printf( "\nTask 4 sm_p on SEM3 returned error %lx\r\n", err );

    puts( "Signalling EVENT4 to Task 1 - Task 4 finished SM_PRIOR test." );
    err = ev_send( task1_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Consume one token from SEM1.
    */
    puts( "\nTask 4 waiting indefinitely to acquire token from SEM1" );
    if ( (err = sm_p( sema41_id, SM_WAIT, 0L )) == ERR_NO_ERROR )
        printf( "\r\nTask 4 acquired token from SEM1\r\n" );
    else
        printf( "\nTask 4 sm_p on SEM1 returned error %lx\r\n", err );

    puts( "Signalling EVENT4 to Task 1 - Task 4 finished sm_delete test." );
    err = ev_send( task1_id, EVENT4 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 4 deleting itself." );
    err = t_delete( 0 );
}


/*****************************************************************************
**  task3
*****************************************************************************/
void task3( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;
    int i;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty VLQ1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 3 waiting on EVENT3 to begin receive on VLQ1" );
    err = ev_receive( EVENT3, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 3 signalling EVENT3 to Task 1 to indicate Task 3 ready." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume 3 messages from VLQ1.
    */
    i = 0;
    puts( "\nTask 3 waiting indefinitely to receive 3 msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 0L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 3 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
        if ( ++i == 3 )
            break;
    }
    printf( "\nTask 3 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT3 to Task 1 - Task 3 finished queuing order test." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 3 waiting on EVENT3 to begin receive on VLQ1" );
    err = ev_receive( EVENT3, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 3 signalling EVENT3 to Task 1 to indicate Task 3 ready." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 3 waiting up to 1 sec to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 3 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 3 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT3 to Task 1 - Task 3 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty VLQ1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 3 waiting on EVENT3 to begin receive on VLQ1" );
    err = ev_receive( EVENT3, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 3 signalling EVENT3 to Task 1 to indicate Task 3 ready." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 3 waiting up to 1 sec to receive msgs on VLQ1" );
    while ( (err = q_vreceive( vqueue1_id, Q_WAIT, 100L, msg.blk, 16, 0 ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 3 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 3 q_vreceive on VLQ1 returned error %lx\r\n", err );
    puts( "Signalling EVENT3 to Task 1 - Task 3 finished q_delete test." );
    err = ev_send( task1_id, EVENT3 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 3 deleting itself." );
    err = t_delete( 0 );
}


/*****************************************************************************
**  task2
*****************************************************************************/
void task2( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    msgblk_t msg;

    sleep( 1 );
    /************************************************************************
    **  First wait on empty QUE1 in pre-determined task order to test
    **  task wait-queueing order ( FIFO vs. PRIORITY ).
    ************************************************************************/
    puts( "\nTask 2 waiting on EVENT2 to begin receive on QUE1" );
    err = ev_receive( EVENT2, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 2 signalling EVENT2 to Task 1 to indicate Task 2 ready." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 2 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 2 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 2 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT2 to Task 1 - Task 2 finished queuing order test." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue broadcast behavior.
    ************************************************************************/
    puts( "\nTask 2 waiting on EVENT2 to begin receive on QUE1" );
    err = ev_receive( EVENT2, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 2 signalling EVENT2 to Task 1 to indicate Task 2 ready." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 2 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 2 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 2 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT2 to Task 1 - Task 2 finished q_broadcast test." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /************************************************************************
    **  Now wait along with other tasks on empty QUE1 to demonstrate
    **  queue delete behavior.
    ************************************************************************/
    puts( "\nTask 2 waiting on EVENT2 to begin receive on QUE1" );
    err = ev_receive( EVENT2, EV_ALL, 0, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    puts( "Task 2 signalling EVENT2 to Task 1 to indicate Task 2 ready." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Consume messages until 1 second elapses without an available message.
    */
    puts( "\nTask 2 waiting up to 1 sec to receive msgs on QUE1" );
    while ( (err = q_receive( queue1_id, Q_WAIT, 100L, msg.blk ))
            == ERR_NO_ERROR )
    {
        printf( "\r\nTask 2 rcvd Test Cycle %ld Msg No. %ld from %s\r\n",
                msg.msg.t_cycle, msg.msg.msg_no, msg.msg.qname.name );
    }
    printf( "\nTask 2 q_receive on QUE1 returned error %lx\r\n", err );
    puts( "Signalling EVENT2 to Task 1 - Task 2 finished q_delete test." );
    err = ev_send( task1_id, EVENT2 );
    if ( err != ERR_NO_ERROR )
         printf( " returned error %lx\r\n", err );

    /*
    **  Tests all done... delete our own task.
    */
    puts( "\n.......... Task 2 deleting itself." );
    err = t_delete( 0 );
}

/*****************************************************************************
**  validate_tasks
**         This function sequences through a series of actions to exercise
**         the various features and characteristics of p2pthread tasks
**
*****************************************************************************/
void validate_tasks( void )
{
    ULONG err;
    ULONG oldpriority;
    ULONG original_value;
    ULONG my_taskid;
    ULONG i;

    puts( "\r\n********** Task validation:" );

    /************************************************************************
    **  Task Create Test
    ************************************************************************/
    puts( "\n.......... First we create the TCBs for the consumer tasks." );

    err = t_create( "TSK2", 10, 0, 0, T_LOCAL, &task2_id );
    err = t_create( "TSK3", 10, 0, 0, T_LOCAL, &task3_id );
    err = t_create( "TSK4", 10, 0, 0, T_LOCAL, &task4_id );
    err = t_create( "TSK5", 15, 0, 0, T_LOCAL, &task5_id );
    err = t_create( "TSK6", 15, 0, 0, T_LOCAL, &task6_id );
    err = t_create( "TSK7", 15, 0, 0, T_LOCAL, &task7_id );
    err = t_create( "TSK8", 20, 0, 0, T_LOCAL, &task8_id );
    err = t_create( "TSK9", 20, 0, 0, T_LOCAL, &task9_id );
    err = t_create( "TSKA", 20, 0, 0, T_LOCAL, &task10_id );

    /************************************************************************
    **  Task Start and Set Mode Preemptibility Test
    ************************************************************************/
    puts( "\n.......... Next call t_mode to make Task 1 non-preemptible." );
    puts( "\r\nTask 1 going non-preemptible (locking scheduler)." );
    err = t_mode( T_NOPREEMPT, T_NOPREEMPT, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_mode returned error %lx\r\n", err );
    display_tcb( task1_id );
    printf( "\r\n" );

    puts( "\n.......... Then start each of the consumer tasks." );

    puts( "Starting Task 2 with timeslicing at priority level 10" );
    err = t_start( task2_id, T_TSLICE, task2, (ULONG *)NULL );

    puts( "Starting Task 3 with timeslicing at priority level 10" );
    err = t_start( task3_id, T_TSLICE, task3, (ULONG *)NULL );

    puts( "Starting Task 4 with timeslicing at priority level 10" );
    err = t_start( task4_id, T_TSLICE, task4, (ULONG *)NULL );

    puts( "Starting Task 5 with timeslicing at priority level 15" );
    err = t_start( task5_id, T_TSLICE, task5, (ULONG *)NULL );

    puts( "Starting Task 6 with timeslicing at priority level 15" );
    err = t_start( task6_id, T_TSLICE, task6, (ULONG *)NULL );

    puts( "Starting Task 7 with timeslicing at priority level 15" );
    err = t_start( task7_id, T_TSLICE, task7, (ULONG *)NULL );

    puts( "Starting Task 8 with timeslicing at priority level 20" );
    err = t_start( task8_id, T_TSLICE, task8, (ULONG *)NULL );

    puts( "Starting Task 9 with timeslicing at priority level 20" );
    err = t_start( task9_id, T_TSLICE, task9, (ULONG *)NULL );

    puts( "Starting Task 10 with timeslicing at priority level 20" );
    err = t_start( task10_id, T_TSLICE, task10, (ULONG *)NULL );

    puts( "\n.......... Next call t_mode to make Task 1 preemptible again." );
    puts( "\r\nTask 1 going preemptible (unlocking scheduler)." );
    err = t_mode( T_NOPREEMPT, T_PREEMPT, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_mode returned error %lx\r\n", err );
    display_tcb( task1_id );
    printf( "\r\n" );
 
    /************************************************************************
    **  Task Suspend and Resume Test
    ************************************************************************/
    puts( "Task 1 sleeping for 2 seconds to allow task 10 to run.\r\n" );
    tm_wkafter( 200 );
    puts( "\n.......... Next call t_suspend to suspend Task 10." );
    puts( "Task 1 calling t_suspend for task 10.\r\n" );
    err = t_suspend( task10_id );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_suspend returned error %lx\r\n", err );
    puts( "Task 1 sleeping for 1.5 seconds to allow task 10 to run.\r\n" );
    puts( "           Since task 10 is printing a message every 1/2 second," );
    puts( "           this demonstrates that the suspension overrides" );
    puts( "           timeouts, etc." );
    tm_wkafter( 150 );
    puts( "\n.......... Next call t_suspend a second time to suspend Task 10." );
    puts( "           The second call should fail with an error 0x14." );
    puts( "Task 1 calling t_suspend for task 10.\r\n" );
    err = t_suspend( task10_id );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_suspend returned error %lx\r\n", err );

    puts( "\n.......... Next call t_resume to make Task 10 runnable again." );
    puts( "Task 1 calling t_resume for task 10.\r\n" );
    err = t_resume( task10_id );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_resume returned error %lx\r\n", err );
    puts( "Task 1 sleeping for 4 seconds to allow task 10 to run.\r\n" );
    tm_wkafter( 400 );

    /************************************************************************
    **  Task Set Mode Time-Slicing Test
    ************************************************************************/
    puts( "\n.......... Next call t_mode to disable time slicing on Task 1." );
    puts( "\r\nTask 1 disabling round robin time slicing for task." );
    err = t_mode( T_TSLICE, T_NOTSLICE, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_mode returned error %lx\r\n", err );
    display_tcb( task1_id );
    printf( "\r\n" );

    puts( "\n.......... Next call t_mode to restore time slicing on Task 1." );
    puts( "\r\nTask 1 disabling round robin time slicing for task." );
    err = t_mode( T_TSLICE, T_TSLICE, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_mode returned error %lx\r\n", err );
    display_tcb( task1_id );
    printf( "\r\n" );

    /************************************************************************
    **  Task Set Priority Test
    ************************************************************************/
    puts( "\n.......... Next call t_setprio to raise priority on Task 2." );
    puts( "\r\nTask 1 setting priority to 22 for Task 2." );
    err = t_setpri( task2_id, 22, &oldpriority );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_setpri returned error %lx\r\n", err );
    display_tcb( task2_id );
    printf( "\r\n" );

    puts( "\n.......... Next call t_setprio to restore Task 2's priority." );
    puts( "\r\nTask 1 restoring Task 2 to original priority setting." );
    err = t_setpri( task2_id, oldpriority, (ULONG *)NULL );
    if ( err != ERR_NO_ERROR )
         printf( "\nt_setpri returned error %lx\r\n", err );
    display_tcb( task2_id );
    printf( "\r\n" );

    /************************************************************************
    **  Task Get and Set Register Test
    ************************************************************************/
    puts( "\n.......... Next display the contents of Task 1's registers." );
    puts( "           Then set the contents of Task 2's registers." );
    puts( "           and then display their new contents." );
    for ( i = 0; i < NUM_TASK_REGS; i++ )
    {
        err = t_getreg( 0, i, &original_value );
        if ( err != ERR_NO_ERROR )
            printf( "\nt_getreg for Task 1 returned error %lx\r\n", err );
        else
            printf( "\nt_getreg for Task 1 register %ld = %lx\r\n", 
                    i, original_value );
    }

    for ( i = 0; i < NUM_TASK_REGS; i++ )
    {
        err = t_getreg( task2_id, i, &original_value );
        if ( err != ERR_NO_ERROR )
            printf( "\nt_getreg for Task 2 returned error %lx\r\n", err );
        else
            printf( "\nOriginal value for Task 2 register %ld = %lx\r\n", 
                    i, original_value );

        printf( "Setting Task 2 Register %ld to %lx\r\n", i, i + 1 );
        err = t_setreg( task2_id, i, i + 1 );
        if ( err != ERR_NO_ERROR )
            printf( "t_setreg for Task 2 returned error %lx\r\n", err );

        err = t_getreg( task2_id, i, &original_value );
        if ( err != ERR_NO_ERROR )
            printf( "t_getreg for Task 2 returned error %lx\r\n", err );
        else
            printf( "New value for Task 2 register %ld = %lx\r\n", 
                    i, original_value );
    }
  
    /************************************************************************
    **  Task Identification Test
    ************************************************************************/
    puts( "\n.......... Finally, we test the t_ident logic..." );

    err = t_ident( "TSK3", 0, &my_taskid );
    if ( err != ERR_NO_ERROR )
        printf( "\npt_ident for TSK3 returned error %lx\r\n", err );
    else
        printf( "\npt_ident for TSK3 returned ID %lx... task3_id == %lx\r\n",
                 my_taskid, task3_id );
}

/*****************************************************************************
**  task1
**         This is the 'sequencer' task.  It orchestrates the production of
**         messages, events, and semaphore tokens in such a way as to provide
**         a predictable sequence of events for the validation record.  All
**         other tasks 'handshake' with task1 to control the timing of their
**         activities.
**
*****************************************************************************/
void task1( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    /*
    **  Indicate messages originated with the first test cycle.
    */
    test_cycle = 1;
    validate_tasks();

    test_cycle++;
    validate_events();

    test_cycle++;
    validate_queues();

    test_cycle++;
    validate_vqueues();

    test_cycle++;
    validate_semaphores();

    test_cycle++;
    validate_partitions();

    perror("Validation tests completed - enter 'q' to quit... (ignore errno)");

    /*
    **  Tests all done... twiddle our thumbs until user quits program.
    */
    for( ;; )
    {
        tm_wkafter( 50 );
    }
}

/*****************************************************************************
**  user system initialization, shutdown, and resource cleanup
**  NOTE:  This function MUST NOT call any p2pthread calls which might BLOCK!
**         DOING SO WILL CAUSE THE p2pthread VIRTUAL MACHINE TO CRASH!
*****************************************************************************/
void
    user_sysroot( void )
{
    ULONG err;
 
    printf( "\r\n" );

    puts( "Creating Queue 1, extensible with 4 16-byte messages" );
    err = q_create( "QUE1", 4, Q_FIFO, &queue1_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating Queue 2, fixed-length with 4 16-byte messages" );
    err = q_create( "QUE2", 4, Q_FIFO | Q_LIMIT, &queue2_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating Queue 3, fixed-length with 0 16-byte messages" );
    err = q_create( "QUE3", 0, Q_FIFO | Q_LIMIT, &queue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating VL Queue 1, with 9 1-byte to 16-byte messages" );
    err = q_vcreate( "VLQ1", Q_PRIOR, 9, 16, &vqueue1_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating VL Queue 2, with 4 1-byte to 128-byte messages" );
    err = q_vcreate( "VLQ2", Q_PRIOR, 4, 128, &vqueue2_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );
 
    puts( "Creating VL Queue 3, with 0 1-byte to 16-byte messages" );
    err = q_vcreate( "VLQ3", Q_PRIOR, 0, 16, &vqueue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx\r\n", err );

    err = t_create( "TSK1", 25, 0, 0, T_LOCAL, &task1_id );
    puts( "Starting Task 1 with timeslicing at priority level 25" );
    err = t_start( task1_id, T_TSLICE, task1, (ULONG *)NULL );

    printf( "\r\n" );

    while ( getchar() != (int)'q' )
        sleep( 1 );

    puts( "Deleting Task 1" );
    err = t_delete( task1_id );

    printf( "\r\n" );
}

