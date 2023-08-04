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

#define NUM_BUCKET 13

struct hashbuf{
  struct buf head;
  struct spinlock lock;
};

struct {
  struct buf buf[NBUF];
  struct hashbuf buckets[NUM_BUCKET];
}bcache;
#define GETHASH(id) ((id % NUM_BUCKET))

void
binit(void)
{
  char name[16];
  for(int i=0;i<NUM_BUCKET;++i){
    // First, init spinlock
    snprintf(name, sizeof(name), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock,name);

    bcache.buckets[i].head.prev=bcache.buckets[i].head.next=&bcache.buckets[i].head;

  }
  // Allocate the buffer list into buckets[0]
  for(struct buf *i=bcache.buf;i<bcache.buf + NBUF;++i){
    i->next=bcache.buckets[0].head.next;
    i->prev = &bcache.buckets[0].head;
    initsleeplock(&i->lock, "buffer");
    bcache.buckets[0].head.next->prev = i;
    bcache.buckets[0].head.next = i;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bufferid=GETHASH(blockno);
  acquire(&bcache.buckets[bufferid].lock);

  // Is the block already cached?
  for(b = bcache.buckets[bufferid].head.next; b != &bcache.buckets[bufferid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;

      // 记录使用时间戳
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[bufferid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  b=0;
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(int i=0;i<NUM_BUCKET;++i){
    // Allocate bucket lock
    int id=(i+bufferid)%NUM_BUCKET;
    if(i){
      if(holding(&bcache.buckets[id].lock)){
        continue;
      }
      else{
        acquire(&bcache.buckets[id].lock);
      }
    }
    for(struct buf* tmp=bcache.buckets[id].head.next;tmp!=&bcache.buckets[id].head;tmp=tmp->next){
      if(tmp->refcnt==0&&(b==0||tmp->timestamp<b->timestamp)){
        b=tmp;
      }
    }
    if(b){
      if(i){ // Other bucket whose id is not equal to bufferid.
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[id].lock);

        b->next = bcache.buckets[bufferid].head.next;
        b->prev = &bcache.buckets[bufferid].head;
        bcache.buckets[bufferid].head.next->prev = b;
        bcache.buckets[bufferid].head.next = b;
      }
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bufferid].lock); // REVIEW
      acquiresleep(&b->lock);
      return b;
    }
    else{
      if(i){
        release(&bcache.buckets[id].lock);
      }
    }
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

  int bufferid=GETHASH(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.buckets[bufferid].lock);
  b->refcnt--;

  // 更新时间戳
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  
  release(&bcache.buckets[bufferid].lock);
}

void
bpin(struct buf *b) {
  int bufferid=GETHASH(b->blockno);
  acquire(&bcache.buckets[bufferid].lock);
  b->refcnt++;
  release(&bcache.buckets[bufferid].lock);
}

void
bunpin(struct buf *b) {
  int bufferid=GETHASH(b->blockno);
  acquire(&bcache.buckets[bufferid].lock);
  b->refcnt--;
  release(&bcache.buckets[bufferid].lock);
}