#include <stdio.h>
#include "simos.h"


void process_admin_command ()
{ char action[10];
  // char *action;
  char fname[100];
  int round, i;

  while (systemActive)
  { printf ("command> ");
    scanf ("%s", action);
    if (Debug) printf ("Command is %c\n", action[0]);
    // only first action character counts, discard remainder
    switch (action[0])
    { case 's':  // submit
        one_submission (); break;
      case 'x':  // execute
        execute_process (); break;
      case 'y':  // multiple rounds of execution
        printf ("Iterative execution: #rounds? ");
        scanf ("%d", &round);
        for (i=0; i<round; i++)
        { execute_process();
          if (Debug) { dump_memoryframe_info(); dump_PCB_memory(); }
        }
        break;
      case 'q':  // dump ready queue and list of processes completed IO
        dump_ready_queue ();
        dump_endWait_list ();
        break;
      case 'r':   // dump the list of available PCBs
        dump_registers (); break;
      case 'p':   // dump the list of available PCBs
        dump_PCB_list (); break;
      case 'm':   // dump memory of each process
        dump_PCB_memory (); break;
      case 'f':   // dump memory frames and free frame list
        dump_memoryframe_info (); break;
      case 'n':   // dump the content of the entire memory
        dump_memory (); break;
      case 'e':   // dump events in clock.c
        dump_events (); break;
      case 't':   // dump terminal IO queue
        dump_termio_queue (); break;
      case 'w':   // dump swap queue
        dump_swapQ (); break;
      case 'T':  // Terminate, do nothing, terminate in while loop
        systemActive = 0; break;
      default:   // can be used to yield to client submission input
        printf ("Error: Incorrect command!!!\n");
    }
  }
  printf ("Admin command processing loop ended!\n");
}


