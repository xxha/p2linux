/*****************************************************************************
 * memblk.c - defines the wrapper functions and data structures needed
 *            to implement a Wind River pSOS+ (R) partition API 
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

#define PT_DEL       0x04

#define ERR_TIMEOUT  0x01
#define ERR_NODENO   0x04
#define ERR_OBJDEL   0x05
#define ERR_OBJTFULL 0x08
#define ERR_OBJNF    0x09

#define ERR_BUFSIZE  0x29
#define ERR_BUFINUSE 0x2B
#define ERR_NOBUF    0x2C
#define ERR_BUFADDR  0x2D
#define ERR_BUFFREE  0x2F

/*****************************************************************************
**  p2pthread partition extent type - this is the header for a dynamically 
**                                    allocated array of contiguous data blocks
*****************************************************************************/
typedef struct prtn_extent
{
    char *
        baddr;           /* Pointer to data block for current extent */
   ULONG 
        bcount;
} prtn_extent_t;

/*****************************************************************************
**  Control block for p2pthread memory partition
**
*****************************************************************************/
typedef struct p2pt_partition
{
        /*
        ** ID for partition
        */
    ULONG
        prtn_id;

        /*
        ** Partition Name
        */
    char
        ptname[4];

        /*
        ** Option Flags for partition
        */
    ULONG
        flags;

        /*
        ** Mutex for partition get/release block
        */
    pthread_mutex_t
        prtn_lock;

        /*
        **  Pointer to first data block in free_list for partition
        */
    char **
        first_free;

        /*
        **  Pointer to last data block in free_list for partition
        */
    char **
        last_free;

        /*
        ** Total number of free data blocks in partition
        */
    ULONG 
        free_blk_count;

        /*
        ** Total number of allocated data blocks in partition
        */
    ULONG
        used_blk_count;

        /*
        ** Number of bytes per allocatable data block
        */
    ULONG 
        blk_size;

        /*
        **  Pointer to data extent allocated for partition
        */
    prtn_extent_t *
        data_extent;

        /*
        **  Pointer to next partition control block in partition list.
        */
    struct p2pt_partition *
        nxt_prtn;

} p2pt_prtn_t;

/*****************************************************************************
**  External function and data references
*****************************************************************************/
extern void *
    ts_malloc( size_t blksize );
extern void 
    ts_free( void *blkaddr );
extern void
   sched_lock( void );
extern void
   sched_unlock( void );
extern p2pthread_cb_t *
   my_tcb( void );

/*****************************************************************************
**  p2pthread Global Data Structures
*****************************************************************************/

/*
**  prtn_list is a linked list of partition control blocks.  It is used to
**            locate partitions by their ID numbers.
*/
static p2pt_prtn_t *
    prtn_list;

/*
**  prtn_list_lock is a mutex used to serialize access to the partition list
*/
static pthread_mutex_t
    prtn_list_lock = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************************
** pcb_for - returns the address of the partition control block for the
**           partition idenified by prtn_id
*****************************************************************************/
static p2pt_prtn_t *
   pcb_for( int prtn_id )
{
    p2pt_prtn_t *current_pcb;
    int found_prtn_id;

        if ( prtn_list != (p2pt_prtn_t *)NULL )
        {
            /*
            **  One or more partitions already exist in the partition list...
            **  Scan the existing partitions for a matching ID.
            */
            found_prtn_id = FALSE;
            for ( current_pcb = prtn_list; 
                  current_pcb != (p2pt_prtn_t *)NULL;
                  current_pcb = current_pcb->nxt_prtn )
            {
                if ( current_pcb->prtn_id == prtn_id )
                {
                    found_prtn_id = TRUE;
                    break;
                }
            }
            if ( found_prtn_id == FALSE )
                /*
                **  No matching ID found
                */
                current_pcb = (p2pt_prtn_t *)NULL;
        }
        else
            current_pcb = (p2pt_prtn_t *)NULL;
 
    return( current_pcb );
}

