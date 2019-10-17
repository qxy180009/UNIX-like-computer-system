#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include "simos.h"

// need to be consistent with paging.c: mType and constant definitions
#define opcodeShift 24
#define operandMask 0x00ffffff
#define diskPage -2

unsigned pageoffsetMask;
int pagenumShift; // 2^pagenumShift = pageSize

FILE *progFd;

//==========================================
// load program into memory and build the process, called by process.c
// a specific pid is needed for loading, since registers are not for this pid
//==========================================

// may return progNormal or progError (the latter, if the program is incorrect)
int load_instruction (mType *buf, int page, int offset)
{ 
  // load instruction to buffer
  // calculate the address
  // read the instr and data in the load functions
  int maddr;
  int pid, opcode, operand;
  
  fscanf (progFd, "%d %d\n", &opcode, &operand);
  if (Debug) printf ("Process %d load instruction: %d, %d, %d\n",
                                   pid, offset, opcode, operand);
  	maddr = offset;
  if (maddr == mError) return (mError);
  else
  { opcode = opcode << opcodeShift;
    operand = operand & operandMask;
    buf[maddr].mInstr = opcode | operand;
    printf("opcode: %d operand: %d\n", buf[maddr].mInstr>>opcodeShift, buf[maddr].mInstr & operandMask);
    return (mNormal);
  }
}

int load_data (mType *buf, int page, int offset)
{ 
  // load data to buffer (same as load instruction, but use the mData field
  int maddr;
  int pid, data;
  
  fscanf (progFd, "%d\n", &data);
  if (Debug) printf ("Process %d load data: %d, %d\n",
                                   pid, offset, data);
  	maddr = offset;
  if (maddr == mError) return (mError);
  else
  { 
    buf[maddr].mData = data;
    printf("data: %f\n", buf[maddr].mData);
    return (mNormal);
  }

}

// load program to swap space, returns the #pages loaded
int load_process_to_swap (int pid, char *fname)
{ 
  // read from program file "fname" and call load_instruction & load_data
  // to load the program into the buffer, write the program into
  // swap space by inserting it (mType *buf -> unsigned *buf) to swapQ
  // update the process page table to indicate that the page is not empty ================= not done!!================
  // and it is on disk (= diskPage)
  init_process_pagetable(pid);

  int msize, numinstr, numdata;
  int ret, i, opcode, operand;
  float data;

  mType *buf = (mType *)malloc(pageSize*sizeof(mType));
  progFd = fopen (fname, "r");
  printf("fname is : %s\n", fname);
  if (progFd == NULL)
  { printf ("Submission Error: Incorrect program name: %s!\n", fname);
    return progError;
  }
  ret = fscanf (progFd, "%d %d %d\n", &msize, &numinstr, &numdata);
  if (ret < 3)   // did not get all three inputs
  { printf ("Submission failure: missing %d program parameters!\n", 3-ret);
    return progError;
  }

  base = numinstr;

  if((msize/pageSize+1) > (freeFtail - freeFhead)) {
    printf("Invalid memory size %d for process %d\n", msize, pid);
    return (progError);
  }
  // load the instr and data to the BUF

  for (i=0; i<numinstr; i++)
  { 
    int page = i / pageSize;
    int offset = i % pageSize;
    ret = load_instruction (buf, page, offset);
    if (ret == mError) {
    	PCB[pid]->exeStatus = eError; return (mError); }
    if (offset == pageSize - 1)
    { 
      insert_swapQ(pid, page, (unsigned *)buf, actWrite, Nothing);
      // printf("end of a page: page: %d; offset: %d\n", page, offset);
      bzero(buf, 32);
      PCB[pid]->PTptr[page] = diskPage;
    }
    // ===========================
  }
  for (; i<numdata + numinstr; i++)
  { 
    int page = i / pageSize;
    int offset = i % pageSize;
    ret = load_data (buf, page, offset);
    if (ret == mError) { PCB[pid]->exeStatus = eError; return (mError); }
    if (offset == pageSize - 1 || i == numdata + numinstr - 1)
    {
      insert_swapQ(pid, page, (unsigned *)buf, actWrite, Nothing);
      PCB[pid]->PTptr[page] = diskPage;
      bzero(buf, 32);
    }
    if (Debug) printf ("Process %d load data: %d, %.2f\n", pid, i, data);
  }

}

int load_pages_to_memory (int pid, int numpage)
{
  // call insert_swapQ to load the pages of process pid to memory
  // #pages to load = min (loadPpages, numpage = #pages loaded to swap for pid)
  // ask swap.c to place the process to ready queue only after the last load
  // do not forget to update the page table of the process
  // this function has some similarity with page fault handler

  pageoffsetMask = 0b00000111;
  pagenumShift = 3; // 2^pagenumShift = pageSize
  
  int numPpages;
  if (numpage > loadPpages)
  {
    numPpages = loadPpages;
  }else numPpages = numpage;
  for (int i = 0; i < numPpages; ++i)
  {
    // i is the page number
    int frameNum = get_free_frame();
    int maddr;
    printf("get_free_frame>>>>>>>>>>>>>>>>>>>: %d\n", frameNum);
    // unsigned buf[pageSize*dataSize];
    mType *buf = (mType *) malloc (pageSize*sizeof(mType));

    insert_swapQ(pid, i,(unsigned *) buf, actRead, toReady);
  	// The contents for a certain page are now in buf;
  	// question: how to differ data from instr
  	for (int k = 0; k < pageSize; ++k)
  	{
  		maddr = (k & pageoffsetMask) | (frameNum << pagenumShift);

      if(maddr >= base + pageSize * OSpages){
        Memory[maddr].mData = buf[k].mData;
        printf("offset: %d, frameNum: %d ", k, frameNum);
        printf("maddr: %d data: %f \n", maddr,
         Memory[maddr].mData);
      }else{
    		Memory[maddr].mInstr = buf[k].mInstr;
    		printf("offset: %d, frameNum: %d ", k, frameNum);
    		printf("maddr: %d opcode[k]: %d operand: %d\n", maddr,
    		 Memory[maddr].mInstr>>opcodeShift,
    		 Memory[maddr].mInstr & operandMask);
      }
  	}
    PCB[pid]->numPF = numPpages;
  	printf("pid: %d, page: %d, findex: %d\n", pid, i, frameNum);
    update_process_pagetable(pid,i,frameNum);
    update_frame_info(frameNum, pid, i);
  }
}

int load_process (int pid, char *fname)
{ int ret;
  ret = load_process_to_swap (pid, fname);   // return #pages loaded
  if (ret != progError) load_pages_to_memory (pid, ret);
  return (ret);
}

// load idle process, idle process uses OS memory
// We give the last page of OS memory to the idle process
#define OPifgo 5   // has to be consistent with cpu.c
void load_idle_process ()
{ int page, frame;
  int instr, opcode, operand, data;

  init_process_pagetable (idlePid);
  page = 0;   frame = OSpages - 1;
  update_process_pagetable (idlePid, page, frame);
  update_frame_info (frame, idlePid, page);
  
  // load 1 ifgo instructions (2 words) and 1 data for the idle process
  opcode = OPifgo;   operand = 0;
  instr = (opcode << opcodeShift) | operand;
  direct_put_instruction (frame, 0, instr);   // 0,1,2 are offset
  direct_put_instruction (frame, 1, instr);
  direct_put_data (frame, 2, 1);
}

