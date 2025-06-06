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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
  char lock_name[NCPU]; // 区分不同CPU的锁
} kmems[NCPU]; // 每个CPU都有一个kmem结构体


void
kinit() // 这个函数只有0号CPU会执行
{
   // 初始化每个CPU的锁
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmems[i].lock_name, sizeof(kmems[i].lock_name), "kmem_%d", i);
    initlock(&kmems[i].lock, kmems[i].lock_name);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu_id = cpuid();
  pop_off();

  acquire(&kmems[cpu_id].lock);
  // 新的空闲物理页插入到freelist头部
  r -> next = kmems[cpu_id].freelist; 
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu_id = cpuid();
  pop_off();

  acquire(&kmems[cpu_id].lock);

  // 从当前CPU的freelist中取出首个空闲物理页
  r = kmems[cpu_id].freelist;

  // 成功获取到空闲物理页
  if (r) {
    kmems[cpu_id].freelist = r->next;
    release(&kmems[cpu_id].lock);
  }
  // 从其他CPU的freelist中获取空闲物理页
  else {
    release(&kmems[cpu_id].lock);

    for(int i = 0; i < NCPU; i++){
      if(i != cpu_id) {
        acquire(&kmems[i].lock);
        r = kmems[i].freelist;

        // 没获取到其它cpu的物理页
        if(!r) {
          release(&kmems[i].lock);
          continue;
        }
        // 成功获取到其它cpu的物理页
        else {
          kmems[i].freelist = r->next;
          release(&kmems[i].lock);
          break;
        }
      }
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
