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

#define NBUFMAP_BUCKET 13
// 哈希索引
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];
  struct spinlock pop_lock;
  struct buf buf[NBUF];
  struct buf* bufmap[NBUFMAP_BUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;
char* bufmap_names[]=
{
  "bufmap_0",
  "bufmap_1",
  "bufmap_2",
  "bufmap_3",
  "bufmap_4",
  "bufmap_5",
  "bufmap_6",
  "bufmap_7",
  "bufmap_8",
  "bufmap_9",
  "bufmap_10",
  "bufmap_11",
  "bufmap_12"
};
void
binit(void)
{
  initlock(&bcache.pop_lock,"pop_lock");
  for(int i=0;i<NBUFMAP_BUCKET;i++)
  {
    bcache.bufmap[i]=0;
    initlock(&bcache.bufmap_locks[i],bufmap_names[i]);
  }
  for(int i=0;i<NBUF;i++)
  {
    bcache.buf[i].refcnt=0;

    bcache.buf[i].next=bcache.bufmap[0];
    bcache.buf[i].prev=0;
    if(bcache.bufmap[0])
      (bcache.bufmap[0])->prev=&(bcache.buf[i]);
    bcache.bufmap[0]=&(bcache.buf[i]);
  }
  //printf("binit\n");

  /*
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
  */
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int key;
  
  key=BUFMAP_HASH(dev,blockno);

  acquire(&(bcache.bufmap_locks[key]));
  //printf("1\n");
  for(b=bcache.bufmap[key];b;b=b->next)
  {
    if(b->blockno==blockno&&b->dev==dev&&b->refcnt)
    {
      b->refcnt++;
      release(&(bcache.bufmap_locks[key]));
      acquiresleep(&b->lock);
      //printf("2\n");
      return b;
    }
  }
  //printf("!\n");
  for(b=bcache.bufmap[key];b;b=b->next)
  {
    if(b->refcnt == 0 )
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&(bcache.bufmap_locks[key]));
      acquiresleep(&b->lock);
      //printf("3\n");
      return b;
    }
  }
  //printf("!\n");
  release(&bcache.bufmap_locks[key]);
  acquire(&bcache.pop_lock);

  for(b=bcache.bufmap[key];b;b=b->next)
  {
    if(b->blockno==blockno&&b->dev==dev&&b->refcnt)
    {
      acquire(&bcache.bufmap_locks[key]);
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.pop_lock);
      acquiresleep(&b->lock);
      //printf("4\n");
      return b;
    }
  }
  //printf("!\n");

  for(int i=0;i<NBUFMAP_BUCKET;i++)
  {
    if(i==key)
      continue;
    acquire(&bcache.bufmap_locks[i]);
    for(b=bcache.bufmap[i];b;b=b->next)
    {
      if(b->refcnt==0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        if(b->prev)
          b->prev->next=b->next;
        else
          bcache.bufmap[i]=b->next;
        if(b->next)
          b->next->prev=b->prev;
        

        release(&bcache.bufmap_locks[i]);
        acquire(&bcache.bufmap_locks[key]);

        b->next=bcache.bufmap[key];
        b->prev=0;
        if(bcache.bufmap[key])
          (bcache.bufmap[key])->prev=b;
        bcache.bufmap[key]=b;
        release(&bcache.bufmap_locks[key]);
        release(&bcache.pop_lock);
        acquiresleep(&b->lock);
        //printf("5\n");
        return b;
      }
    }
    release(&bcache.bufmap_locks[i]);
  }
  panic("bget: no buffers");
/*
  struct buf *b;
  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
  */
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
  int key;

  key=BUFMAP_HASH(b->dev,b->blockno);

  releasesleep(&b->lock);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  /*
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  int key=BUFMAP_HASH(b->dev,b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  int key=BUFMAP_HASH(b->dev,b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


