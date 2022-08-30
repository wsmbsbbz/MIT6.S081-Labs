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
  struct buf buf[NBUF];
  struct bufbucket buckets[NBUCKET];
} bcache;

void
binit(void)
{
  printf("start binit\n");
  struct bufbucket *buck;
  struct buf *b;

  // Initialize each buckets.
  for (buck = bcache.buckets; buck < bcache.buckets+NBUCKET; buck++) {
    initlock(&buck->lock, "bcache");
    buck->head.next = &buck->head;
    buck->head.prev = &buck->head;
  }

  // Initialize linked list of buffers.
  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    // Firstly push all the buf on the buckets[0].
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// TODO: My code:
static struct buf*
bget(uint dev, uint blockno) {
  struct buf* lrub, *b;
  int hashOffset = BUCKID(blockno);
  struct bufbucket *buck = &bcache.buckets[hashOffset];
  struct bufbucket *curbuck;

  // Is the block already cached?
  acquire(&bcache.buckets[hashOffset].lock);
  for(lrub = buck->head.next; lrub != &buck->head; lrub = lrub->next) {
    if(lrub->dev == dev && lrub->blockno == blockno) {
      lrub->refcnt++;

      acquire(&tickslock);
      lrub->timestamp = ticks;
      release(&tickslock);

      release(&buck->lock);
      acquiresleep(&lrub->lock);
      return lrub;
    }
  }

  // Not cached.
  lrub = 0;

  // Recycle the least recently used (LRU) unused buffer.
  for(int id = hashOffset;; id = (id + 1) % NBUCKET) {
    curbuck = &bcache.buckets[id];
    if (id != hashOffset && holding(&curbuck->lock)) {
      continue;
    }
    if (id != hashOffset) {
      acquire(&curbuck->lock);
    }

    for(b = curbuck->head.next; b != &curbuck->head; b = b->next){
      if(b->refcnt == 0 && (lrub == 0 || b->timestamp < lrub->timestamp)) {
        lrub = b;
      }
    }

    if(lrub) {
      if(id != hashOffset) {
        lrub->next->prev = lrub->prev;
        lrub->prev->next = lrub->next;
        release(&curbuck->lock);

        lrub->next = buck->head.next;
        lrub->prev = &buck->head;
        buck->head.next->prev = lrub;
        buck->head.next = lrub;
      }

      lrub->dev = dev;
      lrub->blockno = blockno;
      lrub->valid = 0;
      lrub->refcnt = 1;

      acquire(&tickslock);
      lrub->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[hashOffset].lock);
      acquiresleep(&lrub->lock);
      return lrub;
    } else if(id != hashOffset) {
      release(&curbuck->lock);
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

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int hashOffset = BUCKID(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.buckets[hashOffset].lock);
  b->refcnt--;

  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  release(&bcache.buckets[hashOffset].lock);
}

void
bpin(struct buf *b) {
  int hashOffset = BUCKID(b->blockno);
  acquire(&bcache.buckets[hashOffset].lock);
  b->refcnt++;
  release(&bcache.buckets[hashOffset].lock);
}

void
bunpin(struct buf *b) {
  int hashOffset = BUCKID(b->blockno);
  acquire(&bcache.buckets[hashOffset].lock);
  b->refcnt--;
  release(&bcache.buckets[hashOffset].lock);
}


