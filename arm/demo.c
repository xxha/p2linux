/*****************************************************************************
 * demo.c - demonstrates the implementation of a Wind River pSOS+ (R) 
 *          application in a POSIX Threads environment.
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "not_quite_p_os.h"
#include "p2pthread.h"

#define EVENT1 1
#define EVENT2 2

extern p2pthread_cb_t *
   my_tcb( void );

/*****************************************************************************
**  demo program global data structures
*****************************************************************************/
char partition_task2_part[1024];
char partition_task3_part[2048];

ULONG part2_numblks;
ULONG part3_numblks;

ULONG task1_id;
ULONG task2_id;
ULONG task3_id;

ULONG queue2_id;
ULONG queue3_id;

ULONG part2_id;
ULONG part3_id;

ULONG sema41_id;

/*****************************************************************************
**  display_tcb
*****************************************************************************/
void display_tcb( void )
{
    p2pthread_cb_t *cur_tcb;
    int policy;

    cur_tcb = my_tcb();

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

/*
 * This example illustrates a producer/consumer problem where there is one
 * producer and two consumers. The producer waits till one or both consumers
 * are ready to receive a message (indicated by event flags) and then posts a
 * message on the mailbox of that consumer. Messages consist of memory blocks
 * allocated from two partitions (one per consumer): producer allocates a
 * block, writes data into it and sends it to consumers, that read the data
 * and release the block they received.
 */

/*-----------------------------------------------------------------------*/
/*
 *  task1  - This is the producer task. It waits till one of the consumers is
 *           ready to receive (by pending on 2 events; note that option of
 *           ev_receive is EV_ANY) and then posts a message on the consumer
 *           task's queue.
 */

void task1( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
ULONG err, event, i;
char *buffer;

tm_wkafter( 50 );

for (;;) {
    printf( "\r\ntask1 waiting on event mask %lx",
            (unsigned long)(EVENT1 | EVENT2) );
    err = ev_receive( EVENT1 | EVENT2, EV_ALL, 0, &event );

    if (event & EVENT1) {
      err = pt_getbuf( part2_id, (void **)&buffer );
      if ( err != ERR_NO_ERROR )
          printf( "\r\npt_getbuf on %ld returned error %lx", part2_id, err );
      else
      {
          for (i = 0; i < 10; i++)
              buffer[i] = 'A' + i;
          buffer[i] = 0;

          printf("\r\ntask1's message for task2: %s\n", buffer );

          err = q_vsend( queue2_id, buffer, 11 );
          err = pt_retbuf( part2_id, buffer );
      }
    }
    if (event & EVENT2) {
      err = pt_getbuf( part3_id, (void **)&buffer );
      if ( err != ERR_NO_ERROR )
          printf( "\r\npt_getbuf on %ld returned error %lx", part3_id, err );
      else
      {
          for (i = 0; i < 10; i++)
              buffer[i] = 'Z' - i;
          buffer[i] = 0;

          printf("\r\ntask1's message for task3: %s\n", buffer );
          err = q_send( queue3_id, buffer );
          err = pt_retbuf( part3_id, buffer );
      }
    }
  }
}

/*-----------------------------------------------------------------------*/
/*
 *  task2  - First consumer task. It tells the producer that it is ready to
 *           receive a message by posting an event on which producer is pending.
 *           Then it pends on the queue where producer posts the message.
 */

void task2( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
ULONG err, i;
char foo;
char msg[128];
ULONG msglen;

  for (;;) {
    err = ev_send( task1_id, EVENT1 );

    printf( "\r\ntask2 waiting on vqueue %ld", queue2_id );
    err = q_vreceive( queue2_id, Q_WAIT, 0L, msg, 128, &msglen );
    if ( err != ERR_NO_ERROR )
        printf( "\r\nq_vreceive returned error %lx", err );
    else
    {
        printf("\r\ntask2 received message from task1: %s\n", msg);

        for (i = 0; msg[i]; i++)
            foo ^= msg[i];
    }
  }
}

/*-----------------------------------------------------------------------*/
/*
 *  task3  - Second consumer task. Same as consumer1. The 2 consumers use
 *           different events and queues and they alternate because of the
 *           time slicing.
 */

void task3( ULONG dummy0, ULONG dummy1, ULONG dummy2, ULONG dummy3 )
{
    ULONG err;
    int i;
    char foo, *msg;

  for (;;) {
    err = ev_send( task1_id, EVENT2 );

    printf( "\r\ntask3 waiting on queue %ld", queue3_id );
    err = q_receive( queue3_id, Q_WAIT, 0L, msg );
    if ( err != ERR_NO_ERROR )
        printf( "\r\nq_receive returned error %lx", err );
    else
    {
        printf("\r\ntask3 received message from task1: %s\n", msg);

        for (i = 0; msg[i]; i++)
            foo += msg[i];
    }
  }
}

/*-----------------------------------------------------------------------*/

/*****************************************************************************
**  user system initialization, shutdown, and resource cleanup
*****************************************************************************/
void
    user_sysroot( void )
{
    ULONG err;

    printf( "\r\nCreating Queue 2" );
    err = q_vcreate( "QUE2", Q_PRIOR, 1, 128, &queue2_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );
 
    printf( "\r\nCreating Queue 3" );
    err = q_create( "QUE3", 3, Q_FIFO, &queue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );
  
    printf( "\r\nCreating task 2 partition" );
    err = pt_create( "PRT2", partition_task2_part, partition_task2_part, 1024,
                     128, PT_DEL, &part2_id, &part2_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );
  
    printf( "\r\nCreating task 3 partition" );
    err = pt_create( "PRT3", partition_task3_part, partition_task3_part, 2048,
                     256, PT_NODEL, &part3_id, &part3_numblks );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\nCreating Semaphore 1" );
    err = sm_create( "SM41", 3, SM_FIFO, &sema41_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );
  
    printf( "\r\nCreating Task 1" );
    err = t_create( "TSK1", 10, 0, 0, T_LOCAL, &task1_id );
    err = t_start( task1_id, T_TSLICE, task1, (ULONG *)NULL );

    printf( "\r\nCreating Task 2" );
    err = t_create( "TSK2", 10, 0, 0, T_LOCAL, &task2_id );
    err = t_start( task2_id, T_TSLICE, task2, (ULONG *)NULL );

    printf( "\r\nCreating Task 3" );
    err = t_create( "TSK3", 10, 0, 0, T_LOCAL, &task3_id );
    err = t_start( task3_id, T_TSLICE, task3, (ULONG *)NULL );

    while ( getchar() != (int)'q' )
        sleep( 1 );

    printf( "\r\nDeleting Task 1" );
    err = t_delete( task1_id );

    printf( "\r\nDeleting Task 2" );
    err = t_delete( task2_id );

    printf( "\r\nDeleting Task 3" );
    err = t_delete( task3_id );

    printf( "\r\nDeleting Semaphore 1" );
    err = sm_delete( sema41_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\nDeleting task 3 partition" );
    err = pt_delete( part3_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\nDeleting task 2 partition" );
    err = pt_delete( part2_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\nDeleting Queue 3" );
    err = q_delete( queue3_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\nDeleting Queue 2" );
    err = q_vdelete( queue2_id );
    if ( err != ERR_NO_ERROR )
        printf( "... returned error %lx", err );

    printf( "\r\n" );
}

