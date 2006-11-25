/********************************************************/
// Generic FIFO routines using POSIX shared memory		 
//                                                   
// Each FIFO buffer consists of a generic header for    
// handling the shared memory and basic FIFO functions  
// (i.e. head and tail pointers, base addresses, etc.), 
// a user header which describes something common about 
// the FIFO records (i.e. gate spacing, scaling factor, 
// etc.), and an array of FIFO records.			
//                                                      
// If the FIFO is full, the data just overwrites the    
// last FIFO record.					
//                                                      
// Shared memory is managed using a list which is also  
// shared memory. Programs can access the shared memory 
// containing this list by getting the address from a   
// file called /temp/sharedmem.list. This address needs 
// to be read once. Subsequent dynamic allocations of	
// shared memory FIFO buffers get reflected in the list 
// immediately and all programs have shared access to	
// that information.					
//                                                      
// The fifo users are awakened by UDP socket writes on up
// to N ports.
//
/********************************************************/

/* for shmem calls */
#include	<string.h>
#include	<stdio.h>

#include "piraqComm.h"
#define	ERROR	-1

/* This shorthand is useful */
#define	FP	fp->header

/* Allocate memory for a new fifo buffer. */
/* If the FIFO is already open, then just use it */
/* This assumes that a FIFO with a specific name is always */
/* used the same way by any task (i.e. size, structure type, etc.) */
PFIFO *pfifo_create(char *name, int headersize, int recordsize, int recordnum)
   {

   PFIFO	*fifo;
   fifo = (PFIFO *)PCIBASE;

   if(fifo)
      {
	   /* initialize the fifo structure */
	   strncpy(fifo->name,name,80);
	   fifo->header_off = sizeof(PFIFO);		/* pointer to the user header */
	   fifo->fifobuf_off = fifo->header_off + headersize;	/* pointer to fifo base address */
	   fifo->record_size = recordsize;						/* size in bytes of each PFIFO record */
	   fifo->record_num = recordnum;					/* number of records in PFIFO buffer */
	   fifo->head = fifo->tail = 0;							/* indexes to the head and tail records */
	   fifo->destroy_flag = 0;								/* destroy-when-empty flag */
	   fifo->sock = -1;  /* indicating it's unitialized */
	   fifo->port = 20000;
	   fifo->clients = 0;
	   fifo->magic = MAGIC;  /* do this last so clients can use it as done flag */
      }
      
   return(fifo);
   }


/* Get a pointer to the next available PFIFO. */
/* Returns NULL if none available (already allocated */
/* by pfifo_create). */
PFIFO *pfifo_open(char *name)
   {
   PFIFO	*fifo;
//   fifo = (PFIFO *)shared_mem_open(name);
   fifo = (PFIFO *)PCIBASE;
//   fifo = (PFIFO *)(volatile unsigned int *)0x14000C8; /* PLX Mailbox 2 */
   return(fifo);
   }

/* do your best to destroy and remove PFIFO from operating system */
int pfifo_close(PFIFO *fifo)
   {
   return(0);
   }

/* Return a pointer to the next working write record. */
/* Return -1 if error */
void *pfifo_get_write_address(PFIFO *fifo)
   {
   return((char *)fifo + fifo->fifobuf_off + fifo->record_size * fifo->head); 
   }


/* Return a pointer to a good pointer near the head */
/* Return -1 if error */
void *pfifo_get_last_address(PFIFO *fifo)
   {
   int	last;
   
   last = fifo->head - 1;
   if(last < 0)	last += fifo->record_num;
   return((char *)fifo + fifo->fifobuf_off + fifo->record_size * last); 
   }

/* Increment the PFIFO head pointer. Return the number of remaining */
/* records or -1 on error */
int pfifo_increment_head(PFIFO *fifo)
   {
   int	free;

   if(!fifo)	return(-1);

   /* this code throws away oldest data */
//   fifo->head = (fifo->head + 1) % fifo->record_num;		   /* increment head */
   fifo->head = ((fifo->head) + 1) % fifo->record_num;		   /* increment head */
   if(fifo->head == fifo->tail)   /* if you are about to overwrote some data */
      {
//      printf("PFIFO overflow (%s)\n",fifo->name);
      fifo->tail = (fifo->tail + 1) % fifo->record_num;	/* increment tail */
      }

   /* return the number of remaining records */
   free = fifo->tail - fifo->head;
   if(free <= 0)	free += fifo->record_num;
   return(free);
   }

/* Return a pointer to the next working read record. */
/* Return a NULL if the PFIFO is empty */
/* Return -1 if error */
void *pfifo_get_read_address(PFIFO *fifo, int offset)
   {
   int	offptr;
   
   if(!fifo)	return((void *)-1);

   offptr = fifo->tail + offset;
   while(offptr < 0) offptr += fifo->record_num;

   return((char *)fifo + fifo->fifobuf_off + fifo->record_size * offptr); 
   }

/* Increment the PFIFO tail pointer. Return the number of used */
/* records or -1 on error */
int pfifo_increment_tail(PFIFO *fifo)
   {
   if(!fifo)	return(-1);

   if(fifo->head == fifo->tail)	/* if empty */
      return(0);		/* return without incrementing */

   fifo->tail = (fifo->tail + 1) % fifo->record_num;	/* increment tail */

   return(pfifo_hit(fifo));
   }

/* Return number of records waiting to be read */
/* or -1 on error */
int pfifo_hit(PFIFO *fifo)
   {
   int	used;

   if(!fifo)	return(-1);

   used = fifo->head - fifo->tail;
   if(used < 0)	used += fifo->record_num;
   return(used);
   }

/* Return a pointer to the next working write record. */
/* Return -1 if error */
void *pfifo_get_header_address(PFIFO *fifo)
   {
   return((char *)fifo + fifo->header_off); 
   }
