#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  uint lowest_p;
} ptable;

struct node
{
	int pid;
	int qno;
	int used;
	struct node* next;
	struct node* prev;
};
struct node qparts[NPROC];
struct node *qhead[5];
int mwtime[5]={5,4,3,2,1};
static struct proc *initproc;
void push(int qno, int pid)
{
	struct node *hea=qhead[qno];
	struct node *xo=&qparts[pid];
	if(xo==0)
		return;
	xo->used=1;
	xo->pid=pid;
	xo->next=0;
	xo->qno=qno;
	while(hea && hea->next!=0)
		hea=hea->next;
	if(hea)
	hea->next=xo;
	xo->prev=hea;
	if(!hea)
		qhead[qno]=xo;
}

int pop(int qno)
{
	if(!qhead[qno])
		return -1;
	qhead[qno]->used=0;
	int ao=qhead[qno]->pid;
	//qhead[qno]->qno=-1;
	qhead[qno]=qhead[qno]->next;
	if(qhead[qno])
	qhead[qno]->prev=0;
	return ao;
}
int moveahead(int pid)
{	
	struct node *xo=&qparts[pid];
	if(xo->qno<0)
		return 0;
	ptable.proc[pid].qwtime[xo->qno]++;
	if(xo->qno==0)
		return 0;
	if(ptable.proc[pid].cwtime<(uint)mwtime[xo->qno])
		return 0;
	ptable.proc[pid].cwtime=0;
	if(xo->prev)
		xo->prev->next=xo->next;
	else
		qhead[xo->qno]=xo->next;
	if(xo->next)
		xo->next->prev=xo->prev;
	push(xo->qno-1, pid);
	return 1;
}

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  for(int i=0;i<5;i++)
	  qhead[i]=0;
  for(int i=0;i<NPROC;i++)
	  qparts[i].qno=-1,qparts[i].used=0,ptable.proc[i].surr=1;
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int i=0;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++,i++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->spid = i;
  p->priority = 60;
  if(ptable.lowest_p>p->priority)
	  ptable.lowest_p = p->priority;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  acquire(&tickslock);
  p->ctime=ticks;
  release(&tickslock);
  p->rtime=0;
  // Appropriate value put in scheduler function
#ifdef MLFQ
#endif
  p->timeslice=1;
  p->ded=0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
#ifdef MLFQ
  cprintf("LSW %d\n", p->spid);
  push(0, p->spid);
#endif

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

int set_priority(int new_priority, int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
	  if(p->pid==pid)
		  break;
  }
  if(p->pid!=pid || new_priority>100 || new_priority<0)
  {
      release(&ptable.lock);
	  return -1;
  }
  int old=p->priority;
  if(ptable.lowest_p>(uint)new_priority)
	  ptable.lowest_p = new_priority;
  p->priority = new_priority;
  release(&ptable.lock);
  return old;
}
int lowest_priority()
{
	return ptable.lowest_p;
}
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  acquire(&ptable.lock);

  np->state = RUNNABLE;
#ifdef MLFQ
  push(0, np->spid);
