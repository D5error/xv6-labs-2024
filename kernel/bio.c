// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13 // 哈希表大小
#define HASH(block_number) ((block_number) % NBUCKETS) // 哈希函数

struct bucket {
  struct spinlock lock; // 自旋锁
  struct buf *head; // 链表头
  char lock_name[20]; // 锁的名称
};

struct {
  struct bucket bucket[NBUCKETS]; // 缓冲区桶数组，每个桶包含一组缓冲区
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // 初始化每个桶的锁
  for (int i = 0; i < NBUCKETS; i++) {
    // 初始化锁名称
    snprintf(bcache.bucket[i].lock_name, 17, "bcache_bucket_%d", i);

    // 初始化锁
    initlock(&bcache.bucket[i].lock, bcache.bucket[i].lock_name);
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");

    // 添加到链表头部
    b->next = bcache.bucket[0].head;
    
    // 将所有buf分配到第一个桶bucket[0]中
    bcache.bucket[0].head = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *prev = 0;

  int key = HASH(blockno); // 获取哈希桶的索引
  acquire(&bcache.bucket[key].lock); // 上锁

  // Is the block already cached?
  for(b = bcache.bucket[key].head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[key].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[key].lock);  // 解锁

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (int offset = 0; offset < NBUCKETS; offset++) {
    int i = HASH(key + offset); // 获取哈希桶的索引
    acquire(&bcache.bucket[i].lock); // 上锁

    // 遍历桶中缓冲块链表
    for(b = bcache.bucket[i].head; b; b = b->next){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 如果该缓冲块不在目标桶中，需要移动到目标桶
        if(i != key){ 
          // 从当前桶的链表中移除该缓冲块
          if (b != bcache.bucket[i].head) {
            prev->next = b->next;
          }
          else{
            bcache.bucket[i].head = b->next;
          }
          release(&bcache.bucket[i].lock); // 解锁原桶

          // 加锁目标桶，将该缓冲块插入到目标桶头部
          acquire(&bcache.bucket[key].lock);
          b->next = bcache.bucket[key].head;
          bcache.bucket[key].head = b;
          release(&bcache.bucket[key].lock);
        }
        else{
          release(&bcache.bucket[i].lock);
        }

        acquiresleep(&b->lock);
        return b;
      }
      prev = b;
    }
    release(&bcache.bucket[i].lock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = HASH(b->blockno); // 获取哈希桶的索引
  acquire(&bcache.bucket[key].lock); // 上锁
  b->refcnt--;

  release(&bcache.bucket[key].lock); // 解锁
}

void
bpin(struct buf *b) {
  int i=HASH(b->blockno); // 获取哈希桶的索引

  acquire(&(bcache.bucket[i].lock)); // 上锁
  b->refcnt++;
  release(&(bcache.bucket[i].lock)); // 解锁
}

void
bunpin(struct buf *b) {
  int i = HASH(b->blockno); // 获取哈希桶的索引

  acquire(&(bcache.bucket[i].lock)); // 上锁
  b->refcnt--;
  release(&(bcache.bucket[i].lock)); // 解锁
}


