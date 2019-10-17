#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include "simos.h"


//======================================================================
// This module handles swap space management.
// It has the simulated disk and swamp manager.
// First part is for the simulated disk to read/write pages.
//======================================================================

#define swapFname "swap.disk"
#define itemPerLine 8 // used to arrange the file

pthread_t swapThread;

int diskfd;
int swapspaceSize;
int PswapSize;
int pagedataSize;

sem_t swap_semaq;
sem_t swapq_mutex;
sem_t disk_mutex;

typedef struct SwapQnodeStruct
{ int pid, page, act, finishact;
  unsigned *buf;
  struct SwapQnodeStruct *next;
} SwapQnode;
// pidin, pagein, inbuf: for the page with PF, needs to be brought in
// pidout, pageout, outbuf: for the page to be swapped out
// if there is no page to be swapped out (not dirty), then pidout = nullPid
// inbuf and outbuf are the actual memory page content

SwapQnode *swapQhead = NULL;
SwapQnode *swapQtail = NULL;

//===================================================
// This is the simulated disk, including disk read, write, dump.
// The unit is a page
//===================================================
// each process has a fix-sized swap space, its page count starts from 0
// first 2 processes: OS=0, idle=1, have no swap space
// OS frequently (like Linux) runs on physical memory address (fixed locations)
// virtual memory is too expensive and unnecessary for OS => no swap needed

int read_swap_page (int pid, int page, unsigned *buf)
{ int location, ret, retsize, k;
  // buf is the content to be swapped in
  // reference the previous code for this part
  // but previous code was not fully completed

  if (pid < 2 || pid > maxProcess)
  { printf ("Error: Incorrect pid for disk read: %d\n", pid); 
    return (-1);
  }
  location = (pid-2) * maxPpages * pagedataSize + page*pagedataSize;
  ret = lseek (diskfd, location, SEEK_SET);
  if (ret < 0) perror ("Error lseek in read: \n");
  retsize = read (diskfd, (char *)buf, pagedataSize);
  if (retsize != pagedataSize)
  { printf ("Error: Disk read returned incorrect size: %d\n", retsize);
    perror("read: ");
    exit(-1);
  }
  usleep (diskRWtime);
}

int write_swap_page (int pid, int page, unsigned *buf)
{ 
  // reference the previous code for this part
  // but previous code was not fully completed
  int location, ret, retsize;

  if (pid < 2 || pid > maxProcess) 
  { printf ("Error: Incorrect pid for disk write: %d\n", pid); 
    return (-1);
  }
  location = (pid-2) * maxPpages * pagedataSize + page*pagedataSize;
  ret = lseek (diskfd, location, SEEK_SET);

  if (ret < 0) perror ("Error lseek in write: \n");
  retsize = write (diskfd, (char *)buf, pagedataSize);
  if (retsize != pageSize*dataSize) 
  { printf ("Error: Disk read returned incorrect size: %d\n", retsize); 
	exit(-1);
  }
  usleep (diskRWtime);
}

int dump_process_swap_page (int pid, int page)
{ 
  // reference the previous code for this part
  // but previous code was not fully completed
}

void dump_process_swap (int pid)
{ int j;

  printf ("****** Dump swap pages for process %d\n", pid);
  for (j=0; j<maxPpages; j++) dump_process_swap_page (pid, j);
}

// open the file with the swap space size, initialize content to 0
void initialize_swap_space ()
{ int ret, i, j, k;
  int buf[pageSize];

  swapspaceSize = maxProcess*maxPpages*pageSize*dataSize;
  PswapSize = maxPpages*pageSize*dataSize;
  pagedataSize = pageSize*dataSize;

  diskfd = open (swapFname, O_RDWR | O_CREAT, 0600);
  if (diskfd < 0) { perror ("Error open: "); exit (-1); }
  ret = lseek (diskfd, swapspaceSize, SEEK_SET); 
  if (ret < 0) { perror ("Error lseek in open: "); exit (-1); }
  // i is the pid
  for (i=2; i<maxProcess; i++)
    for (j=0; j<maxPpages; j++)
    { for (k=0; k<pageSize; k++) buf[k]=0;
      // read all the initial data and instructions into the swap space;
      write_swap_page (i, j, buf);
    }
    // last parameter is the origin, offset from the origin, which can be:
    // SEEK_SET: 0, SEEK_CUR: from current position, SEEK_END: from eof
}


