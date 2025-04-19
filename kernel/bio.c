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

#define NBUCKET 13
struct list{
  struct buf *head;
  struct spinlock lock;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct list bucs[NBUCKET];
  uint time;
} bcache;
#define num2buc(blockno) (&bcache.bucs[blockno % NBUCKET])
#define buf2buc(buf) num2buc(buf->blockno)


// add a buf to list l, caller must hold l->lock
void
ladd(struct list *l, struct buf *b) {
  b->prev = 0;
  b->next = l->head;
  if(l->head)
    l->head->prev = b;
  l->head = b;
}

// remove a buf b from l
// the lock for l must be held
void
lremove(struct buf *b) {
  struct list *l = buf2buc(b);

  if(b->next)
    b->next->prev = b->prev;
  if(b->prev)
    b->prev->next = b->next;
  else{
    if(b != l->head) panic("lremove");
    l->head = b->next;
  }
}

// update b->stamp
void
btouch(struct buf *b) {
  b->stamp = __sync_fetch_and_add(&bcache.time, 1);
}

// if the block is already cached, return it
// else return 0
struct buf*
bcheck(uint dev, uint blockno) {
  struct buf *b;
  struct list *l;
  l = num2buc(blockno);
  acquire(&l->lock);
  for(b = l->head; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      break;
    }
  }
  release(&l->lock);
  return b;
}

// find a unused buffer to be cached
// if no, find a sacrifice and evict it
// use LRU strategy
// caller must hold bcache.lock
struct buf*
bfind() {
  struct buf *b, *a;
  struct list *l;

  // find a buffer that never cached
  for(b = bcache.buf; b < bcache.buf+NBUF; b++)
    if(b->stamp == 0)
      return b;
  
  // evict one that has minimum stamp
  a = 0;
  for(l = bcache.bucs; l < bcache.bucs+NBUCKET; l++) {
    int first = 0;
    acquire(&l->lock);
    for(b = l->head; b; b = b->next) {
      if(b->refcnt) continue;
      if(!a || a->stamp > b->stamp) {
        if(!first && a) release(&buf2buc(a)->lock);
        a = b;
        first = 1;
      }
    }
    if(!first)
      release(&l->lock);
  }
  if(a) {
    lremove(a);
    release(&buf2buc(a)->lock);
  }
  return a;
}

// handle cache miss
// return a buffer for this block
struct buf*
bhandle(uint dev, uint blockno) {
  struct buf *b;
  struct list *l;

  acquire(&bcache.lock);
  // maybe the block is already cached by another bhandle()
  b = bcheck(dev, blockno);
  if(b) {
    release(&bcache.lock);
    return b;
  }

  // I'm the first bhandle() to handle this block's cache miss
  if(!(b = bfind())){
    release(&bcache.lock);
    return 0;
  }
  b->refcnt = 1;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  btouch(b);

  l = num2buc(blockno);
  acquire(&l->lock);
  ladd(l, b);
  release(&l->lock);

  release(&bcache.lock);
  return b;
}

void
binit(void)
{
  struct buf *b;
  struct list *l;

  initlock(&bcache.lock, "bcache");
  bcache.time = 1;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->stamp = 0;
  }
  for(l = bcache.bucs; l < bcache.bucs+NBUCKET; l++){
    initlock(&l->lock, "bcache.bucs");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  if(!(b = bcheck(dev, blockno)))
    b = bhandle(dev, blockno);
  if(b){
    acquiresleep(&b->lock);
    return b;
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
void
brelse(struct buf *b)
{
  struct list *l;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  l = buf2buc(b);
  acquire(&l->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    btouch(b);
  }
  release(&l->lock);
}

void
bpin(struct buf *b) {
  acquire(&buf2buc(b)->lock);
  b->refcnt++;
  release(&buf2buc(b)->lock);
}

void
bunpin(struct buf *b) {
  acquire(&buf2buc(b)->lock);
  b->refcnt--;
  release(&buf2buc(b)->lock);
}


