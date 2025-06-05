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
  struct run *freelist; // 空闲普通物理页链表
  struct run *super_freelist; // 空闲超级物理页链表
} kmem;

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
  // 留8个页给超级页
  for(; p + PGSIZE <= (char*)pa_end - 8 * SUPERPGSIZE; p += PGSIZE)
    kfree(p);

  p = (char*)SUPERPGROUNDUP((uint64)p);
  for(; p + SUPERPGSIZE <= (char*)pa_end; p += SUPERPGSIZE)
    super_kfree(p);
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

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// 分配超级页，大小为2MB
void* super_kalloc(void) {
  struct run* r;

  acquire(&kmem.lock); // 加锁
  r = kmem.super_freelist; // 取出第一个空闲的超级页

  if (r) {
    kmem.super_freelist = r->next;
  }
  release(&kmem.lock); // 解锁

  if (r) {
    memset((char*)r, 5, SUPERPGSIZE); // 填充超级页
  }
  return (void*)r;
}

// 释放超级页
void super_kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % SUPERPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    panic("super_kfree");
  }

  // Fill with junk to catch dangling refs.
  // 用无效数据填充以捕获悬空引用
  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock); // 加锁
  r->next = kmem.super_freelist; // 将释放的超级页插入到空闲链表的头部
  kmem.super_freelist = r; // 更新空闲链表的头部
  release(&kmem.lock); // 解锁
}