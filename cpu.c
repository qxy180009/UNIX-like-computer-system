#include <stdio.h>
#include <stdlib.h>
#include "simos.h"


#define OPload 2
#define OPadd 3
#define OPmul 4
#define OPifgo 5
#define OPstore 6
#define OPprint 7
#define OPsleep 8
#define OPend 1


void initialize_cpu ()
{ // Generally, cpu goes to a fix location to fetch and execute OS
  CPU.interruptV = 0;
  CPU.numCycles = 0;
}

void dump_registers ()
{ 
  printf ("Pid=%d, ", CPU.Pid);
  printf ("PC=%d, ", CPU.PC);
  printf ("IR=(%d,%d), ", CPU.IRopcode, CPU.IRoperand);
  printf ("AC="mdOutFormat", ", CPU.AC);
  printf ("MBR="mdOutFormat"\n", CPU.MBR);
  printf ("          Status=%d, ", CPU.exeStatus);
  printf ("IV=%x, ", CPU.interruptV);
  printf ("PT=%x, ", *(CPU.PTptr));
  printf ("cycle=%d\n", CPU.numCycles);
}

void set_interrupt (unsigned bit)
{ CPU.interruptV = CPU.interruptV | bit; }

void clear_interrupt (unsigned bit)
{ 
  unsigned negbit = -bit - 1;
  printf ("IV is %x, ", CPU.interruptV);
  CPU.interruptV = CPU.interruptV & negbit;
  printf ("after clear is %x\n", CPU.interruptV);
}

void handle_interrupt ()
{
  if (Debug) 
    printf ("Interrupt handler: pid = %d; interrupt = %x; exeStatus = %d\n",
            CPU.Pid, CPU.interruptV, CPU.exeStatus); 
  while (CPU.interruptV != 0)
  { if ((CPU.interruptV & endWaitInterrupt) == endWaitInterrupt)
    { endWait_moveto_ready ();  
      // interrupt may overwrite, move all IO done processes (maybe > 1)
      clear_interrupt (endWaitInterrupt);
    }
    if ((CPU.interruptV & tqInterrupt) == tqInterrupt)
    { if (CPU.exeStatus == eRun) CPU.exeStatus = eReady;
      clear_interrupt (tqInterrupt);
    }
    ////////////////////////////////////////////////////
    if ((CPU.interruptV & ageInterrupt) == ageInterrupt){
      memory_agescan ();
      clear_interrupt(ageInterrupt);
    }
    if ((CPU.interruptV & pFaultException) == pFaultException)
    {
      page_fault_handler ();
      clear_interrupt(pFaultException);
    }
    // *** ADD CODE to handle page fault and periodical age scan
    /////////////////////////////////////////////
  }
}

void fetch_instruction ()
{ int mret;

  mret = get_instruction (CPU.PC);
  if (mret == mError) CPU.exeStatus = eError;
  else if (mret == mPFault) CPU.exeStatus = ePFault;
  else // fetch data, but exclude OPend and OPsleep, which has no data
       // also exclude OPstore, which stores data, not gets data
    if (CPU.IRopcode != OPend && CPU.IRopcode != OPsleep
        && CPU.IRopcode != OPstore)
    { mret = get_data (CPU.IRoperand);
      if (mret == mError) CPU.exeStatus = eError;
      else if (mret == mPFault) CPU.exeStatus = ePFault;
      else if (CPU.IRopcode == OPifgo)
      { mret = get_instruction (CPU.PC+1);
        if (mret == mError) CPU.exeStatus = eError;
        else if (mret == mPFault) CPU.exeStatus = ePFault;
        else { CPU.PC++; CPU.IRopcode = OPifgo; }
        // ifgo is different from other instructions, it has two words
        //   test variable is in memory, memory addr is in the 1st word
        //      get_data above gets it in MBR
        //   goto addr is in the operand field of the second word
      } //     we use get_instruction again to get it as the operand
    }   // we also advance PC to make it looks like ifgo only has 1 word
}       // ****** if there is page fault, PC will not be incremented

void execute_instruction ()
{ int gotoaddr, mret;
  switch (CPU.IRopcode)
  { case OPload:
      // *** ADD CODE for the instruction
      CPU.AC = CPU.MBR;
      break;
    case OPadd:
      // *** ADD CODE for the instruction
      CPU.AC += CPU.MBR;
      break;
    case OPmul:
      // *** ADD CODE for the instruction
      CPU.AC *= CPU.MBR;
      break;
    case OPifgo:
      // *** ADD CODE for the instruction
      if (CPU.MBR > 0)
      {
        CPU.PC = CPU.IRoperand - 1;
      }
      break;
    case OPstore:
      // *** ADD CODE for the instruction
      put_data (CPU.IRoperand); break;
    case OPprint:
      // *** ADD CODE for the instruction
    { 
      CPU.exeStatus = eWait; ///////////////////
      char *outstr = (char *) malloc (10);
      /////////////////////////////////
      printf("outstr: %f\n", CPU.MBR);
      sprintf(outstr,"%f",CPU.MBR);
      printf("outstr: %s\n", outstr);
      insert_termio (CPU.Pid, outstr, regularIO);
      break;
    }
    case OPsleep:
      // *** ADD CODE for the instruction
      CPU.exeStatus = eWait;
      add_timer(CPU.IRoperand, CPU.Pid, actReadyInterrupt, oneTimeTimer);
      break;
    case OPend:
      // *** ADD CODE for the instruction
      CPU.exeStatus = eEnd; break;
    default:
      printf ("Illegitimate OPcode in process %d\n", CPU.Pid);
      CPU.exeStatus = eError;
  }
}

void cpu_execution ()
{ int mret;

  // perform all memory fetches, analyze memory conditions all here
  while (CPU.exeStatus == eRun)
  { 
  	fetch_instruction ();
    if (Debug) { printf ("Fetched: "); dump_registers (); }
    if (CPU.exeStatus == eRun)
    { 
      execute_instruction ();
      // if it is eError or eEnd, does not matter
      // if it is page fault, then AC, PC should not be changed
      // because the instruction should be re-executed
      // so only execute if it is eRun
      if (CPU.exeStatus != ePFault) CPU.PC++;
        // the put_data may change exeStatus, need to check again
        // if it is ePFault, then data has not been put in memory
        // => need to set back PC so that instruction will be re-executed
        // no other instruction will cause problem and execution is done
      if (Debug) { printf ("Executed: "); dump_registers (); }
    }
    if (CPU.interruptV != 0) handle_interrupt ();
    advance_clock ();
      // since we don't have clock, we use instruction cycle as the clock
      // no matter whether there is a page fault or an error,
      // should handle clock increment and interrupt
  }
}

