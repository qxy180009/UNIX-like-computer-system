#include <stdio.h>
#include "simos.h"


void initialize_system ()
{ FILE *fconfig;
  char str[60];

  fconfig = fopen ("config.sys", "r");
  fscanf (fconfig, "%d %d %d %s\n",
          &maxProcess, &cpuQuantum, &idleQuantum, str);
  fscanf (fconfig, "%d %d %s\n", &pageSize, &numFrames, str);
  fscanf (fconfig, "%d %d %d %s\n", &loadPpages, &maxPpages, &OSpages, str);
  fscanf (fconfig, "%d %d %d %s\n",
          &periodAgeScan, &termPrintTime, &diskRWtime, str);
  fscanf (fconfig, "%d %d %d %d %d %s\n", &Debug,
          &cpuDebug, &memDebug, &swapDebug, &clockDebug, str);
  fclose (fconfig);

  // all processing has a while loop on systemActive
  // admin with T command can stop the system
  systemActive = 1;

  initialize_timer ();
  initialize_cpu ();
  initialize_memory_manager ();
  initialize_process_manager ();
}

// initialize system mainly intialize data structures of each component.
// start_terminal, process_client_submission, process_admin_command are
// system operations and are started in main.
void main ()
{
  initialize_system ();
  start_terminal ();   // term.c
  start_swap_manager ();   // swap.c
  //start_client_submission ();
  process_admin_command ();   // admin.c

  // admin terminated the system, wait for other components to terminate
  //end_client_submission ();   // submit.c
  end_terminal ();   // term.c
  end_swap_manager ();
}

