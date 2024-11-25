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

struct spinlock ref_lock;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int refcounts[(PHYSTOP - KERNBASE) / PGSIZE];

uint64 WHICHPG(uint64 pa) {
  return ((uint64)pa - KERNBASE) / PGSIZE;
} 

void ref_up(uint64 pa) {
    acquire(&ref_lock);
    refcounts[WHICHPG(pa)]++;
    release(&ref_lock);
}

void ref_down(uint64 pa) {
    acquire(&ref_lock);
    refcounts[WHICHPG(pa)]--;
    release(&ref_lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "refcounts");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  // printf("successfully freerange\n");
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
  
  acquire(&ref_lock);
  refcounts[WHICHPG((uint64) pa)]--;
  if (refcounts[WHICHPG((uint64) pa)] <= 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else {
    // printf("ref to mem %p = %d\n", (void *) pa, refcounts[WHICHPG((uint64) pa)]);
    // panic("have ref unfreed");
  }
  release(&ref_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // Set the reference count to 1
    acquire(&ref_lock);
    refcounts[WHICHPG((uint64)r)] = 1;
    release(&ref_lock);
  }
  return (void*)r;
}

uint64
cowallloc(uint64 pa) {
    acquire(&ref_lock);
    if (refcounts[WHICHPG(pa)] <= 1) {
        release(&ref_lock);
        return pa;
    }
    release(&ref_lock);
    uint64 new = (uint64) kalloc();
    
    acquire(&ref_lock);
    if (new == 0) {
        release(&ref_lock);
        panic("oom!");
        return 0;
    }   

    memmove((void *) new, (void *)pa, PGSIZE);
    refcounts[WHICHPG(pa)]--;
    release(&ref_lock);
    return new;
}