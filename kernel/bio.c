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
#define BUCKID(blockid) (blockid % NBUCKET)

struct bufbucket {
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  struct spinlock lock;
};

struct {
  // This lock atomizes the operation on the freelist
  struct spinlock lock;
  struct buf freehead;
  struct buf freelist[NBUF];
  struct bufbucket buckets[NBUCKET];
} bcache;

// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

void
binit(void)
{
  struct bufbucket *buck;
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of free buffers
  bcache.freehead.next = &bcache.freehead;
  bcache.freehead.prev = &bcache.freehead;
  for (b = bcache.freelist; b < bcache.freelist+NBUF; b++) {
    b->next = bcache.freehead.next;
    b->prev = &bcache.freehead;
    initsleeplock(&b->lock, "buffer");
    bcache.freehead.next->prev = b;
    bcache.freehead.next = b;
  }

  // Initial each head of buckets
  int i = 0;
  for (buck = bcache.buckets; buck < bcache.buckets+NBUCKET; buck++) {
    buck->head.next = &buck->head;
    buck->head.prev = &buck->head;
    i++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = BUCKID(blockno);
  struct bufbucket *buck = &bcache.buckets[id];

  // Is the block already cached?
  // In this case, doesn't need to acquire bcache.lock.
  acquire(&buck->lock);
  for (b = buck->head.next; b != &buck->head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buck->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&buck->lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  // BUG: If freelist is empty, will loop indefinitely.
  for(b = bcache.freehead.prev; b != &bcache.freehead; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      // Move newly created buf to the corresponding hash bucket.
      bcache.freehead.prev->prev->next = &bcache.freehead;
      bcache.freehead.prev = bcache.freehead.prev->prev;
      b->next = buck->head.next;
      b->prev = &buck->head;
      acquire(&buck->lock);
      buck->head.next->prev = b;
      buck->head.next = b;
      // Release the two locks
      release(&buck->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
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
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
//   
//   release(&bcache.lock);
// }

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // Move the buf from the hash bucket to the freelist.
    b->next = bcache.freehead.next;
    b->prev = &bcache.freehead;
    acquire(&bcache.lock);
    bcache.freehead.next->prev = b;
    bcache.freehead.next = b;
    release(&bcache.lock);
  }
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


