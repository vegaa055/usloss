/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
void enableInterrupts();
void disableInterrupts();
static void check_deadlock();
void check_kernel_mode();
void init_proc_table(int i);
int zap(int pid);
int is_zapped();
int getpid();
int find_proc_slot();
void add_proc_to_readylist(proc_ptr proc);
void remove_from_readylist(proc_ptr proc);

/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

static proc_ptr ReadyList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

void clock_handler(int dev, void *arg)
{

}
/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   // check if currently in kernel mode
   check_kernel_mode();

   int i = 0;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   if(DEBUG && debugflag)
      console("startup(): initializing process table - ProcTable[]\n");

   ReadyList = NULL;

   for (i = 0; i < MAXPROC; i++)
   {
      init_proc_table(i);
   }
   
   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");



   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_INT] = clock_handler; 
   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }
  
   
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");

   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);

   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - init proc tables
   Purpose - Initializes the process table properties to an initial value
   Parameters - Integer value i - the index being sent
   Returns - nothing
   Side Effects - the properties within the process table ProcTable are changed. 
   ----------------------------------------------------------------------- */
void init_proc_table(int i)
{
   check_kernel_mode();
   disableInterrupts();
   proc_struct current_proc = ProcTable[i];

   // initialize ProcTable
   current_proc.next_proc_ptr = NULL;
   current_proc.child_proc_ptr = NULL;
   current_proc.next_sibling_ptr = NULL;
   current_proc.parent_ptr = NULL;
   current_proc.quit_child_ptr = NULL;
   current_proc.next_sibling_quit = NULL;
   current_proc.who_zapped = NULL;
   current_proc.next_who_zapped = NULL;
   strcpy(current_proc.name, "");
   current_proc.start_arg[0] = '\0';
   current_proc.pid = -1;
   current_proc.priority = -1;
   current_proc.start_func = NULL;
   current_proc.stack = NULL;
   current_proc.stacksize = -1;
   current_proc.status = STATUS_EMPTY;
   current_proc.quitStatus = STATUS_EMPTY;
   current_proc.startTime = -1;
   current_proc.zapped = 0;
   current_proc.cpuStartTime = -1;
}

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*func)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   check_kernel_mode();
   disableInterrupts();

   /* Return if stack size is too small */
   if(stacksize < USLOSS_MIN_STACK)
   {
      console("fork1(): process %s stack size too small\n", name);
      return -2;  //TODO return value??
   }

   /* find an empty slot in the process table */
   proc_slot = find_proc_slot();

   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }

   ProcTable[proc_slot].pid = next_pid;
   //  set the proc name
   strcpy(ProcTable[proc_slot].name, name);
   // set the proc start func
   ProcTable[proc_slot].start_func = func;

   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) 
   {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   /* Initialize the new process's stack */
   ProcTable[proc_slot].stack = malloc(stacksize);

   if(ProcTable[proc_slot].stack == NULL)
   {
      console("malloc failure. Halting...\n");
      halt(1);
   }

   // Set the stack size and priority of the new process
   ProcTable[proc_slot].stacksize = stacksize;

   ProcTable[proc_slot].priority = priority;

   // If there is a currently running process, 
   // add the new process as a child or sibling
   if(Current != NULL)
   {
      // If the current process has no child, set the new process as its child
      if(Current->child_proc_ptr == NULL)
      {
         Current->child_proc_ptr = &ProcTable[proc_slot];
      }
      // Otherwise, find the last sibling of the current process 
      // and set the new process as its next sibling
      else
      {
         proc_ptr child = Current->child_proc_ptr;

         while(child->next_sibling_ptr != NULL)
         {
            child = child->next_sibling_ptr;
         }
         child->next_sibling_ptr = &ProcTable[proc_slot];
      }
   }

   ProcTable[proc_slot].parent_ptr = Current;

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   /* Make process ready and add to ready list*/
   ProcTable[proc_slot].status = STATUS_READY;
   
   add_proc_to_readylist(&ProcTable[proc_slot]);
   next_pid++;

   // call dispatcher
   if(func != sentinel)
   {
      dispatcher();
   }
   
   return ProcTable[proc_slot].pid;

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   Current->status = STATUS_RUNNING;
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
   // check if in kernel mode
   check_kernel_mode();
   disableInterrupts();


} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{

   int current_pid;

   check_kernel_mode();

   disableInterrupts();

   if(Current->child_proc_ptr != NULL)
   {
      console("quit(): process %s has children.\n", Current->name);
      halt(1);
   }

   Current->status = STATUS_QUIT;
   remove_from_readylist(Current);


   // Are there any zappers
   p1_quit(Current->pid);

   Current->status = STATUS_QUIT;

   // foreach zapper, wakeup
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
   proc_ptr next_process;

   // pick the next process to run

   // context switch to it
   p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char * dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */

/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
} /* check_deadlock */

void enableInterrupts()
{

}

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - 
   Parameters - int pid
   Returns - 
   Side Effects - 
   ------------------------------------------------------------------------ */
