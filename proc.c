#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

extern char getSharedCounter(int index);

void clearThread(struct thread * t);

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct {
  struct spinlock lock;
  struct kthread_mutex_t mutex[MAX_MUTEXES];
} mtable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 1;
int nextmid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

struct thread*
allocthread(struct proc * p)
{
  struct thread *t;
  char *sp;
  int found = 0;

  acquire(&p->lock);
  for(t = p->threads; found != 1 && t < &p->threads[NTHREAD]; t++)
  {
    if(t->state == TUNUSED)
    {
      found = 1;
      t--;
    }
    else if(t->state == TZOMBIE)
    {
      clearThread(t);
      t->state = TUNUSED;
      found = 1;
      t--;
    }
  }

  if(!found)
    return 0;

  t->tid = nexttid++;
  t->state = EMBRYO;
  t->parent = p;
  t->killed = 0;

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0){
    t->state = TUNUSED;
    return 0;
  }
  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;

  release(&p->lock);
  return t;
}



//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
// Must hold ptable.lock.
static struct proc*
allocproc(void)
{
  struct proc *p;
  struct thread *t;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == PUNUSED)
      goto found;
  return 0;

found:
  p->state = USED;
  p->pid = nextpid++;
  initlock(&p->lock, p->name);

  t = allocthread(p);

  if(t == 0)
  {
    p->state = PUNUSED;
    return 0;
  }
  p->threads[0] = *t;

  for(t = p->threads; t < &p->threads[NTHREAD]; t++)
    t->state = TUNUSED;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  struct thread *t;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);

  p = allocproc();
  t = p->threads;
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(t->tf, 0, sizeof(*t->tf));
  t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  t->tf->es = t->tf->ds;
  t->tf->ss = t->tf->ds;
  t->tf->eflags = FL_IF;
  t->tf->esp = PGSIZE;
  t->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  t->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  acquire(&proc->lock);
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0){
      release(&proc->lock);
      return -1;
    }
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0){
      release(&proc->lock);
      return -1;
    }
  }
  proc->sz = sz;
  switchuvm(proc);
  release(&proc->lock);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct thread *nt;

  acquire(&ptable.lock);

  // Allocate process.
  if((np = allocproc()) == 0){
    release(&ptable.lock);
    return -1;
  }
  nt = np->threads;

  // Copy process state from p.
  if((np->pgdir = cowcopyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(nt->kstack);
    nt->kstack = 0;
    np->state = PUNUSED;
    release(&ptable.lock);
    return -1;
  }

  np->sz = proc->sz;
  np->parent = proc;
  *nt->tf = *thread->tf;

  // Clear %eax so that fork returns 0 in the child.
  nt->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  nt->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// take care that only one thread will perform and the others will become invalid
void
killBrothers()
{
  char found;
  struct thread * t;
  acquire(&proc->lock);
  if(thread->killed == 1)
  {
    wakeup1(thread);
    thread->state = INVALID; // thread must INVALID itself! - else two cpu's can run on the same thread
    release(&proc->lock);
    acquire(&ptable.lock);
    sched();
  }
  else
    for(t = proc->threads; t < &proc->threads[NTHREAD]; t++)
      if(t->tid != thread->tid)
        t->killed = 1;

  release(&proc->lock);

  for(;;)
  {
    found = 0;
    acquire(&proc->lock);
    for(t = proc->threads; t < &proc->threads[NTHREAD]; t++)
      if(t->tid != thread->tid && t->state != INVALID && t->state != TUNUSED)
        found = 1;

    release(&proc->lock);
    if(found) // some thread does not know the process is in dying state, lets wait for him to recover
      yield();
    else      // all of the other threads are dead
      break;
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  // take care that only one thread will perform
  killBrothers();

  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  thread->state = INVALID;
  proc->state = ZOMBIE;

  sched();
  panic("zombie exit");
}

void
clearThread(struct thread * t)
{
  if(t->state == INVALID || t->state == TZOMBIE)
    kfree(t->kstack);

  t->kstack = 0;
  t->tid = 0;
  t->state = TUNUSED;
  t->parent = 0;
  t->killed = 0;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct thread * t;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;

        for(t = p->threads; t < &p->threads[NTHREAD]; t++)
          clearThread(t);

        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = PUNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct thread *t;

  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != USED)
          continue;

      for(t = p->threads; t < &p->threads[NTHREAD]; t++){
        if(t->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.


        proc = p;
        thread = t;
        switchuvm(p);
        t->state = RUNNING;
        swtch(&cpu->scheduler, t->context);
        switchkvm();


        // Process is done running for now.
        // It should have changed its p->state before coming back.
        proc = 0;
        if(p->state != USED)
          t = &p->threads[NTHREAD];
        
        thread = 0;
      }

    }
    release(&ptable.lock);

  }
}

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
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(holding(&proc->lock))
    panic("sched proc->lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(thread->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");

  intena = cpu->intena;
  swtch(&thread->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  thread->state = RUNNABLE;
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
  if(proc == 0 || thread == 0)
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
    acquire(&ptable.lock);  //DOC: 4lock1
    release(lk);
  }

  
  // Go to sleep.
  acquire(&proc->lock);
  thread->chan = chan;
  thread->state = SLEEPING;
  release(&proc->lock);
  sched();

  // Tidy up.
  thread->chan = 0;

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
  struct thread *t;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == USED)
    {
      acquire(&p->lock);
      for(t = p->threads; t < &p->threads[NTHREAD]; t++)
        if(t->state == SLEEPING && t->chan == chan)
          t->state = RUNNABLE;
      release(&p->lock);
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
  struct thread *t;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      for(t = p->threads; t < &p->threads[NTHREAD]; t++)
        if(t->state == SLEEPING)
          t->state = RUNNABLE;

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Kill the threads with of given process with pid.
// Thread won't exit until it returns
// to user space (see trap in trap.c).
void
killSelf()
{
  acquire(&ptable.lock);
  wakeup1(thread);
  thread->state = INVALID; // thread must INVALID itself! - else two cpu's can run on the same thread
  sched();
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [PUNUSED]    "punused",
  [USED]    "used",
  [ZOMBIE]    "zombie"
  };
  // static char *threadStates[] = {
  // [TUNUSED]    "tunused",
  // [EMBRYO]    "embryo",
  // [SLEEPING]  "sleeping",
  // [RUNNABLE]  "runnable",
  // [RUNNING]  "running",
  // [TZOMBIE]  "tzombie",
  // [INVALID]    "invalid",
  // };
  int i;
  struct proc *p;
  struct thread *t;
  char *state;//, *threadState;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == PUNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    cprintf("%d %s %s\n", p->pid, state, p->name);
    for(t = p->threads; t < &p->threads[NTHREAD]; t++){
      // if(t->state >= 0 && t->state < NELEM(threadStates) && threadStates[p->state])
      //   threadState = threadStates[t->state];
      // else
      //   threadState = "???";

      if(t->state == SLEEPING){
        getcallerpcs((uint*)t->context->ebp+2, pc);
        for(i=0; i<10 && pc[i] != 0; i++)
          cprintf("%p ", pc[i]);
        cprintf("\n");
      }
    }

    cprintf("Page mappings:\n");
    pte_t *pte, *pgtab;
    int vpn; // virtual page number

    for(vpn = 0; vpn < NPDENTRIES*NPTENTRIES; vpn++)
    {
      void *va = (void *) (vpn << PTXSHIFT);
      //                        must use it as virtual address.
      pde_t *pde = &p->pgdir[PDX(va)];
      if(*pde & PTE_P && *pde & PTE_U){
          pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); // the page table
          pte = &pgtab[PTX(va)];               // the page table entry

          if(*pte & PTE_P && *pte & PTE_U)
            cprintf("%d -> %d, %s\n", vpn, *pte >> PTXSHIFT, ((*pte & PTE_W) || ((*pte & PTE_WS) && !getSharedCounter(*pte >> PTXSHIFT))) ? "y" : "n");
      }
    }
  }
}