//===================================================
// Here is the swap space manager. 
//===================================================
// When a process address to be read/written is not in the memory,
// meory raises a page fault and process it (in kernel mode).
// We implement this by cheating a bit.
// We do not perform context switch right away and switch to OS.
// We simply let OS do the processing.
// OS decides whether there is free memory frame, if so, use one.
// If no free memory, then call select_aged_page to free up memory.
// In either case, proceed to insert the page fault req to swap queue
// to let the swap manager bring in the page
//===================================================


void print_one_swapnode (SwapQnode *node)
{ printf ("pid,page=(%d,%d), act,ready=(%d, %d), buf=%p\n", 
           node->pid, node->page, node->act, node->finishact, node->buf);
}

void dump_swapQ ()
{ 
  // dump all the nodes in the swapQ
  SwapQnode *node = swapQhead;
  while(node != NULL){
    print_one_swapnode(node);
  }
}

// act can be actRead or actWrite
// finishact indicates what to do after read/write swap disk is done, it can be:
// toReady (send pid back to ready queue), freeBuf: free buf, Both, Nothing
void insert_swapQ (pid, page, buf, act, finishact)
int pid, page, act, finishact;
unsigned *buf;
{ 
  // simply add all the parameters in the swap queue (to the tail)
  sem_wait(&swapq_mutex);
  printf("insert_swapQ once!\n");
  SwapQnode *node;
  node = (SwapQnode *)malloc(sizeof(SwapQnode));
  node->pid = pid;
  node->page = page;
  node->act = act;
  node->finishact = finishact;
  node->buf = buf;
  node->next = NULL;
  if (swapQtail == NULL)
  {
    swapQhead = node;
    swapQtail = node;
  }else{
    swapQtail->next = node;
    swapQtail = node;
  }
  sem_post(&disk_mutex);
  sem_post(&swapq_mutex);

  sem_wait(&swap_semaq);
}

void *process_swapQ ()
{
  // called as the entry function for the swap thread
  // traverse the swap queue to perform thea read and write swap (where the read / write swap operations);
  // semaphores are required to protect the swap queue
  while(systemActive){
    sem_wait(&disk_mutex);
    while(swapQhead != NULL){
      sem_wait(&swapq_mutex);
      SwapQnode *snode;
      if(swapQhead->act == actWrite){
        write_swap_page(swapQhead->pid, swapQhead->page, swapQhead->buf);
      }else if(swapQhead->act == actRead){
        read_swap_page(swapQhead->pid, swapQhead->page, swapQhead->buf);
      }
      if (swapQhead->finishact == toReady)
      {
        insert_ready_process (swapQhead->pid);
      }
      else if (swapQhead->finishact == freeBuf)
      {
        bzero(swapQhead->buf, pageSize*dataSize);
      }
      snode = swapQhead;
      swapQhead = snode->next;
      free(snode);
      if(swapQhead == NULL) swapQtail = NULL;
      sem_post(&swapq_mutex);
    }
    sem_post(&swap_semaq);
  }  
}

void start_swap_manager ()
{ 
  // initialize_swap_space ();
  // create swap thread
  int ret;
  sem_init(&swap_semaq,0,0);
  sem_init(&swapq_mutex,0,1);
  sem_init(&disk_mutex,0,0);
  initialize_swap_space();
  ret = pthread_create(&swapThread, NULL, process_swapQ, NULL);
  if (ret < 0) printf ("swap manager thread creation problem\n");
  else printf ("swap manager thread has been created successsfully\n");

}

void end_swap_manager ()
{ 
  // terminate the swap thread
  int ret;
  sem_post(&swap_semaq);
  sem_post(&disk_mutex);
  ret = pthread_join (swapThread, NULL);
  printf ("swap manager thread has terminated %d\n", ret);
}
