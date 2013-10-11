#ifndef _P2LINUX_H
#define _P2LINUX_H

#define UCHAR           unsigned char
#define USHORT          unsigned short
#define UINT            unsigned int
#define ULONG           unsigned long

#define EV_ALL          ((ULONG)0)
#define EV_ANY          ((ULONG)2)
#define EV_NOWAIT       ((ULONG)1)
#define EV_WAIT         ((ULONG)0)

#define PT_LOCAL        ((ULONG)0)
#define PT_DEL          ((ULONG)4)
#define PT_NODEL        ((ULONG)0)

#define Q_FIFO          ((ULONG)0)
#define Q_LIMIT         ((ULONG)4)
#define Q_NOLIMIT       ((ULONG)0)
#define Q_NOWAIT        ((ULONG)1)
#define Q_PRIOR         ((ULONG)2)
#define Q_WAIT          ((ULONG)0)

#define SM_FIFO         ((ULONG)0)
#define SM_PRIOR        ((ULONG)2)
#define SM_NOWAIT       ((ULONG)1)
#define SM_WAIT         ((ULONG)0)

#define T_LOCAL         ((ULONG)0)
#define T_NOPREEMPT     ((ULONG)1)
#define T_PREEMPT       ((ULONG)0)
#define T_NOTSLICE      ((ULONG)0)
#define T_TSLICE        ((ULONG)2)


/*
**  Some useful functions other than pSOS+ APIs.
*/

/* thread-safe malloc. */
void *ts_malloc( size_t blksize );  
/* thread-safe free. */
void ts_free( void *blkaddr );
/* 'locks the scheduler' to prevent preemption of the 
   current task by other task-level code. You can use
   it in some urgent functions that needed to be 
   implemented as quickly as possible, for ex. i use 
   it in q_send(), q_broadcast(). */
void sched_lock( void );    
/* 'unlocks the scheduler' to allow preemption of the 
   current task by other task-level code. */
void sched_unlock( void );			

/*
**  pSOS+ task related functions.
*/

/* enables a task to delete itself or another task. */
ULONG t_delete( ULONG tid ); 
/* creates a pthread to contain the specified p2pthread task and 
   initializes the requisite data structures to support p2pthread 
   task behavior not directly supported by Posix threads. */
ULONG t_create( char name[4], ULONG pri, ULONG sstack, ULONG ustack, ULONG mode,
                ULONG *tid );
/* creates a pthread to contain the specified p2pthread task and 
   initializes the requisite data structures to support p2pthread 
   task behavior not directly supported by Posix threads. 
   Note: only the mode T_TSLICE,T_NOTSLICE is supported, and the 
   flags T_PREEMPT and T_NOPREEMPT is useless here since the priority 
   of the task has been set by the arg 'pri' of t_create() function. */
ULONG t_start( ULONG tid, ULONG mode, void (*task)( ULONG, ULONG, ULONG, ULONG ),
               ULONG parms[4] );
/* suspends the specified p2pthread task. */
ULONG t_suspend( ULONG tid );
/* resume the specified p2pthread task. */
ULONG t_resume( ULONG tid );
/* retrieves the contents of the specified task notepad register. */
ULONG t_getreg( ULONG tid, ULONG regnum, ULONG *reg_value );
/* overwrites the contents of the specified task notepad register
   with the specified reg_value. */
ULONG t_setreg( ULONG tid, ULONG regnum, ULONG reg_value );
/* sets a new priority for the specified task. */
ULONG t_setpri( ULONG tid, ULONG pri, ULONG *oldpri );
/* sets the value of the calling task's mode flags. 
   Note: only the mode T_TSLICE | T_NOTSLICE and 
   T_PREEMPT | T_NOPREEMPT has been supported. */
ULONG t_mode( ULONG mask, ULONG new_flags, ULONG *old_flags );
/* identifies the specified p2pthread task. */
ULONG t_ident( char name[4], ULONG node, ULONG *tid );

/*
**  pSOS+ event related functions.
*/

/* sets the specified flag bits in a p2pthread task event group. */
ULONG ev_send( ULONG taskid, ULONG new_events );
/* blocks the calling task until a matching combination of events
   occurs in the specified p2pthread event flag group. */
ULONG ev_receive( ULONG mask, ULONG opt, ULONG max_wait, ULONG *captured );

/*
**  pSOS+ memory related functions.
*/

/* creates a new memory management area from which fixed-size
   data blocks may be allocated for applications use.
   Note:  the arg 'laddr' is not used here, and it depends on 
   youself to decide the 'paddr' value. */
ULONG pt_create( char name[4], void *paddr, void *laddr, ULONG length,
                 ULONG bsize, ULONG flags, ULONG *ptid, ULONG *nbuf );
/* removes the specified partition from the memory. */
ULONG pt_delete( ULONG ptid );
/* obtains a free data buffer from the specified memory partition. */
ULONG pt_getbuf( ULONG ptid, void **bufaddr );
/* releases a data buffer back to the specified memory partition. */
ULONG pt_retbuf( ULONG ptid, void *bufaddr );
/* identifies the named p2pthread partition. */
ULONG pt_ident( char name[4], ULONG node, ULONG *ptid );

/*
**  pSOS+ queue related functions.
*/

/* sends the specified message to all tasks pending on the
   specified p2pthread queue and awakens the tasks. */
ULONG q_broadcast( ULONG qid, ULONG msg[4], ULONG *count );
/* creates a p2pthread message queue. */
ULONG q_create( char name[4], ULONG qsize, ULONG opt, ULONG *qid );
/* delete a p2pthread message queue. */
ULONG q_delete( ULONG qid );
/* identifies the specified p2pthread queue. */
ULONG q_ident( char name[4], ULONG node, ULONG *qid );
/* blocks the calling task until a message is available in the
   specified p2pthread queue. */
ULONG q_receive( ULONG qid, ULONG opt, ULONG max_wait, ULONG msg[4] );
/* posts a message to the tail of a p2pthread queue and awakens the
   first selected task waiting on the queue. */
ULONG q_send( ULONG qid, ULONG msg[4] );
/* sends a message to the front of a p2pthread queue and awakens the
   first selected task waiting on the queue. */
ULONG q_urgent( ULONG qid, ULONG msg[4] );

/*
**  pSOS+ sema4 related functions.
*/

/* creates a p2pthread message semaphore. */
ULONG sm_create( char name[4], ULONG count, ULONG opt, ULONG *smid );
/* removes the specified semaphore from the semaphore list and frees
   the memory allocated for the semaphore control block and extents. */
ULONG sm_delete( ULONG smid );
/* identifies the specified p2pthread semaphore. */
ULONG sm_ident( char name[4], ULONG node, ULONG *smid );
/* blocks the calling task until a token is available on the
   specified p2pthread semaphore. */
ULONG sm_p( ULONG smid, ULONG opt, ULONG max_wait );
/* releases a p2pthread semaphore token and awakens the first selected
   task waiting on the semaphore. */
ULONG sm_v( ULONG smid );















#endif



