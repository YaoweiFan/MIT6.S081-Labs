// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit(int id)
{
  uint64 start;
  uint64 over;
  start = (uint64)end + id * (PHYSTOP - (uint64)end)/NCPU;
  over = (uint64)end + (id+1) * (PHYSTOP-(uint64)end)/NCPU;
  if (id == 2)
    over = PHYSTOP;
  initlock(&kmem[id].lock, "kmem");
  freerange((void*)start, (void*)over);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  int cpu_id = cpuid();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  
  int cpu_id = cpuid();
  struct run *r;

  acquire(&kmem[cpu_id].lock);// 其它 CPU 在调整剩余内存分配的时候，不能提取内存
  r = kmem[cpu_id].freelist;
  if(r){
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
    pop_off();
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  } else {
    release(&kmem[cpu_id].lock);

/**************************************************************/
    // 依次获得所有 cpu 的 kmem 锁
    for(int i=0; i<NCPU ;i++)
      acquire(&kmem[i].lock);

    // 开始调整内存分配
    uint64 total_remain_pages = 0;
    uint64 remain_pages[NCPU] = {0};
    uint64 remain_pages_average;
    // 统计剩余内存
    for(int i=0; i<NCPU; i++){
      struct run *r = kmem[i].freelist;
      while(r){
        total_remain_pages ++;
        remain_pages[i] ++;
        r = r->next;
      }
    }
    // 如果剩余内存为零，就返回零
    if (total_remain_pages == 0){
      // 依次释放所有 cpu 的 kmem 锁
      for(int i=NCPU-1; i>=0; i--)
        release(&kmem[i].lock);
      pop_off();
      return (void*)0;
    }
    // 求出多余的内存平均到每个 CPU 上应该分配的数量
    remain_pages_average = total_remain_pages/NCPU + 1;

    // 搜集每个 CPU 超出平均值的那部分内存
    struct run *remainlist = 0;
    for(int i=0; i<NCPU; i++){
      struct run *r;
      while(remain_pages[i] >= remain_pages_average){
        r = kmem[i].freelist;
        kmem[i].freelist = r->next; // 让出内存 r
        r->next = remainlist;
        remainlist = r; // 占有内存 r
        remain_pages[i] -= 1;
      }
    }

    // 把搜集到的内存再分配
    for(int i=cpu_id; i<NCPU+cpu_id; i++){
      struct run *r;
      while(remain_pages[i%NCPU] < remain_pages_average){
        r = remainlist;
        if(r == 0)
          break;
        remainlist = r->next; // 让出内存
        r->next = kmem[i%NCPU].freelist;
        kmem[i%NCPU].freelist = r; //占有内存
        remain_pages[i%NCPU] += 1;
      }
    }

    // 再次申请同一 CPU 的内存
    // acquire(&kmem[cpu_id].lock);
    r = kmem[cpu_id].freelist;
    if(r)
      kmem[cpu_id].freelist = r->next;
    // release(&kmem[cpu_id].lock);

    // 依次释放所有 cpu 的 kmem 锁
    for(int i=NCPU-1; i>=0; i--)
      release(&kmem[i].lock);
/**************************************************************/

    pop_off();

    if(r)
      memset((char*)r, 5, PGSIZE); // fill with junk 

    return (void*)r;
  }
}