/*****************************************************************************
** new_prtn_id - automatically returns a valid, unused partition ID
*****************************************************************************/
static ULONG
   new_prtn_id( void )
{
    p2pt_prtn_t *current_pcb;
    ULONG new_prtn_id;

    /*
    **  Protect the queue list while we examine it.
    */

    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&prtn_list_lock );
    pthread_mutex_lock( &prtn_list_lock );

    /*
    **  Get the highest previously assigned queue id and add one.
    */
    if ( prtn_list != (p2pt_prtn_t *)NULL )
    {
        /*
        **  One or more queues already exist in the queue list...
        **  Find the highest queue ID number in the existing list.
        */
        new_prtn_id = prtn_list->prtn_id;
        for ( current_pcb = prtn_list; 
              current_pcb->nxt_prtn != (p2pt_prtn_t *)NULL;
              current_pcb = current_pcb->nxt_prtn )
        {
            if ( (current_pcb->nxt_prtn)->prtn_id > new_prtn_id )
            {
                new_prtn_id = (current_pcb->nxt_prtn)->prtn_id;
            }
        }

        /*
        **  Add one to the highest existing queue ID
        */
        new_prtn_id++;
    }
    else
    {
        /*
        **  this is the first queue being added to the queue list.
        */
        new_prtn_id = 1;
    }
 
    /*
    **  Re-enable access to the queue list by other threads.
    */
    pthread_mutex_unlock( &prtn_list_lock );
    pthread_cleanup_pop( 0 );

    return( new_prtn_id );
}

/*****************************************************************************
** link_pcb - appends a new partition control block pointer to the prtn_list
*****************************************************************************/
static void
   link_pcb( p2pt_prtn_t *new_prtn )
{
    p2pt_prtn_t *current_pcb;

    /*
    **  Protect the partition list while we examine and modify it.
    */
    pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                          (void *)&prtn_list_lock );
    pthread_mutex_lock( &prtn_list_lock );

    new_prtn->nxt_prtn = (p2pt_prtn_t *)NULL;
    if ( prtn_list != (p2pt_prtn_t *)NULL )
    {
        /*
        **  One or more partitions already exist in the partition list...
        **  Insert the new entry in ascending numerical sequence by prtn_id.
        */
        for ( current_pcb = prtn_list; 
              current_pcb->nxt_prtn != (p2pt_prtn_t *)NULL;
              current_pcb = current_pcb->nxt_prtn )
        {
            if ( (current_pcb->nxt_prtn)->prtn_id > new_prtn->prtn_id )
            {
                new_prtn->nxt_prtn = current_pcb->nxt_prtn;
                break;
            }
        }
        current_pcb->nxt_prtn = new_prtn;
#ifdef DIAG_PRINTFS 
        printf("\r\nadd partition cb @ %p to list @ %p", new_prtn, current_pcb);
#endif
    }
    else
    {
        /*
        **  this is the first partition being added to the partition list.
        */
        prtn_list = new_prtn;
#ifdef DIAG_PRINTFS 
        printf("\r\nadd partition cb @ %p to list @ %p", new_prtn, &prtn_list);
#endif
    }
 
    /*
    **  Re-enable access to the partition list by other threads.
    */
    pthread_mutex_unlock( &prtn_list_lock );
    pthread_cleanup_pop( 0 );
}

