// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define STEAL_SIZE 64

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

char* kmem_name[]=
{
  "kmem_0",
  "kmem_1",
  "kmem_2",
  "kmem_3",
  "kmem_4",
  "kmem_5",
  "kmem_6",
  "kmem_7"
};
void init_kmems()
{
  for(int i=0;i<NCPU;i++)
  {
    kmem[i].freelist=0;
    initlock(&kmem[i].lock,kmem_name[i]);
  }
}
void
kinit()
{
  init_kmems();
  //initlock(&kmem.lock, "kmem");
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
  int cpu_id;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  cpu_id=cpuid();

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
  struct run *r;
  int cpu_id;

  push_off();
  cpu_id=cpuid();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  else
  {
    int j=0;
    for(int i=0;i<NCPU;i++)
    {
      if(i==cpu_id||j==STEAL_SIZE)
        continue;
      //printf("waiting for %d lock\n",i);
      acquire(&kmem[i].lock);
      for(;j<STEAL_SIZE;j++)
      {
        struct run* ans=kmem[i].freelist;
        if(ans)
          kmem[i].freelist=ans->next;
        else
          break;
        ans->next=kmem[cpu_id].freelist;
        kmem[cpu_id].freelist=ans;
      }
      release(&kmem[i].lock);
      //printf("release lock for %d \n",i);
    }
    r=kmem[cpu_id].freelist;
    if(r)
      kmem[cpu_id].freelist=r->next;
  }
  release(&kmem[cpu_id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
