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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem.lock[i], "kmem");
    kmem.freelist[i] = 0;
  }
  // 最后还有余数, 都分配给cpu 0吧
  freerange((void*)end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int id = cpuid();
  acquire(&kmem.lock[id]);
  r->next = kmem.freelist[id];
  kmem.freelist[id] = r;
  release(&kmem.lock[id]);
  pop_off();
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int id = cpuid();
  acquire(&kmem.lock[id]);
  if (!kmem.freelist[id]) {
    // steal
    for (int j = 1; j < NCPU; j++) {
      int i = (j + id) % NCPU;
      acquire(&kmem.lock[i]);
      if (kmem.freelist[i]) {
        struct run *fast = kmem.freelist[i];
        struct run *slow = kmem.freelist[i];
        struct run *mid = kmem.freelist[i];
        while(fast && fast->next) {
          fast = fast->next->next;
          mid = slow;
          slow = slow->next;
        }
        kmem.freelist[id] = kmem.freelist[i];
        kmem.freelist[i] = mid->next;
        mid->next = 0;
      }
      release(&kmem.lock[i]);
      if (kmem.freelist[id]) {
        break;
      }
    }
  }
  struct run *r;
  // 获取链表头
  r = kmem.freelist[id];
  if(r)
    kmem.freelist[id] = r->next;
  release(&kmem.lock[id]);
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
