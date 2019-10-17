#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "simos.h"


int currentPid = 2;    // user pid should start from 2, pid=0/1 are OS/idle
int numUserProcess = 0; 

//============================================
// context switch, switch in or out a process pid

void context_in (int pid)
{ 
  // *** ADD CODE to switch in the context from PCB to CPU
  CPU.Pid = pid;
  CPU.PC = PCB[pid]->PC;
  CPU.AC = PCB[pid]->AC;
  CPU.exeStatus = PCB[pid]->exeStatus;
  CPU.PTptr = PCB[pid]->PTptr;
}

void context_out (int pid, int intime)
{  
  // *** ADD CODE to switch out the context from CPU to PCB
  // I dont include intime because I dont know what the intime means, lag time?
  PCB[pid]->PC = CPU.PC;
  PCB[pid]->AC = CPU.AC;
  PCB[pid]->exeStatus = CPU.exeStatus;
  PCB[pid]->PTptr = CPU.PTptr;
}

//=========================================================================
// ready queue management
// Implemented as a linked list with head and tail pointers
// The ready queue needs to be protected in case insertion comes from
// process submission and removal from process execution
//=========================================================================

#define nullReady 0
       // when get_ready_process encoutered empty queue, nullReady is returned

typedef struct ReadyNodeStruct
{ int pid;
  struct ReadyNodeStruct *next;
} ReadyNode;

ReadyNode *readyHead = NULL;
ReadyNode *readyTail = NULL;


void insert_ready_process (pid)
int pid;
{ ReadyNode *node;

  node = (ReadyNode *) malloc (sizeof (ReadyNode));
  node->pid = pid;
  node->next = NULL;
  if (readyTail == NULL) // readyHead would be NULL also
    { readyTail = node; readyHead = node; }
  else // insert to tail
    { readyTail->next = node; readyTail = node; }
}

int get_ready_process ()
{ ReadyNode *rnode;
  int pid;

  if (readyHead == NULL)
  { printf ("No ready process now!!!\n");
    return (nullReady); 
  }
  else
  { pid = readyHead->pid;
    rnode = readyHead;
    readyHead = rnode->next;
    free (rnode);
    if (readyHead == NULL) readyTail = NULL;
  }
  return (pid);
}

void dump_ready_queue ()
{ ReadyNode *node;

  printf ("******************** Ready Queue Dump\n");
  node = readyHead;
  while (node != NULL) { printf ("%d, ", node->pid); node = node->next; }
  printf ("\n");
}


//=========================================================================
// endWait list management
// processes that has finished waiting can be inserted into endWait list
//   -- when adding process to endWait list, should set endWaitInterrupt
//      interrupt handler moves processes in endWait list to ready queue
// The list needs to be protected because multiple threads may insert
// to endWait list and a thread will remove nodes in the list concurrently
//=========================================================================

sem_t pmutex;

typedef struct EndWaitNodeStruct
{ int pid;
  struct EndWaitNodeStruct *next;
} EndWaitNode;

EndWaitNode *endWaitHead = NULL;
EndWaitNode *endWaitTail = NULL;

void insert_endWait_process (int pid)
{ EndWaitNode *node;

  sem_wait (&pmutex);
  node = (EndWaitNode *) malloc (sizeof (EndWaitNode));
  node->pid = pid;
  node->next = NULL;
  if (endWaitTail == NULL) // endWaitHead would be NULL also
    { endWaitTail = node; endWaitHead = node; }
  else // insert to tail
    { endWaitTail->next = node; endWaitTail = node; }
  sem_post (&pmutex);
}

// move all processes in endWait list to ready queue, empty the list
// need to set exeStatus from eWait to eReady

void endWait_moveto_ready ()
{ EndWaitNode *node;

  sem_wait (&pmutex);
  while (endWaitHead != NULL)
  { node = endWaitHead;
    insert_ready_process (node->pid);
    PCB[node->pid]->exeStatus = eReady;
    endWaitHead = node->next;
    free (node);
  }
  endWaitTail = NULL;
  sem_post (&pmutex);
}

void dump_endWait_list ()
{ EndWaitNode *node;

  node = endWaitHead;
  printf ("endWait List = ");
  while (node != NULL) { printf ("%d, ", node->pid); node = node->next; }
  printf ("\n");
}

//=========================================================================
// Some support functions for PCB 
// PCB related definitions are in simos.h
//=========================================================================

void init_PCB_ptrarry ()
{ PCB = (typePCB **) malloc (maxProcess*addrSize); }

int new_PCB ()
{ int pid;

  pid = currentPid;
  currentPid++;
  if (pid >= maxProcess)
  { printf ("Exceeding maximum number of processes: %d\n", pid);
    return (-1);
  }
  PCB[pid] = (typePCB *) malloc ( sizeof(typePCB) );
  PCB[pid]->Pid = pid;
  return (pid);
}

void free_PCB (int pid)
{
  free (PCB[pid]);
  if (Debug) printf ("Free PCB: %d\n", pid);
  PCB[pid] = NULL;
}

void dump_PCB (int pid)
{
  printf ("******************** PCB Dump for Process %d\n", pid);
  printf ("Pid = %d\n", PCB[pid]->Pid);
  printf ("PC = %d\n", PCB[pid]->PC);
  printf ("AC = "mdOutFormat"\n", PCB[pid]->AC);
  printf ("PTptr = %p\n", PCB[pid]->PTptr);
  printf ("exeStatus = %d\n", PCB[pid]->exeStatus);
}

void dump_PCB_list ()
{ int pid;

  printf ("Dump all PCB: From 0 to %d\n", currentPid);
  for (pid=idlePid; pid<currentPid; pid++)
    if (PCB[pid] != NULL) dump_PCB (pid);
}

