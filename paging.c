#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "simos.h"

// Memory definitions, including the memory itself and a page structure
// that maintains the informtion about each memory page
// config.sys input: pageSize, numFrames, OSpages
// ------------------------------------------------
// process page table definitions
// config.sys input: loadPpages, maxPpages

// mType *Memory;   // The physical memory, size = pageSize*numFrames
////////////////////////////////////////////////////////////////////////////
// typedef unsigned ageType;
// typedef struct
// { int pid, page;   // the frame is allocated to process pid for page page
//   ageType age;
//   char free, dirty, pinned;   // in real systems, these are bits
//   int next, prev;
// } FrameStruct;

// FrameStruct *memFrame;   // memFrame[numFrames]
// int freeFhead, freeFtail;   // the head and tail of free frame list
/////////////////////////////////////////////////////////////////////////////

// define special values for page/frame number
#define nullIndex -1   // free frame list null pointer
#define nullPage -1   // page does not exist yet
#define diskPage -2   // page is on disk swap space
#define pendingPage -3  // page is pending till it is actually swapped
   // have to ensure: #memory-frames < address-space/2, (pageSize >= 2)
   //    becuase we use negative values with the frame number
   // nullPage & diskPage are used in process page table 
//////////////////////////////////////////////////////////////////////////
// define values for fields in FrameStruct
// #define zeroAge 0x00000000
// #define highestAge 0x80000000
// #define dirtyFrame 1
// #define cleanFrame 0
// #define freeFrame 1
// #define usedFrame 0
// #define pinnedFrame 1
// #define nopinFrame 0
//////////////////////////////////////////////////////////////////////////


// define shifts and masks for instruction and memory address 
#define opcodeShift 24
#define operandMask 0x00ffffff

// shift address by pagenumShift bits to get the page number
unsigned pageoffsetMask;
int pagenumShift; // 2^pagenumShift = pageSize

//============================
// Our memory implementation is a mix of memory manager and physical memory.
// get_instr, put_instr, get_data, put_data are the physical memory operations
//   for instr, instr is fetched into registers: IRopcode and IRoperand
//   for data, data is fetched into registers: MBR (need to retain AC value)
// page table management is software implementation
//============================


//==========================================
// run time memory access operations, called by cpu.c
//==========================================

// define rwflag to indicate whehter the addr computation is for read or write
#define flagRead 1
#define flagWrite 2

int in_page = -1;

// address calcuation are performed for the program in execution
// so, we can get address related infor from CPU registers
int calculate_memory_address (unsigned offset, int rwflag)
{ 
  // rwflag is used to differentiate the caller
  // different access violation decisions are made for reader/writer
  // if there is a page fault, need to set the page fault interrupt
  // also need to set the age and dirty fields accordingly
  // returns memory address or mPFault or mError 

  int page = offset / pageSize;
  int maddr = (offset & pageoffsetMask) | (PCB[CPU.Pid]->PTptr[page] << pagenumShift);
  printf("( CPU.PC : %d, page : %d, maddr : %d )\n", offset, page, maddr);
  if (maddr < pageSize*OSpages && maddr >= 0)
  { printf ("Process %d accesses %d. In OS region!\n", CPU.Pid, maddr);
    return (mError);
  }
  else if (maddr >= pageSize*numFrames) // outside of the memory (mError)
  { printf ("Process %d accesses %d. Outside memory!\n", CPU.Pid, maddr);
    return (mError);
  }
  // printf("calcuation: the memFrame[%d].free %d now!\n", *(CPU.PTptr), memFrame[*(CPU.PTptr)].free);
  // if (memFrame[*(CPU.PTptr)].free == freeFrame)
  if(PCB[CPU.Pid]->PTptr[page] < 0) // -1 indicates that the desired page is in the swap disk;
  {
  	// printf("page fault handling!\n");
    set_interrupt(pFaultException);
    return (mPFault);}
  else if (memFrame[PCB[CPU.Pid]->PTptr[page]].free == freeFrame){
    if (rwflag == flagRead)
    {
      printf ("Process %d accesses %d. maddr (access violation)!\n", CPU.Pid, maddr);
      return (mError);
    }else if (rwflag == flagWrite)
    {
      printf ("Process %d accesses %d. writing to an unused page!\n", CPU.Pid, maddr);
      // return (mPFault);
      memFrame[PCB[CPU.Pid]->PTptr[page]].free = usedFrame;
    }
  }

  if (Debug) printf ("content = %.2f, %x\n",
                        Memory[maddr].mData, Memory[maddr].mInstr);
  return (maddr);
}