int 
kthread_create(void* (*start_func)(), void* stack, int stack_size)
{
  struct thread * t;
  t = allocthread(proc);

  if(t == 0)
    return -1;

  *t->tf = *thread->tf;
  t->tf->eip = (int) start_func; // ?? should be on esp waiting for ret?
  t->tf->esp = (int) stack + stack_size;
  t->tf->ebp = t->tf->esp;

  t->state = RUNNABLE;
  return t->tid;
}

int 
kthread_id()
{
  if(proc && thread)
    return thread->tid;

  return -1;
}

void 
kthread_exit()
{
  struct thread * t;
  int found = 0;

  acquire(&proc->lock);
  for(t = proc->threads; t < &proc->threads[NTHREAD]; t++)
    if( t->tid != thread->tid && (t->state == EMBRYO || t->state == RUNNABLE || t->state == RUNNING || t->state == SLEEPING))
      found = 1;

  if(!found)
  {
    release(&proc->lock);
    wakeup(t);
    exit();
  }
  release(&proc->lock);

  acquire(&ptable.lock);
  wakeup1(thread);
  thread->state = TZOMBIE;
  
  sched();
}

int 
kthread_join(int thread_id)
{
  struct thread * t;
  if(thread_id == thread->tid)
    return -1;

  for(t = proc->threads; t < &proc->threads[NTHREAD] && t->tid != thread_id; t++);

  acquire(&ptable.lock);
  // found the one
  while(t->tid == thread_id && (t->state == EMBRYO || t->state == RUNNABLE || t->state == RUNNING || t->state == SLEEPING))
    sleep(t, &ptable.lock);

  release(&ptable.lock);

  if(t->state == TZOMBIE)
    clearThread(t);

  return 0;
}

