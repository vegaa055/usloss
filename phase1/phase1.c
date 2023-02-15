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
int zap(int pid);
int is_zapped();
int getpid();
static int get_next_pid();
int find_proc_slot();
void add_proc_to_readylist(proc_ptr proc);
void remove_from_readylist(proc_ptr proc);

/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lisadd_proc_to_readylistts  */
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

   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   if(DEBUG && debugflag)
      console("startup(): initializing process table - ProcTable[]\n");
   
   for (i = 0; i < MAXPROC; i++)
   {
      init_proc_table(i);
   }
   
   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");

   ReadyList = NULL;

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
   current_proc.zapped = -1;
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
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
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
      console("fork1(): process %s stack size too small\n");
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
   ProcTable[proc_slot].start_func = f;

   // set the proc args
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) 
   {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   ProcTable[proc_slot].stacksize = stacksize;
   if(ProcTable[proc_slot].stack == malloc(stacksize) == NULL)
   {
      console("malloc failure. Halting...\n");
      halt(1);
   }
   ProcTable[proc_slot].priority = priority;

   if(Current != NULL)
   {
      if(Current->child_proc_ptr == NULL)
      {
         Current->child_proc_ptr = &ProcTable[proc_slot];
      }
      else
      {
         proc_ptr child = Current->child_proc_ptr;

         while(child->next_sibling_ptr != NULL)
         {
            child = child->next_proc_ptr;
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

   /* Sentinel does not call dispatcher when initially created*/
   if(ProcTable[proc_slot].pid != SENTINELPID)
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

   proc_ptr parent;

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

int zap(int pid)
{
   int result = 0;
   proc_ptr zap_ptr;

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
   remove_from_readylist(Current);
   zap_ptr = &ProcTable[pid%MAXPROC];
   zap_ptr->zapped = 1;

   if(zap_ptr->who_zapped == NULL)
   {
      zap_ptr->who_zapped = Current;
   }
   else
   {
      proc_ptr ptr = zap_ptr->who_zapped;
      zap_ptr->who_zapped = Current;
      zap_ptr->who_zapped->next_who_zapped = ptr;
   }

   dispatcher();

   if(is_zapped)
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

int find_proc_slot()
{
   int proc_slot = -1;
   int i;
   int start_pid = next_pid;

   for(i = (start_pid%MAXPROC); i < MAXPROC; i++)
   {
      if(ProcTable[i].status == STATUS_EMPTY)
      {
         proc_slot = i;
         break;
      }
      else
      {
         next_pid++;
      }
   }

   if(proc_slot == -1)
   {
      for (i = 0; i < (start_pid%MAXPROC); i++)
      {
         if(ProcTable[i].status == STATUS_EMPTY)
         {
            proc_slot = i;
            break;
         }
         else
         {
            next_pid++;
         }
      }
   }
   return proc_slot;
}

void add_proc_to_readylist(proc_ptr proc)
{
   if(DEBUG && debugflag)
   {
      console("add_proc_to_readylist(): Adding process %s to ReadyList\n", proc->name);
   }

   if(ReadyList == NULL)
   {
      ReadyList = proc;
   }
   else
   {
      if(ReadyList->priority > proc->priority)
      {
         proc_ptr tempProc = ReadyList;
         ReadyList = proc;
         proc->next_proc_ptr = tempProc;
      }
      else
      {
         proc_ptr next = ReadyList->next_proc_ptr;
         proc_ptr last = ReadyList;
         while(next->priority <= proc->priority)
         {
            last = next;
            next = next->next_proc_ptr;
         }
      }
      if(DEBUG && debugflag)
      {
         console("add_proc_to_readylist(): Process %s added to ReadyList", proc->name);
      }
   }
}/*add_proc_to_readylist*/