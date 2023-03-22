#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
#define MAX_CHECK 64

uint64 pgaccess(uint64 base, int len, uint64 mask) {
  struct proc *p = myproc();
  uint64 bitmap[MAX_CHECK / 64] = {0};  // 分配位图空间并初始化为0
  for (int i = 0; i < len; i++) {
    pte_t *pte = walk(p->pagetable, base, 0);
    if ((*pte) & PTE_A) {
      bitmap[i / 64] |= 1ULL << (i & 63);
      *pte = (*pte) ^ PTE_A;
    }
    base += PGSIZE;
  }
  copyout(p->pagetable, mask, (char *)&bitmap, sizeof(bitmap));
  return 0;
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  int len;
  uint64 base, mask;
  argaddr(0, &base);
  argint(1, &len);
  if (len > MAX_CHECK) {
    return -1;
  }
  argaddr(2, &mask);
  // 检查addr开始的len页是否被访问过， 将结果放入mask
  return pgaccess(base, len, mask);
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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

// My Code
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  struct proc *p = myproc();
  p->mask |= mask;
  return 0;
}


#include "kernel/sysinfo.h"

uint64 sysinfo(uint64 addr) {
  struct proc *p = myproc();
  struct sysinfo info;
  // 空闲内存的数量, kernel/kalloc.c中实现
  info.freemem = GetFreeMem();
  // 进程的数量，kernel/proc.c中实现
  info.nproc = GetNproc();
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  argaddr(0, &addr);
  return sysinfo(addr);
}