int get_data (int offset)
{ 
  // call calculate_memory_address to get memory address
  // copy the memory content to MBR
  // return mNormal, mPFault or mError
  // printf("get_data offset: %d\n", offset);
  int maddr; 
  printf("get data >>>>>>>>>>>>>>>>>>>>>>>>>>> ");
  maddr = calculate_memory_address(offset, flagRead) + base;
  // printf("maddr: %d\n", maddr);
  if (calculate_memory_address(offset, flagRead) == mError) return (mError);
  else if (calculate_memory_address(offset,flagRead) == mPFault){
    return (mPFault);
  }else if(maddr >= PCB[CPU.Pid]->numPF * pageSize + OSpages * pageSize)
  {
    in_page = (maddr - ( OSpages * pageSize )) / pageSize;
    printf("OUTSITE: page fault handling! maddr: %d\n", maddr);
    set_interrupt(pFaultException);
    return (mPFault);
  }
  else
  { CPU.MBR = Memory[maddr].mData;
    printf("maddr: %d CPU.MBR: %f\n", maddr, CPU.MBR);
    return (mNormal);
  }
}

int put_data (int offset)
{ 
  // call calculate_memory_address to get memory address
  // copy MBR to memory 
  // return mNormal, mPFault or mError
  int maddr;
  maddr = calculate_memory_address(offset, flagWrite) + base;
  if (calculate_memory_address(offset, flagWrite) == mError) return (mError);
  else if(calculate_memory_address(offset, flagWrite) == mPFault){
    return (mPFault);
  }else if (maddr >= PCB[CPU.Pid]->numPF * pageSize + OSpages * pageSize)
  {
    in_page = (maddr - ( OSpages * pageSize )) / pageSize;
    set_interrupt(pFaultException);
    return (mPFault);
  }
  else
  { Memory[maddr].mData = CPU.AC;
    memFrame[CPU.Pid].free = usedFrame;
    memFrame[CPU.Pid].dirty = dirtyFrame;

    return (mNormal);
  }
}

int get_instruction (int offset)
{ 
  // call calculate_memory_address to get memory address
  // convert memory content to opcode and operand
  // return mNormal, mPFault or mError
  int maddr, instr;
  printf("get instruction >>>>>>>>>>>>>>>>>>>> ");
  maddr = calculate_memory_address(offset, flagRead);
  if (maddr == mError) return (mError);
  else if (maddr == mPFault){
    in_page = CPU.PC / pageSize;
    return mPFault;
  }
  else
  { 
    // need to check whether the page is used or not
    instr = Memory[maddr].mInstr;
    CPU.IRopcode = instr >> opcodeShift; 
    CPU.IRoperand = instr & operandMask;
    printf("instruction==: %d, %d\n", CPU.IRopcode, CPU.IRoperand);
    return (mNormal);
  }
}

// these two direct_put functions are only called for loading idle process
// no specific protection check is done
void direct_put_instruction (int findex, int offset, int instr)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mInstr = instr;
}

void direct_put_data (int findex, int offset, mdType data)
{ int addr = (offset & pageoffsetMask) | (findex << pagenumShift);
  Memory[addr].mData = data;
}

//==========================================
// Memory and memory frame management
//==========================================

// void dump_one_frame (int findex)
// { 
//   printf("pid: %d, page: %d, age: %x, free: %d, dirty: %d, pinned: %d\n", 
//     memFrame[findex].pid,memFrame[findex].page,memFrame[findex].age,
//     memFrame[findex].dirty,memFrame[findex].pinned);

// }

void dump_memory ()
{ int i;

  printf ("************ Dump the entire memory\n");
  for (i=0; i<numFrames; i++) print_one_frameinfo (i);
}

// above: dump memory content, below: only dump frame infor

void dump_free_list ()
{ 
  // dump the list of free memory frames
  // traverse the free list
  printf ("******************** Free list Metadata\n");
  int curFrame = freeFhead;
  while(curFrame != freeFtail){
    print_one_frameinfo(curFrame);
    curFrame = memFrame[curFrame].next;
  }
}

void print_one_frameinfo (int indx)
{ printf ("pid/page/age=%d,%d,%x, ",
          memFrame[indx].pid, memFrame[indx].page, memFrame[indx].age);
  printf ("dir/free/pin=%d/%d/%d, ",
          memFrame[indx].dirty, memFrame[indx].free, memFrame[indx].pinned);
  printf ("next/prev=%d,%d\n",
          memFrame[indx].next, memFrame[indx].prev);
}