#endif

  release(&ptable.lock);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  acquire(&tickslock);
  curproc->etime = ticks;
  release(&tickslock);
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
int update_rtime()
{	
	struct proc *p;
	int i=0;
	int cnt=0;
	acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++, i++)
	{ 
		if(p->state == RUNNING)
		{
			cnt++;
			p->rtime++;	
		}
		if(p->state == RUNNABLE)
		{
			cnt++;
			p->cwtime++;
			p->wtime++;
			moveahead(i);
		}
		if(p->state == SLEEPING)
			cnt++;
	}
	release(&ptable.lock);
	return cnt;
}
int
waitx(int* wtime, int* rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
		acquire(&tickslock);
		*rtime = curproc->rtime;
		*wtime = (ticks-*rtime) - curproc->ctime;
		release(&tickslock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
		acquire(&tickslock);
	  *rtime = curproc->rtime;
	  *wtime = ticks-(*rtime + curproc->ctime);
		release(&tickslock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
void
pscall()
{
	struct proc* p;
  cprintf("PID Priority State r_time w_time n_run cur_q  q0  q1   q2   q3   q4 \n");
  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++)
  {
	  if(p->state==UNUSED || p->state==EMBRYO)
		  continue;
	  cprintf("%d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d \t %d\n", p->pid, p->priority, p->state, p->rtime, p->cwtime, p->n_run, qparts[p->spid].qno,p->qwtime[0],p->qwtime[1],p->qwtime[2],p->qwtime[3],p->qwtime[4]);
  }
   release(&ptable.lock);
   return;
}



//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef FCFS
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
	struct proc *p_run = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
	  if(p_run) {
		  if(p_run->ctime>p->ctime)
		  p_run = p;
	   }
	  else
		  p_run=p;
	}
	if(p_run)
	{
	  p = p_run;
	  // Set process runtime
	  p->timeslice = __INT_MAX__;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
	  p->cwtime = 0;
	  p->n_run++;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
	}
    release(&ptable.lock);
  }
}
#elif defined PBS
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
	ptable.lowest_p = 100;
	int cnt=0;
	struct proc *p_run = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
	  if(p_run) {
		  if(p_run->priority > p->priority)
		{
		  p_run = p;
		  cnt=0;
		}
		else if(p_run->priority==p->priority)
		{
			cnt++;
			if(p->rtime < p_run->rtime)
				p_run = p;
		}
	   }
	  else
		  p_run=p;
	}
	if(p_run)
	{
	  p = p_run;
	  // Set process runtime
	  // If multiple proccess with min priority then round-robin
	  // Else give full time
	  if(cnt)
		  p->timeslice = 1;
	  else
	  p->timeslice = __INT_MAX__;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
	  p->cwtime = 0;
	  p->n_run++;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
	}
    release(&ptable.lock);
  }
}
#elif defined MLFQ
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    sti();
	/*
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->state == RUNNABLE && qparts[p->spid].qno==-1)
			push(0, p->spid);
	}
	*/

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
	struct proc *p_run = 0;
    for(int i=0;i<5;i++){
      if(qhead[i])
	  {
		  if(ptable.proc[qhead[i]->pid].state==RUNNABLE)
	  {
		  p_run = &ptable.proc[qhead[i]->pid];
		  pop(i);
		  p_run->timeslice = 1<<i;
		  break;
	  }
	  else
		  pop(i);
	}
	}
	if(p_run)
	{
	  p = p_run;
	  p->surr=1;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
	  p->cwtime = 0;
	  p->n_run++;
      swtch(&(c->scheduler), p->context);
      switchkvm();
	  //Decide which queue the process goes into
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
	}
    release(&ptable.lock);
  }
}




#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
	  p->timeslice = 1;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
	  p->cwtime = 0;
	  p->n_run++;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
#endif
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();
#ifdef MLFQ
  int qid = qparts[p->spid].qno;
  if(p->state==RUNNABLE && (p->surr || qid==4))
		  push(qid, p->spid);
	  else if(p->state==RUNNABLE)
		  push(qid+1,p->spid);
#endif

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {    if(p->state == SLEEPING && p->chan == chan)
	{p->state = RUNNABLE;
#ifdef MLFQ
  int qid = qparts[p->spid].qno;
  if(p->state==RUNNABLE && (p->surr || qid==4))
		  push(qid, p->spid);
	  else if(p->state==RUNNABLE)
		  push(qid+1,p->spid);
#endif
	}
}
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
	  {
		  p->state = RUNNABLE;
        #ifdef MLFQ
  int qid = qparts[p->spid].qno;
  if(p->state==RUNNABLE && (p->surr || qid==4))
		  push(qid, p->spid);
	  else if(p->state==RUNNABLE)
		  push(qid+1,p->spid);
#endif

	  }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