/*****************************************************************************
** unlink_pcb - removes a partition control block pointer from the prtn_list
*****************************************************************************/
static p2pt_prtn_t *
   unlink_pcb( int prtn_id )
{
    p2pt_prtn_t *current_pcb;
    p2pt_prtn_t *selected_pcb;

    selected_pcb =  (p2pt_prtn_t *)NULL;

    if ( prtn_list != (p2pt_prtn_t *)NULL )
    {
        /*
        **  One or more partitions exist in the partition list...
        **  Protect the partition list while we examine and modify it.
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&prtn_list_lock );
        pthread_mutex_lock( &prtn_list_lock );

        /*
        **  Scan the partition list for a pcb with a matching partition ID
        */
        if ( prtn_list->prtn_id == prtn_id )
        {
            /*
            **  The first partition in the list matches the partition ID
            */
            selected_pcb = prtn_list; 
            prtn_list = selected_pcb->nxt_prtn;
#ifdef DIAG_PRINTFS 
            printf( "\r\ndel partition cb @ %p from list @ %p", selected_pcb,
                    &prtn_list );
#endif
        }
        else
        {
            /*
            **  Scan the next pcb for a matching prtn_id while retaining a
            **  pointer to the current pcb.  If the next pcb matches,
            **  select it and then unlink it from the partition list.
            */
            for ( current_pcb = prtn_list; 
                  current_pcb->nxt_prtn != (p2pt_prtn_t *)NULL;
                  current_pcb = current_pcb->nxt_prtn )
            {
                if ( (current_pcb->nxt_prtn)->prtn_id == prtn_id )
                {
                    /*
                    **  Queue ID of next pcb matches...
                    **  Select the pcb and then unlink it by linking
                    **  the selected pcb's next pcb into the current pcb.
                    */
                    selected_pcb = current_pcb->nxt_prtn;
                    current_pcb->nxt_prtn = selected_pcb->nxt_prtn;
#ifdef DIAG_PRINTFS 
                    printf( "\r\ndel partition cb @ %p from list @ %p",
                            selected_pcb, current_pcb );
#endif
                    break;
                }
            }
        }

        /*
        **  Re-enable access to the partition list by other threads.
        */
        pthread_mutex_unlock( &prtn_list_lock );
        pthread_cleanup_pop( 0 );
    }

    return( selected_pcb );
}

/*****************************************************************************
** new_extent_for - allocates space for an extent control block, whioh maps
**                  partition data areas.  Initializes the free block list in
**                  the data block specified and updates the partition control
**                  block to reflect the addition of the new data extent to
**                  the free block and extent lists.
*****************************************************************************/
static prtn_extent_t *
    new_extent_for( p2pt_prtn_t *prtn, char *datablk, unsigned long numblks )
{
    prtn_extent_t *new_extent;
    size_t extent_data_size;
    char *block_ptr;

    /*
    **  The prtn_extent_t contains a pointer to the data block and the
    **  number of bytes in the data block.
    **  Now allocate a block of memory to contain the extent control block.
    */
    if ( (new_extent = (prtn_extent_t *)ts_malloc( sizeof( prtn_extent_t ) )) !=
                                                           (void *)NULL )
    {
        if ( datablk != (char *)NULL )
        {
            /*
            **  Fill in the extent control block itself.
            */
            new_extent->baddr = datablk;
            new_extent->bcount = numblks;

            /*
            **  Clear the data block memory.
            */
            extent_data_size = (size_t)(prtn->blk_size * numblks);
            if ( extent_data_size )
                memset( datablk, 0, extent_data_size );

            /*
            **  Initialize the free block list for the extent.  The free block
            **  list is a forward-linked list of pointers to each of the
            **  unused data blocks in the partition.  The linked list pointers
            **  are kept in the start of the data blocks themselves, and
            **  are overwritten when the blocks are allocated for use.  The
            **  pointer in the last data block is left NULL to terminate
            **  the list.
            */
            for (block_ptr = datablk;
                (block_ptr + prtn->blk_size) < (datablk + extent_data_size);
                 block_ptr += prtn->blk_size )
            {
                /*
                **  Write a pointer to the next data block into the
                **  first few bytes of each data block in the extent.
                */
                *((char **)block_ptr) = (block_ptr + prtn->blk_size);
#ifdef DIAG_PRINTFS 

                printf( "\r\n   add prtn_data_block @ %p nxt_blk @ %p",
                        block_ptr, *((char **)block_ptr) );
#endif
            }
#ifdef DIAG_PRINTFS 
                printf( "\r\n   add prtn_data_block @ %p nxt_blk @ %p",
                        block_ptr, *((char **)block_ptr) );
#endif

            /*
            **  Link the new extent into the partition control block
            */
            prtn->data_extent = new_extent;

            /*
            **  First data block in this extent is first free block
            */
            prtn->first_free = (char **)datablk;

            /*
            **  Last block in new extent is new last_free block
            */
            prtn->last_free = (char **)block_ptr;

            /*
            ** Initialize total number of free data blocks in partition
            */
            prtn->free_blk_count = new_extent->bcount;
        }
        else
        {
            ts_free( (void *)new_extent );
            new_extent = (prtn_extent_t *)NULL;
        }
    }
    return( new_extent );
}

