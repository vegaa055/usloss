#define DEBUG 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   proc_ptr       parent_ptr;
   proc_ptr       quit_child_ptr;
   proc_ptr       next_sibling_quit;
   proc_ptr       who_zapped;
   proc_ptr       next_who_zapped;
   char           name[MAXNAME];       /* process's name */
   char           start_arg[MAXARG];   /* args passed to process */
   context        state;               /* current context for process */
   short          pid;                 /* process id */
   int            priority;
   int (* start_func) (char *);        /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;              /* READY, BLOCKED, QUIT, etc. */
   int            quitStatus;
   int            startTime;
   int            zapped;
   int            cpuStartTime;
   /* other fields as needed... */
};

struct psr_bits {
   unsigned int cur_mode:1;
   unsigned int cur_int_enable:1;
   unsigned int prev_mode:1;
   unsigned int prev_int_enable:1;
   unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY

/* Process Statuses */
#define STATUS_EMPTY -1
#define STATUS_READY 1
#define STATUS_RUNNING 2
#define STATUS_QUIT 4
#define STATUS_EMPTY 5
#define STATUS_BLOCKED 8
#define STATUS_JOIN_BLOCKED 9
#define STATUS_ZAP_BLOCKED 10