void dump_memoryframe_info ()
{ int i;

  printf ("******************** Memory Frame Metadata\n");
  printf ("Memory frame head/tail: %d/%d\n", freeFhead, freeFtail);
  for (i=OSpages; i<numFrames; i++)
  { printf ("Frame %d: ", i); print_one_frameinfo (i); }
  dump_free_list ();
}

void update_frame_info (findex, pid, page)
int findex, pid, page;
{
  // update the metadata of a frame, need to consider different update scenarios
  // need this function also becuase loader also needs to update memFrame fields
  // while it is better to not to expose memFrame fields externally

  if (pid == nullPid)
  {
    memFrame[findex].page = nullPage;
    memFrame[findex].age = zeroAge;
    memFrame[findex].dirty = cleanFrame;
    memFrame[findex].free = freeFrame;
    memFrame[findex].pid = nullPid;
  }else{
    memFrame[findex].page = page;
    memFrame[findex].age = highestAge;
    memFrame[findex].dirty = dirtyFrame;
    memFrame[findex].free = usedFrame;
    memFrame[findex].pid = pid;
    printf("memFrame[%d].age: %x \n", findex, memFrame[findex].age);
  }

}

// should write dirty frames to disk and remove them from process page table
// but we delay updates till the actual swap (page_fault_handler)
// unless frames are from the terminated process (status = nullPage)
// so, the process can continue using the page, till actual swap
void addto_free_frame (int findex, int status)
{
  //write dirty frames to disk (save the contents)
  // write_swap_page(memFrame[findex].pid, memFrame[findex].page, buf); ////////////////////////////////////////
  // remove the frame from the process page table

}

int select_agest_frame ()
{ 
  // select a frame with the lowest age 
  // if there are multiple frames with the same lowest age, then choose the one
  // that is not dirty

  // 1. Scan all the frames in the memory (not the swap space), get the smallest
  ageType lowestAge = memFrame[OSpages].age;
  for (int i = OSpages; i < numFrames; ++i)
   {
     if (memFrame[i].age < lowestAge)
     {
       lowestAge = memFrame[i].age;
     }
   } 

  // 2. Scan again, add to freelist; update the information;
  int first = numFrames;
  int count = 0;
  for (int i = OSpages; i < numFrames; ++i)
  {
    // dirty or not && equal to lowestAge;
    if (memFrame[i].age == lowestAge)
    {
      // freelist
      if (memFrame[i].dirty != dirtyFrame)
      {
        memFrame[freeFtail].next = i;
        freeFtail = i;
      }
      count++;
      if (first > i) first = i;
    }
  }
  if (count > 1)
    return freeFhead;
  else if(count == 1)
    return first;
  // insert the dirty frame will be done in the page_fault_handler()
}

int get_free_frame ()
{ 
// get a free frame from the head of the free list 
// if there is no free frame, then get one frame with the lowest age
// this func always returns a frame, either from free list or get one with lowest age
  if (freeFhead != freeFtail) {
    int Fhead = freeFhead;
    freeFhead = memFrame[freeFhead].next;
    return Fhead;
  }
  else return select_agest_frame();
} 

void initialize_memory ()
{ int i;

  // create memory + create page frame array memFrame 
  Memory = (mType *) malloc (numFrames*pageSize*sizeof(mType));
  memFrame = (FrameStruct *) malloc (numFrames*sizeof(FrameStruct));

  // compute #bits for page offset, set pagenumShift and pageoffsetMask
  // *** ADD CODE
  pagenumShift = 3;
  pageoffsetMask = 0b00000111; // Binary data;

  // initialize OS pages
  for (i=0; i<OSpages; i++)
  { memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = usedFrame;
    memFrame[i].pinned = pinnedFrame;
    memFrame[i].pid = osPid;
  }
  // initilize the remaining pages, also put them in free list
  // *** ADD CODE
  freeFhead = OSpages;
  freeFtail = numFrames;

  for (i = OSpages; i < numFrames; ++i)
  {
    memFrame[i].pid = nullPid;
    memFrame[i].age = zeroAge;
    memFrame[i].dirty = cleanFrame;
    memFrame[i].free = freeFrame;
    memFrame[i].pinned = pinnedFrame;
    // add Frame[i] to the freelist;
    memFrame[freeFtail].next = i;
    freeFtail = i;
  }
}

//==========================================
// process page table manamgement
//==========================================

void init_process_pagetable (int pid)
{ int i;
  PCB[pid]->PTptr = (int *) malloc (addrSize*maxPpages);
  for (i=0; i<maxPpages; i++) {
    PCB[pid]->PTptr[i] = nullPage;
  }
}