/*****************************************************************************
** pt_create - creates a new memory management area from which fixed-size
**             data blocks may be allocated for applications use.
*****************************************************************************/
ULONG
    pt_create( char name[4], void *paddr, void *laddr, ULONG length,
               ULONG bsize, ULONG flags, ULONG *ptid, ULONG *nbuf )
{
    p2pt_prtn_t *prtn;
    ULONG error;
    int i;

    error = ERR_NO_ERROR;

    /*
    **  First allocate memory for the partition control block.
    */
    prtn = (p2pt_prtn_t *)ts_malloc( sizeof( p2pt_prtn_t ) );
    if ( prtn != (p2pt_prtn_t *)NULL )
    {
        /*
        **  Ok... got a control block.
        **  Prepare to allocate and init the first partition data extent.
        */

        /*
        ** Total number of used data blocks in partition
        */
        prtn->used_blk_count = 0L;

        /*
        **  Pointer to data extent for partition
        */
        prtn->data_extent = (prtn_extent_t *)NULL;

        /*
        ** Total data blocks per memory allocation block (extent)
        */
        if ( (bsize % 2) || (bsize < 4) )
            error = ERR_BUFSIZE;
        prtn->blk_size = bsize;

        if ( new_extent_for( prtn, paddr, (length / bsize) ) !=
             (prtn_extent_t *)NULL )
        {
            /*
            ** ID for partition
            */
            prtn->prtn_id = new_prtn_id();
            
            /*
            **  Name for partition
            */
            for ( i = 0; i < 4; i++ )
                prtn->ptname[i] = name[i];

            /*
            ** Option Flags for partition
            */
            prtn->flags = flags;

            /*
            ** Mutex for partition get/release block
            */
            pthread_mutex_init( &(prtn->prtn_lock),
                                (pthread_mutexattr_t *)NULL );
            /*
            **  If no errors thus far, we have a new partition ready to link
            **  into the partition list.
            */
            if ( error == ERR_NO_ERROR )
            {
			    /*
				**  Partition is linked by the acsending sequence of prtn_id. 
				*/
                link_pcb( prtn );

                /*
                **  Return the partition ID into the caller's storage location.
                */
                if ( ptid != (ULONG *)NULL )
                    *ptid = prtn->prtn_id;

                /*
                **  Return the number of buffers in the partition into
                **  the caller's storage location.
                */
                if ( nbuf != (ULONG *)NULL )
                    *nbuf = prtn->free_blk_count;
            }
            else
            {
                /*
                **  Oops!  Problem somewhere above.  Release control block
                **  and data memory and return.
                */
                ts_free( (void *)prtn->data_extent );
                ts_free( (void *)prtn );
            }
        }
        else
        {
            /*
            **  No memory for partition data... free partition control block
            */
            ts_free( (void *)prtn );
            error = ERR_OBJTFULL;
        }
    }
    else
    {
        error = ERR_OBJTFULL;
    }

    return( error );
}

/*****************************************************************************
** delete_prtn - takes care of destroying the specified partition and freeing
**                any resources allocated for that partition
*****************************************************************************/
static void
   delete_prtn( p2pt_prtn_t *prtn )
{
    /*
    **  First remove the partition from the partition list
    */
    unlink_pcb( prtn->prtn_id );

    /*
    **  Next delete the extent control block allocated for partition data.
    */
    ts_free( (void *)prtn->data_extent );

    /*
    **  Finally delete the partition control block itself;
    */
    ts_free( (void *)prtn );

}

/*****************************************************************************
** pt_delete - removes the specified partition from the partition list and
**             frees the memory allocated for the partition control block
**             and extents.
*****************************************************************************/
ULONG
    pt_delete( ULONG ptid )
{
    p2pt_prtn_t *prtn;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (prtn = pcb_for( ptid )) != (p2pt_prtn_t *)NULL )
    {

        /*
        ** Lock mutex for partition delete
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(prtn->prtn_lock));
        pthread_mutex_lock( &(prtn->prtn_lock) );

        if ( prtn->flags & PT_DEL )
        {
            /*
            **  Delete the partition even if buffers still allocated from it.
            */
			/*
			**  Note: here we should not use pthread_mutex_unlock(&(prtn->prtn_lock))
			**  to unlock the mutex, cause delete_prtn has do it.
			*/
            sched_lock();
            delete_prtn( prtn );
            sched_unlock();
        }
        else
        {
            /*
            **  Ensure that none of the partition's buffers are allocated.
            */
            sched_lock();
            if ( prtn->used_blk_count > 0L ) 
            {
                error = ERR_BUFINUSE;

                /*
                **  Unlock the mutex for the condition variable
                */
                pthread_mutex_unlock( &(prtn->prtn_lock) );
            }
            else
            {
                /*
                **  No buffers are allocated from the partition. Delete it.
                */
                error = ERR_NO_ERROR;
                delete_prtn( prtn );
            }
            sched_unlock();
        }

        pthread_cleanup_pop( 0 );
    }
    else
    {
        error = ERR_OBJDEL;
    }

    return( error );
}