void dump_PCB_memory ()
{ int pid;

  printf ("Dump memory/swap of all processes: From 1 to %d\n", currentPid-1);
  dump_process_memory (idlePid);
  for (pid=idlePid+1; pid<currentPid; pid++)
    if (PCB[pid] != NULL) dump_process_memory (pid);
}


//=========================================================================
// process management
//=========================================================================

#define OPifgo 5
#define idleMsize 3
#define idleNinstr 2

// this function initializes the idle process
// idle process has only 1 instruction, ifgo (2 words) and 1 data
// the ifgo condition is always true and will always go back to 0

void clean_process (int pid)
{
  free_process_memory (pid);
  free_PCB (pid);  // PCB has to be freed last, other frees use PCB info
} 

void end_process (int pid)
{ PCB[pid]->exeStatus = CPU.exeStatus;
    // PCB[pid] is not updated, no point to do a full context switch

  // send end process print msg to terminal, str will be freed by terminal
  char *str = (char *) malloc (80);
  if (CPU.exeStatus == eError)
  { printf ("\aProcess %d has an error, dumping its states\n", pid);
    dump_PCB (pid);
    dump_process_memory (pid); 
    sprintf (str, "Process %d had encountered error in execution!!!\n", pid);
  }
  else  // was eEnd
  { printf ("Process %d had completed successfully: Time=%d, PF=%d\n",
             pid, PCB[pid]->timeUsed, PCB[pid]->numPF);
    sprintf (str, "Process %d had completed successfully: Time=%d, PF=%d\n",
             pid, PCB[pid]->timeUsed, PCB[pid]->numPF);
  }
  insert_termio (pid, str, endIO);

  // invoke io to print str, process has terminated, so no wait state

  numUserProcess--;
  clean_process (pid); 
    // cpu will clean up process pid without waiting for printing to finish
    // so, io should not access PCB[pid] for end process printing
}

void init_idle_process ()
{ 
  // create and initialize PCB for the idle process
  PCB[idlePid] = (typePCB *) malloc ( sizeof(typePCB) );

  PCB[idlePid]->Pid = idlePid;  // idlePid = 1, set in ???
  PCB[idlePid]->PC = 0;
  PCB[idlePid]->AC = 0;
  load_idle_process ();
  if (Debug) { dump_PCB (idlePid); dump_process_memory (idlePid); }
}

void initialize_process_manager ()
{
  init_PCB_ptrarry ();

  currentPid = 2;  // the next pid value to be used
  numUserProcess = 0;  // the actual number of processes in the system

  init_idle_process ();
  sem_init (&pmutex, 0, 1);
}

// submit_process always working on a new pid and the new pid will not be 
// used by anyone else till submit_process finishes working on it
// currentPid is not used by anyone else but the dump functions
// So, no conflict for PCB and Pid related data
// -----------------
// During insert_ready_process, there is potential of conflict accesses

int submit_process (char *fname)
{ int pid, ret, i;

  if ( ((numFrames-OSpages)/(numUserProcess+1)) < 2 )
    printf ("\aToo many processes => they may not execute due to page faults\n");
  else
  { pid = new_PCB ();
    if (pid > idlePid)
    { 
      ret = load_process (pid, fname);   // return #pages loaded
      if (ret > 0)
      { PCB[pid]->PC = 0;
        PCB[pid]->AC = 0;
        PCB[pid]->exeStatus = eReady;
        // swap manager will put the process to ready queue
        numUserProcess++;
        return (pid);
      }
      else free_PCB (pid);   // cannot clean_process(), no page table
  } }
  // abnormal situation, PCB has not been allocated or has been freed
  char *str = (char *) malloc (80);
  printf ("Program %s has loading problem!!!\n", fname);
  sprintf (str, "Program %s has loading problem!!!\n", fname);
  insert_termio (pid, str, endIO);
  return (-1);
}

void execute_process ()
{ int pid, intime;
  genericPtr event;

  pid = get_ready_process ();
  if (pid != nullReady)
  { 
    // *** ADD CODE to perform context switch and call cpu_execution
    // context_in();
    // also add code to keep track of accounting info: timeUsed & numPF
    context_in(pid);
    CPU.exeStatus = eRun;
    event = add_timer(cpuQuantum, CPU.Pid, actTQinterrupt, oneTimeTimer);
    cpu_execution();
    if (CPU.exeStatus == eReady) {
      context_out(pid,0);
      insert_ready_process (pid);
    }
    else if (CPU.exeStatus == ePFault || CPU.exeStatus == eWait) {
      context_out(pid,0);
      deactivate_timer (event);
    }
    else // CPU.exeStatus == eError or eEnd
      { 
      	end_process (pid); deactivate_timer (event); }
    // ePFault and eWait has to be handled differently
    // in ePFfault, interrupt is set and memory handles the interrupt
    // in eWait, CPU directly execute IO libraries and initiate IO
    // ----------------------------------
    // if exeStatus is not eReady, the process was not stopped by time quantum
    // and the time quantum timer (pointed by event) should be deactivated
    // otherwise, it has the potential of impacting exe of next process
    // but if time quantum just expires when the above cases happends,
    // event would have just been freed, our deactivation can be dangerous
  }
  else // no ready process in the system, so execute idle process
       // idle process will not have page fault, or go to wait state
       // or encoutering error or terminate (it is infinite)
       // so after execution, no need to check these status
       // only time quantum will stop idle process, and shoud use idleQuantum
  { context_in (idlePid);
    CPU.exeStatus = eRun;
    add_timer (idleQuantum, CPU.Pid, actTQinterrupt, oneTimeTimer);
    cpu_execution (); 
  }
}


