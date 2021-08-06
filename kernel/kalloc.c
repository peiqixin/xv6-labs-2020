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
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  struct run* freelist;
} kmems[NCPU];

void freerange_multi_cpu(void* pa_start, void* pa_end, int cpu);

void kinit_multi_cpu() {
  for (int i = 0; i < NCPU; ++i) {
    initlock(&kmems[i].lock, "kmem");
  }
  
  uint64 pa_start = PGROUNDUP((uint64)end);
  uint64 pa_end = PHYSTOP;
  // 每个CPU所占的物理页数
  uint64 each = (pa_end / PGSIZE - pa_start / PGSIZE) / NCPU;
  each = each * PGSIZE;
  
  for (int i = 0; i < NCPU - 1; ++i) {
    freerange_multi_cpu((void*)(pa_start + each * i), (void*)(pa_start + each * (i + 1)), i);
  }
  // 剩余的所有内存都分给最后一个CPU
  freerange_multi_cpu((void*)(pa_start + each * (NCPU - 1)), (void*)pa_end, NCPU - 1);
}

void freerange_multi_cpu(void* pa_start, void* pa_end, int cpu) {
  char* pa;
  pa = (char*)PGROUNDUP((uint64)pa_start);
  for (; pa + PGSIZE <= (char*)pa_end; pa += PGSIZE) {
    struct run *r;
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
      panic("kfree");
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmems[cpu].freelist;
    kmems[cpu].freelist = r;
  }
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  push_off();
  int cpu = cpuid();
  pop_off();
  
  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmems[cpu].lock);
  r->next = kmems[cpu].freelist;
  kmems[cpu].freelist = r;
  release(&kmems[cpu].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run* r = 0;
  
  push_off();
  int i = cpuid();
  pop_off();
  
  acquire(&kmems[i].lock);
  r = kmems[i].freelist;
  if (r) kmems[i].freelist = r->next;
  release(&kmems[i].lock);

  if (!r) {
    for (int j = NCPU - 1; j >= 0; --j) {
      if (j == i) continue;
      acquire(&kmems[j].lock);
      r = kmems[j].freelist;
      if (r) kmems[j].freelist = r->next;
      release(&kmems[j].lock);
      if (r) break;
    }
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