int 
kthread_mutex_alloc()
{
  struct kthread_mutex_t *m;
  
  acquire(&mtable.lock);
  for(m = mtable.mutex; m->state != M_UNUSED && m < &mtable.mutex[NPROC]; m++);

  if(m == &mtable.mutex[NPROC])
  {
    release(&mtable.lock);
    return -1;
  }

  m->mid = nextmid++;
  m->state = M_UNLOCKED;

  release(&mtable.lock);
  return m->mid;
}

int 
kthread_mutex_dealloc(int mutex_id)
{
  struct kthread_mutex_t *m;

  acquire(&mtable.lock);
  for(m = mtable.mutex; m->mid != mutex_id && m < &mtable.mutex[NPROC]; m++);

  if(m == &mtable.mutex[NPROC] || m->state == M_LOCKED)
  {
    release(&mtable.lock);
    return -1;
  }

  m->mid = 0;
  m->state = M_UNUSED;

  wakeup1(m);

  release(&mtable.lock);
  return 0;
}

int 
kthread_mutex_lock(int mutex_id)
{
  struct kthread_mutex_t *m;
  
  acquire(&mtable.lock);
  for(m = mtable.mutex; m->mid != mutex_id && m < &mtable.mutex[NPROC]; m++);

  if(m == &mtable.mutex[NPROC] || m->state == M_UNUSED)
  {
    release(&mtable.lock);
    return -1;
  }

  while(m->state == M_LOCKED)
    sleep(m, &mtable.lock);

  if(m->state != M_UNLOCKED) // may happen if another process thinks about change the state to UNUSED while waiting here.
  {
    release(&mtable.lock);
    return -1; 
  }

  m->state = M_LOCKED;

  release(&mtable.lock);

  return 0;
}

int 
kthread_mutex_unlock(int mutex_id)
{
  struct kthread_mutex_t *m;
  
  acquire(&mtable.lock);
  for(m = mtable.mutex; m->mid != mutex_id && m < &mtable.mutex[NPROC]; m++);

  if(m == &mtable.mutex[NPROC] || m->state != M_LOCKED)
  {
    release(&mtable.lock);
    return -1;
  }

  m->state = M_UNLOCKED;
  wakeup1(m);

  release(&mtable.lock);
  return 0;
}