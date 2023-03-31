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

#define mo 13
#define ID(x) ((x) % mo)
struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct spinlock lock[mo];
  struct buf ha[mo];
} bcache;

void
binit(void)
{
  struct buf *b;
  // Create linked list of buffers
  for (int i = 0; i < mo; i++) {
    initlock(&bcache.lock[i], "bcache.bucket");
    bcache.ha[i].prev = &bcache.ha[i];
    bcache.ha[i].next = &bcache.ha[i];
  }
  int i = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    int id = ID(i);
    i++;
    b->blockno = i;
    b->refcnt = 0;
    b->next = bcache.ha[id].next;
    b->prev = &bcache.ha[id];
    bcache.ha[id].next->prev = b;
    bcache.ha[id].next = b;
  }
}

void trans(struct buf *b, struct buf *head) {
  b->prev->next = b->next;
  b->next->prev = b->prev;

  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // Is the block already cached?
  int id = ID(blockno);
  acquire(&bcache.lock[id]);
  for(b = bcache.ha[id].next; b != &bcache.ha[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.ha[id].next; b != &bcache.ha[id]; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // steal
  for (int i = 1; i < mo; i++) {
    int index = (i + id) % mo;
    acquire(&bcache.lock[index]);
    for(b = bcache.ha[index].next; b != &bcache.ha[index]; b = b->next) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 先从这个桶中移除
        trans(b, &bcache.ha[id]);
        release(&bcache.lock[id]);
        release(&bcache.lock[index]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[index]);
  }
  release(&bcache.lock[id]);
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
  // 只能释放自己拥有的块
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int id = ID(b->blockno);
  releasesleep(&b->lock);
  // 需要修改对应桶中的buf属性, 并移除
  acquire(&bcache.lock[id]);

  b->refcnt--; // 似乎应该在释放b->lock之前修改b的属性
  if (b->refcnt == 0) {
    // no one is waiting for it.
    ;
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = ID(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = ID(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