/*****************************************************************************
** pt_getbuf - obtains a free data buffer from the specified memory partition
*****************************************************************************/
ULONG
    pt_getbuf( ULONG ptid, void **bufaddr )
{
    p2pt_prtn_t *prtn;
    char *blk_ptr;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (prtn = pcb_for( ptid )) != (p2pt_prtn_t *)NULL )
    {
        /*
        ** Lock mutex for partition block allocation
        */
        pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                              (void *)&(prtn->prtn_lock));
        pthread_mutex_lock( &(prtn->prtn_lock) );

        /*
        **  Each free data block contains a pointer to the next free data
        **  block in the partition.  This pointer is a pointer to a char
        **  as well as a pointer to a pointer to a char, since it is the
        **  address of both the next block and the next link pointer.
        **  The caller wants the address of a data block, so cast the
        **  first_free pointer to point to the first free data block
        **  and give that block to the caller.  NOTE that if the free list
        **  is empty, blk_ptr will be returned as a NULL pointer.
        */
        blk_ptr = (char *)prtn->first_free;

        if ( blk_ptr != (char *)NULL )
        {
            /*
            **  Take the contents of the first free data block and interpret
            **  its first bytes as the address of the link pointer in the
            **  following data block.  This address becomes the new value for
            **  the first_free pointer for the partition.  This means data
            **  blocks are always allocated from the front of the free list.
            */
            prtn->first_free = *((char ***)blk_ptr);

            /*
            **  Adjust the block counters to reflect the fact that another
            **  block was just allocated.
            */
            prtn->free_blk_count--;
            prtn->used_blk_count++;
#ifdef DIAG_PRINTFS 
            printf( "\r\npt_getbuf allocated block @ %p from partition %ld nxt_blk @ %p",
                    blk_ptr, ptid, *((char **)blk_ptr) );
#endif
        }
        else
        {
            error = ERR_NOBUF;
#ifdef DIAG_PRINTFS 
            printf( "\r\npt_getbuf - no blocks free in partition %ld", ptid );
#endif
        }

        /*
        **  Unlock the mutex for the condition variable and clean up.
        */
        pthread_mutex_unlock( &(prtn->prtn_lock) );
        pthread_cleanup_pop( 0 );
    }
    else
    {
        blk_ptr = (char *)NULL;
        error = ERR_OBJDEL;       /* Invalid prtn specified */
#ifdef DIAG_PRINTFS 
            printf( "\r\npt_getbuf - partition %ld not found", ptid );
#endif
    }

    /*
    **  Return the allocated block address (or NULL) to the caller's
    **  storage location.
    */
    if ( bufaddr != (void **)NULL )
        *bufaddr = (void *)blk_ptr;

    return( error );
}

