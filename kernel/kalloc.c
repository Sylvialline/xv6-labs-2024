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

struct kmemalloc{
  struct spinlock lock;
  struct run *freelist;
  int freed;
} kmems[NCPU];

void
kinit()
{
  static char names[8][7] = {
    "kmem#0",
    "kmem#1",
    "kmem#2",
    "kmem#3",
    "kmem#4",
    "kmem#5",
    "kmem#6",
    "kmem#7",
  };
  for(int i = 0; i < NCPU; i++){
    initlock(&kmems[i].lock, names[i]);
  }
  freerange(end, (void*)PHYSTOP);
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
  struct kmemalloc *kmem;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  kmem = &kmems[cpuid()];
  acquire(&kmem->lock);
  kmem->freed = 1;
  r->next = kmem->freelist;
  kmem->freelist = r;
  release(&kmem->lock);
  pop_off();
}

// find a free page in kmems[cid]
void *
findfree(int cid) {
  struct run *r;
  struct kmemalloc *kmem;
  kmem = &kmems[cid];
  if(atomic_read4(&kmem->freed) == 0)
    return 0;
  acquire(&kmem->lock);
  r = kmem->freelist;
  if(r)
    kmem->freelist = r->next;
  release(&kmem->lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int i, cid;
  
  push_off();
  cid = cpuid();
  r = findfree(cid);
  pop_off();
  i = cid;
  while(!r) {
    i = (i+1) % NCPU;
    if(i == cid)
      break;
    r = findfree(i);
  }
  return (void*)r;
}