int zap(int pid)
{
   int result = 0;
   proc_ptr zap_ptr;

   // make sure that PSR is in kernel mode and disable interupts
   check_kernel_mode();
   disableInterrupts();

   /* make sure we don't zap ourselves */
   if(getpid() == pid)
   {
      console("Zap: process %d is attempting to zap itself.\n", pid);
      halt(1);
   }

   /* check if attempting to zap non-existant process */
   if(ProcTable[pid%MAXPROC].status == STATUS_EMPTY || ProcTable[pid%MAXPROC].pid != pid)
   {
      console("Zap: process being zapped does not exist.\n");
      halt(1);
   }

/* Process to be zapped has finished running but is waiting for a parent */
   if(ProcTable[pid%MAXPROC].status == STATUS_QUIT)
   {
      if(DEBUG && debugflag)
      {
         console("The process being zapped has quit but not joined.\n");
      }

      /* zapped by another process */
      if(is_zapped()){
         result = -1;
      }
      result = 0; //Might be redundant but just in case
   }

   /* mark the process as zapped */
   
   /* block until pid quits */
   Current->status = STATUS_ZAP_BLOCKED;
   // remove the process from the readylist
   remove_from_readylist(Current);
   // set the zap_ptr and zapped property to 1 to mark as 'zapped'
   zap_ptr = &ProcTable[pid%MAXPROC];
   zap_ptr->zapped = 1;

   if(zap_ptr->who_zapped == NULL)
   {
      // If nobody has previously zapped the process, set Current as the first to do so
      zap_ptr->who_zapped = Current;
   }
   else
   {
      // If somebody has previously zapped the process, set Current as the most recent to do so.
      // Set Current's next_who_zapped to the previous value of who_zapped so we don't lose the
      // previous zapper
      proc_ptr ptr = zap_ptr->who_zapped;
      zap_ptr->who_zapped = Current;
      zap_ptr->who_zapped->next_who_zapped = ptr;
   }

   dispatcher();

   // Check if the process was zapped during its execution.
   if(is_zapped())
      // If the process was zapped, set the result to -1.
      result = -1;

   return result;
}

int is_zapped() 
{
    return Current->zapped;
}

int getpid()
{
   return Current->pid;
}

void check_kernel_mode()
{
   if((PSR_CURRENT_MODE && psr_get()) == 0 )
   {
      console("fork1(): called while in user mode, by process %d", Current->pid);
      halt(1);
   }
}

/* ------------------------------------------------------------------------
   Name - find_proc_slot
   Purpose - Finds an empty slot in the process table.
   Parameters - none
   Returns  If it has searched the entire process table without 
   finding an empty slot, it returns -1 to indicate that no empty slot was found.
   If an empty slot is found, that index is returned
   Side Effects -  None
   ----------------------------------------------------------------------- */
int find_proc_slot()
{
   // calculate the starting index 
   int proc_slot = next_pid%MAXPROC;
   // initialize the count variable
   int i = 0;

   // search the process table statuses. If not empty increment next_pid and recalculate the index
   while(ProcTable[proc_slot].status != STATUS_EMPTY)
   {
      next_pid++;
      // recalculate the proc_slot index
      proc_slot = next_pid%MAXPROC;
      // If we have searched the process list and have not found an empty slot
      if(i >= MAXPROC)
      {
         return -1;
      }
      // increment the counter
      i++;
   }
   return proc_slot;
}

/* ------------------------------------------------------------------------
   Name - add_proc_to_readylist
   Purpose - Adds a new process to the ReadyList based on its priority .
   Parameters - proc_ptr proc - the process to be added to the ReadyList
   Returns - None
   Side Effects -  proc is added to the ReadyList in a correct position
   based on its priority 
   ----------------------------------------------------------------------- */
void add_proc_to_readylist(proc_ptr proc)
{
   if (DEBUG && debugflag)
   {
      console("add_proc_to_readylist(): Adding process %s to ReadyList\n", proc->name);
   }

   // If the ReadyList is NULL set as proc and its next_proc_ptr to NULL
   if (ReadyList == NULL)
   {
      ReadyList = proc;
      proc->next_proc_ptr = NULL;
   }

   // If new process has a higher priority than any of the existing in the readylist
   else if (ReadyList->priority > proc->priority)
   {
      proc->next_proc_ptr = ReadyList;
      ReadyList = proc;
   }
   else
   {
      // Traverse the readylist to find the correct position for the new process
      proc_ptr current = ReadyList;
      while (current->next_proc_ptr != NULL && current->next_proc_ptr->priority <= proc->priority)
      {
         current = current->next_proc_ptr;
      }
      // Insert the new process into the correct position
      proc->next_proc_ptr = current->next_proc_ptr;
      current->next_proc_ptr = proc;
   }

   if (DEBUG && debugflag)
   {
      console("add_proc_to_readylist(): Process %s added to ReadyList\n", proc->name);
   }
}
/*add_proc_to_readylist*/

/* ------------------------------------------------------------------------
   Name - remove_from_readylist
   Purpose - Removes a given process from the ReadyList and reassigns 
   remaining accordingly
   Parameters - proc_ptr the process to be removed from the ReadyList
   Returns - None
   Side Effects -  proc is removed from the ReadyList
   ----------------------------------------------------------------------- */
void remove_from_readylist(proc_ptr proc) {
    if (proc->status != STATUS_READY) {
        // process is not on the ready list, nothing to do
        return;
    }
    
    // find the previous process on the ready list, if any
    proc_ptr prev = NULL;
    proc_ptr curr = ReadyList;
    while (curr != NULL && curr != proc) {
        prev = curr;
        curr = curr->next_proc_ptr;
    }
    
    if (curr == NULL) {
        // process not found on the ready list, nothing to do
        return;
    }
    
    // remove the process from the ready list
    if (prev == NULL) {
        ReadyList = curr->next_proc_ptr;
    } else {
        prev->next_proc_ptr = curr->next_proc_ptr;
    }
    proc->next_proc_ptr = NULL;
    proc->status = STATUS_EMPTY;
}/*remove_from_readylist*/
