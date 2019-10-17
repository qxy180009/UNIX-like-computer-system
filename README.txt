We have finished all three phases, but there are several deviations in the program and one bug at the end of the process.

The problem of the program is that after the completion of one process, further execution command 'x's will cause a segmentation fault. Also, after finishing one process, the on going memory address will step into the OS region, which might account for the appreance of the segmentation fault afterwards. 
The semaphores that are provided in the swap.c confuse me a little bit, so I modified the design to the case of two semaphores for syncronization(0) and one for mutex (1).


The new tar files are used. Besides the required swap.c, loader.c, paging.c, the following files are changed. 
 1.In the cpu.c, the execute_instruction() is completed and the memory scan interrupt and page_fault_interrupt are invoked.
 2.In the process.c, the context_in and context_out are written and invoked in the execute_process function.
 3.The declerations of some data structures and functions are moved to the simos.h to make the program work.
 4.The semaphores are completed in term.c which is designed exactly like proj3.


Contributions:
	The dump related functions are completed by Kumar Vedant, the rest of the functions are completed by Qiang Yao.