/*****************************************************************************
** pt_retbuf - releases a data buffer back to the specified memory partition
*****************************************************************************/
ULONG
    pt_retbuf( ULONG ptid, void *bufaddr )
{
    p2pt_prtn_t *prtn;
    prtn_extent_t *extent;
    unsigned long extent_size;
    char *blk_ptr;
    ULONG error;

    error = ERR_NO_ERROR;

    if ( (prtn = pcb_for( ptid )) != (p2pt_prtn_t *)NULL )
    {

        /*
        **  Ensure that the block being returned falls within this
        **  partition's data memory range.
        */
        blk_ptr = (char *)bufaddr;
        extent = prtn->data_extent;
        extent_size = extent->bcount * prtn->blk_size;
#ifdef DIAG_PRINTFS 
        printf( "\r\npt_retbuf bufaddr @ %p extent base @ %p size %lx",
                blk_ptr, extent->baddr, extent_size );
#endif
        if ( (blk_ptr == extent->baddr) ||
             ((blk_ptr > extent->baddr) && 
              (blk_ptr < (extent->baddr + extent_size))) )
        {
            /*
            ** Lock mutex for partition block release
            */
            pthread_cleanup_push( (void(*)(void *))pthread_mutex_unlock,
                                  (void *)&(prtn->prtn_lock));
            pthread_mutex_lock( &(prtn->prtn_lock) );

            /*
            **  Search the partition's free list to see if the caller's
            **  buffer has already been freed.
            */
            for ( blk_ptr = (char *)prtn->first_free;
                  blk_ptr != (char *)NULL;
                  blk_ptr = *(char **)blk_ptr )
            {
#ifdef DIAG_PRINTFS 
                printf( "\r\npt_retbuf cur_blk @ %p nxt_blk @ %p",
                        blk_ptr, *(char **)blk_ptr );
#endif
                if ( blk_ptr == (char *)bufaddr )
                {
                    error = ERR_BUFFREE;
                    break;
                }
            }

            if ( error == ERR_NO_ERROR )
            {
                /*
                **  The caller's buffer address falls within the partition
                **  and has not already been freed...
                **  Insert a list-terminating NULL pointer into the block 
                **  being freed up.  Then link the block into the free list
                **  after the previous last free block.  This means free blocks
                **  are always returned to the rear of the free list, ensuring
                **  an even distribution of use for the blocks in the partition.
                */
                blk_ptr = (char *)bufaddr;
                *(char **)blk_ptr = (char *)NULL;
                *(prtn->last_free) = blk_ptr;
                prtn->last_free = (char **)blk_ptr;
                if ( prtn->first_free == (char **)NULL )
                    prtn->first_free = (char **)blk_ptr;

#ifdef DIAG_PRINTFS 
                printf( "\r\npt_retbuf returned block @ %p to partition %ld",
                        blk_ptr, ptid );
#endif

                /*
                **  Adjust the block counters to reflect the fact that another
                **  block was just released.
                */
                prtn->free_blk_count++;
                prtn->used_blk_count--;
            }
#ifdef DIAG_PRINTFS 
            else
            {
                printf( "\r\npt_retbuf - block @ %p already freed", blk_ptr );
            }
#endif
            /*
            **  Unlock the mutex for the condition variable and clean up.
            */
            pthread_mutex_unlock( &(prtn->prtn_lock) );
            pthread_cleanup_pop( 0 );
        }
        else
        {
            error = ERR_BUFADDR;
#ifdef DIAG_PRINTFS 
            printf( "\r\npt_retbuf - block @ %p not in partition %ld",
                    blk_ptr, ptid );
#endif
        }
    }
    else
    {
        error = ERR_OBJDEL;       /* Invalid prtn specified */
    }

    return( error );
}

/*****************************************************************************
** pt_ident - identifies the named p2pthread partition
*****************************************************************************/
ULONG
    pt_ident( char name[4], ULONG node, ULONG *ptid )
{
    p2pt_prtn_t *current_pcb;
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
        **  We'll ASSUME the ptid pointer isn't NULL!
        */
        if ( name == (char *)NULL )
        {
            *ptid = (ULONG)NULL;
            error = ERR_OBJNF;
        }
        else
        {
            /*
            **  Scan the task list for a name matching the caller's name.
            */
            for ( current_pcb = prtn_list;
                  current_pcb != (p2pt_prtn_t *)NULL;
                  current_pcb = current_pcb->nxt_prtn )
            {
                if ( (strncmp( name, current_pcb->ptname, 4 )) == 0 )
                {
                    /*
                    **  A matching name was found... return its QID
                    */
                    *ptid = current_pcb->prtn_id;
                    break;
                }
            }
            if ( current_pcb == (p2pt_prtn_t *)NULL )
            {
                /*
                **  No matching name found... return caller's QID with error.
                */
                *ptid = (ULONG)NULL;
                error = ERR_OBJNF;
            }
        }
    }

    return( error );
}

