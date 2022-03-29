#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  int checknum;
  uint64 firstva, mask;
  // "kbuf" will be copyout to "mask" later.
  uint64 kbuf = 0;
  pte_t *pte;
  pagetable_t pagetable;
  if (argaddr(0, &firstva) < 0) {
    return -1;
  }
  if (argint(1, &checknum) < 0) {
    return -1;
  }
  if (argaddr(2, &mask) < 0) {
    return -1;
  }
  // I don't know what the appropriate range for "checknum" is.
  if (checknum > 512)
    return -1;
  pagetable = myproc()->pagetable;
  for (int i = 0; i < checknum; i++) {
    if((pte = walk(pagetable, firstva, 0)) == 0) {
      return -1;
    }
    if (*pte & PTE_A) {
      // Be sure to clear PTE_A after checking if it is set. Otherwise, it
      // won't be possible to determine if the page was accessed since the
      // last time pgaccess() was called (i.e., the bit will be set forever).
      *pte = CLEAR_PTE_A(*pte);
      // set the bitmask
      kbuf |= 1 << i;
    }
    firstva += PGSIZE;
  }
  // don't forget to use "&" operator to get the address of kbuf
  if (copyout(pagetable, mask, (char *) &kbuf, sizeof(kbuf)) < 0) {
    return -1;
  }

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
