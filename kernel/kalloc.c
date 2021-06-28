// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct {
  struct spinlock lock;
  uint8* cow_array;
} cow;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 数组清零
  acquire(&cow.lock);
  cow.cow_array = (uint8*)end;
  memset(cow.cow_array, 0, 8*PGSIZE); 
  release(&cow.lock);

  // 空出8页用来记录freelist中每一页的共享页表数
  freerange(end + 0x8000, (void*)PHYSTOP);
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&cow.lock);
  if(cow.cow_array[PPA2COWARRAY((uint64)pa)] == 1 || cow.cow_array[PPA2COWARRAY((uint64)pa)] == 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);

    cow.cow_array[PPA2COWARRAY((uint64)pa)] = 0;
  }else{
    cow.cow_array[PPA2COWARRAY((uint64)pa)] -= 1;
  }
  release(&cow.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  acquire(&cow.lock);
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    // Set a page's reference count to one when kalloc() allocates it.
    // testr = (struct run *)0x123;
    cow.cow_array[PPA2COWARRAY((uint64)r)] = 1;
    // printf("%p\n", r);
  }
  release(&kmem.lock);
  release(&cow.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void *
newkalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    // Set a page's reference count to one when kalloc() allocates it.
    // testr = (struct run *)0x123;
    // acquire(&cow.lock);
    cow.cow_array[PPA2COWARRAY((uint64)r)] = 1;
    // release(&cow.lock);
    // printf("%p\n", r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
rci(uint64 ppa)
{
  acquire(&cow.lock);
  // Increment a page's reference count when fork causes a child to share the page.
  cow.cow_array[PPA2COWARRAY(ppa)] += 1;
  release(&cow.lock);
}

void
rcd(uint64 ppa)
{
  // acquire(&kmem.lock);
  // Decrement a page's count each time any process drops the page from its page table
  cow.cow_array[PPA2COWARRAY(ppa)] -= 1;
  // release(&kmem.lock);
}

uint8
r_rc(uint64 ppa)
{
  uint8 rc;
  // acquire(&kmem.lock);
  rc = cow.cow_array[PPA2COWARRAY(ppa)];
  // release(&kmem.lock);
  return rc;
}

// void
// print_cow_array(pagetable_t pagetable, uint64 vas, uint64 vae)
// {
//   uint64 va = PGROUNDDOWN(vas);
//   int num = 0;
//   printf("vas = %p, vae = %p\n", vas, vae);
//   printf("%p: ", va);
//   for(uint64 addr=va; addr<vae; addr+=PGSIZE){
//     num ++;
//     uint64 ppa = walkaddr(pagetable, addr);
//     printf("%d ", cow_array[PPA2COWARRAY(ppa)]);
//     if(num == 128){
//       printf("\n");
//       printf("%p: ", addr+PGSIZE);
//       num = 0;
//     }
//   }
// }

void
print_cow_array()
{
  int num = 0;
  for(uint64 i=0; i<0x8000; i++){
    num ++;
    // printf("%d ", cow_array[i]);
    if(num == 128){
      printf("\n");
      num = 0;
    }
  }
}