// frame can be normal frame number or nullPage, diskPage
void update_process_pagetable (pid, page, frame)
int pid, page, frame;
{ 
  // update the page table entry for process pid to point to the frame
  // or point to disk or null

  PCB[pid]->PTptr[page] = frame;

}

int free_process_memory (int pid)
{ 
  // free the memory frames for a terminated process
  // some frames may have already been freed, but still in process pagetable

  for (int i = OSpages; i < maxPpages; ++i)
  {
    if (memFrame[i].pid == pid)
    {

      memFrame[i].age = zeroAge; 

      memFrame[i].free = freeFrame;
      memFrame[i].page = nullPage;
      memFrame[i].pid = nullIndex;
      memFrame[i].dirty = cleanFrame;

      memFrame[freeFtail].next = i;
      freeFtail = i;
    }
  }

}

void dump_process_pagetable (int pid)
{ 
  // print page table entries of process pid

}

void dump_process_memory (int pid)
{ 
  // print out the memory content for process pid
}

//==========================================
// the major functions for paging, invoked externally
//==========================================

#define sendtoReady 1  // has to be the same as those in swap.c
#define notReady 0   
#define actRead 0   
#define actWrite 1

void page_fault_handler ()
{ 
  // handle page fault
  // obtain a free frame or get a frame with the lowest age
  // unsigned buf[pageSize*dataSize];
  mType *buf = (mType *)malloc(pageSize*sizeof(mType));
  int frameNum = get_free_frame();
  int maddr;
  // if the frame is dirty, insert a write request to swapQ
  if (memFrame[frameNum].dirty == dirtyFrame && CPU.Pid != nullPid)
  {
    // obtain the contents of the dirty frame (i.e. the address)
    // one page has 8 offsets
    mType *out_buf = (mType *)malloc(pageSize*sizeof(mType));
    int out_pid = memFrame[frameNum].pid;
    int out_page = memFrame[frameNum].page;
    int out_maddr;
    for (int offset = 0; offset < pageSize; ++offset)
    {
      out_maddr = (offset & pageoffsetMask) | (frameNum << pagenumShift);
      out_buf[offset] = Memory[out_maddr];
    }
    insert_swapQ(out_pid, out_page, (unsigned *)out_buf, actWrite, freeBuf);
    update_process_pagetable(out_pid, out_page, diskPage);
  }
  printf("get_free_frame>>>>>>>>>>>>>>>>>>>: %d\n", frameNum);
  // insert a read request to swapQ to bring the new page to this frame
  insert_swapQ(CPU.Pid, in_page, (unsigned *)buf, actRead, toReady);

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
      // printf("Memory[maddr].mInstr: %d\n", Memory[maddr].mInstr);
      printf("offset: %d, frameNum: %d ", k, frameNum);
      printf("maddr: %d opcode[k]: %d operand: %d\n", maddr,
       Memory[maddr].mInstr>>opcodeShift,
       Memory[maddr].mInstr & operandMask);
    }
  }

  // update the frame metadata and the page tables of the involved processes
  // bothe the swaped out and swapped in processes should be considered;//////////////////////////////////
  PCB[CPU.Pid]->numPF++;
  update_frame_info(frameNum, CPU.Pid, in_page);
  update_process_pagetable(CPU.Pid, in_page, frameNum);
}

// scan the memory and update the age field of each frame
void memory_agescan ()
{ 
  for (int i = OSpages; i < numFrames; ++i)
  {
    memFrame[i].age = memFrame[i].age >> 4;
    if (memFrame[i].age == zeroAge)
    {
      // printf("memFrame[i].dirty: %d\n", memFrame[i].dirty);
      if (memFrame[i].dirty == dirtyFrame)
      {
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        int maddr, instr;
        mType *buf; // not sure!!!!!!!!!

        int out_pid = memFrame[i].pid;
        int out_page = PCB[out_pid]->PTptr[memFrame[i].page];
        for (int offset = 0; offset < pageSize; ++offset)
        {
          maddr = (offset & pageoffsetMask) | (out_page << pagenumShift);
          buf[offset].mInstr = Memory[maddr].mInstr;
          buf[offset].mData = Memory[maddr].mData;
        }
        insert_swapQ(out_pid, out_page, (unsigned *)buf, actWrite, freeBuf);
      }
      memFrame[freeFtail].next = i;
      freeFtail = i;
      memFrame[i].free = freeFrame;
      memFrame[i].page = nullPage;
    }
  }
}

void initialize_memory_manager ()
{ 
  // initialize memory and add page scan event request
  initialize_memory();
  add_timer (periodAgeScan, osPid, actAgeInterrupt, periodAgeScan);
}
