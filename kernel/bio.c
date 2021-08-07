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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

#define BUCKET_SIZE 9
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf head;
} bcaches[BUCKET_SIZE];


void
binit_s(void)
{
  struct buf* b;

  for (int i = 0; i < BUCKET_SIZE; ++i) {
    initlock(&bcaches[i].lock, "bcache");
     // Create linked list of buffers
    bcaches[i].head.prev = &bcaches[i].head;
    bcaches[i].head.next = &bcaches[i].head;
    for(b = bcaches[i].buf; b < bcaches[i].buf+NBUF; b++){
      b->next = bcaches[i].head.next;
      b->prev = &bcaches[i].head;
      initsleeplock(&b->lock, "buffer");
      bcaches[i].head.next->prev = b;
      bcaches[i].head.next = b;
    } 
  }
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int i = blockno % BUCKET_SIZE;
  acquire(&bcaches[i].lock);
  // 最近经常使用的链表上查找是否已经被缓存
  for (b = bcaches[i].head.next; b != &bcaches[i].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcaches[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 查找最近最少使用的buf
  for (b = bcaches[i].head.prev; b != &bcaches[i].head; b = b->prev) {
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcaches[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // TODO 如果还没找到，去其他链表找空闲的buf
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
  int no = b->blockno;
  int i = no % BUCKET_SIZE;
  acquire(&bcaches[i].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // buf 重新连接到bcaches的头部
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcaches[i].head.next;
    b->prev = &bcaches[i].head;
    bcaches[i].head.next->prev = b;
    bcaches[i].head.next = b;
  }
  release(&bcaches[i].lock);
  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  // acquire(&bcache.lock);
  int no = b->blockno;
  int i = no % BUCKET_SIZE;
  acquire(&bcaches[i].lock);
  b->refcnt++;
  release(&bcaches[i].lock);
  // release(&bcache.lock);
  
}

void
bunpin(struct buf *b) {
  // acquire(&bcache.lock);
  int no = b->blockno;
  int i = no % BUCKET_SIZE;
  acquire(&bcaches[i].lock);
  b->refcnt--;
  release(&bcaches[i].lock);
  // release(&bcache.lock);
}


