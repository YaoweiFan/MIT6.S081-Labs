#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#define min(a, b) ((a) < (b) ? (a) : (b))

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

static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

uint64
sys_mmap(void)
{
  struct proc* p = myproc();
  uint64 addr;
  int length, prot, flags, offest;
  struct file *file;
  struct vma* vma = 0;
  
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0
      || argint(3, &flags) < 0 || argfd(4, 0, &file) < 0 || argint(5, &offest) < 0)
    return -1;

  if(file->writable == 0 && (prot & PROT_WRITE) && (flags & MAP_SHARED))
    return -1;

  // find a vma which is not used
  for(int i=0; i<16; i++){
      if(p->vma[i].addr == 0)
        vma = &p->vma[i];
  }
  if(vma == 0)
    panic("no vma for mmap!");

  // alloc virtual memory
  if(addr == 0)
    // if p->sz % PGSIZE != 0, the page p->sz located was allocated.
    vma->addr = PGROUNDUP(p->sz);
  else
    panic("mmap's first parameter addr is not equal to 0!");

  p->sz = PGROUNDUP(vma->addr + length);
  vma->length = length;
  vma->prot = prot;
  vma->flags = flags;
  vma->file = file;
  filedup(vma->file);
  vma->offset = offest;

  return vma->addr;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  struct proc* p = myproc();
  struct vma* vma = 0;

  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  // find the vma which contains the addr
  for(int i=0; i<16; i++){
      if(addr >= p->vma[i].addr && addr < p->vma[i].addr + p->vma[i].length){
        vma = &p->vma[i];
        break;
      }
  }
  if(vma == 0)
    panic("no vma for munmap!");

  // assume addr % PGSIZE == 0
  if(vma->flags & MAP_SHARED){
    for(uint64 adds = 0; adds < length; adds += PGSIZE){
      if(check_dirty(p->pagetable, addr+adds))
        filewrite(vma->file, addr+adds, min(PGSIZE, length-adds));
    }
  }

  uvmunmap(p->pagetable, addr, PGROUNDUP(length)/PGSIZE, 1);

  if(addr == vma->addr && length == vma->length){
    fileclose(vma->file);
    vma->addr = 0;
  }
  else if(addr == vma->addr){
    vma->addr = vma->addr + PGROUNDUP(length);
    vma->length -= PGROUNDUP(length);
  }
  else
    vma->length = addr - vma->addr;

  return 0;
}