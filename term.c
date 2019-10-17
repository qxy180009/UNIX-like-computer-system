#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "simos.h"

//=========================================================================
// Terminal manager is responsible for printing an output string to terminal.
// When there is a term output, the process has to be in eWait state and
// we insert both pid and the output string to the terminal queue.
// After terminal output is done, we need to put process back to ready state,
// which has to be done by the process manager.
// The terminal can only put process in endWait queue and set the interrupt.
//=========================================================================

// terminal output file name and file descriptor
// write terminal output to a file, to avoid messy printout
#define termFN "terminal.out"   
FILE *fterm;

void terminal_output (int pid, char *outstr);


//=========================================================================
// terminal output queue 
// implemented as a list with head and tail pointers
//=========================================================================

typedef struct TermQnodeStruct
{ int pid, type;
  char *str;
  struct TermQnodeStruct *next;
} TermQnode;

TermQnode *termQhead = NULL;
TermQnode *termQtail = NULL;

// if terminal queue is empty, wait on term_semaq
// for each insersion, signal term_semaq
// essentially, term_semaq.count keeps track of #req in the queue
sem_t term_semaq;

// for access the queue head and queue tail
sem_t term_mutex;

// dump terminal queue is not inside the terminal thread,
// only called by admin.c
void dump_termio_queue ()
{ TermQnode *node;

  printf ("******************** Term Queue Dump\n");
  node = termQhead;
  while (node != NULL)
  { printf ("%d, %s\n", node->pid, node->str);
    node = node->next;
  }
  printf ("\n");
}

// insert terminal queue is not inside the terminal thread, but called by
// the main thread when terminal output is needed (only in cpu.c, process.c)
void insert_termio (pid, outstr, type)
int pid, type;
char *outstr;
{ 
  sem_wait(&term_mutex);
  TermQnode *node;
  if (1) printf ("Insert term queue %d %s\n", pid, outstr);
  node = (TermQnode *) malloc (sizeof (TermQnode));
  node->pid = pid;
  node->str = outstr;
  node->type = type;
  node->next = NULL;
  if (termQtail == NULL) // termQhead would be NULL also
  { termQtail = node; termQhead = node; }
  else // insert to tail
  { termQtail->next = node; termQtail = node; }
  sem_post(&term_semaq); ///////////////////////////////////////////////////////////////////
  sem_post(&term_mutex);
  if (Debug) dump_termio_queue ();
}

// remove the termIO job from queue and call terminal_output for printing
// after printing, put the job to endWait list and set endWait interrupt
void handle_one_termio ()
{ TermQnode *node;
  if (Debug) dump_termio_queue ();
  sem_wait(&term_semaq);
  sem_wait(&term_mutex);
  // if (termQhead == NULL){
  //   // printf ("No process in terminal queue!!!\n");
  // }
  node = termQhead;
  if(node != NULL) {
    // node = termQhead;
    terminal_output (node->pid, node->str);
    if (node->type != endIO)
    { insert_endWait_process (node->pid);
      set_interrupt (endWaitInterrupt);
    }   // if it is the endIO type, then job done, just clean termio queue

    if (Debug) printf ("Remove term queue %d %s\n", node->pid, node->str);
    termQhead = node->next;
    if (termQhead == NULL) termQtail = NULL;
    free (node->str);
    free (node);
    if (Debug) dump_termio_queue ();
  }
  sem_post(&term_mutex);
}


//=====================================================
// IO function, loop on handle_one_termio to process the termio requests
// This has to be a separate thread to loop on the termio_one_handler
//=====================================================

// pretent to take a certain amount of time by sleeping printTime
// output is now simply printf, but it should be sent to client terminal
void terminal_output (pid, outstr)
int pid;
char *outstr;
{
  fprintf (fterm, "terminal_output: %s\n", outstr);
  fflush (fterm);
  usleep (termPrintTime);
}

void *termIO ()
{
  while (systemActive) handle_one_termio ();
  if (Debug) printf ("TermIO loop has ended\n");
}

pthread_t termThread;

void start_terminal ()
{ int ret;
  sem_init(&term_mutex, 0, 1);////////////////////////////////////////////////////////////////////////
  sem_init(&term_semaq, 0, 0);////////////////////////////////////////////////////////////////////////
  fterm = fopen (termFN, "w");
  ret = pthread_create (&termThread, NULL, termIO, NULL);
  if (ret < 0) printf ("TermIO thread creation problem\n");
  else printf ("TermIO thread has been created successsfully\n");
}

void end_terminal ()
{ int ret;
  sem_post(&term_semaq);
  fclose (fterm);
  ret = pthread_join (termThread, NULL);
  printf ("TermIO thread has terminated %d\n", ret);
}


