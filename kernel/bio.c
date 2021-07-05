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
  // struct buf head;
  struct spinlock hashlock[13];
  struct buf *hashbuf[13];

} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i=0; i<10; i++){
    initlock(&bcache.hashlock[i], "bcache");
    bcache.hashbuf[i] = &bcache.buf[i*3];
    bcache.hashbuf[i]->next = &bcache.buf[i*3+1];
    bcache.hashbuf[i]->next->prev = bcache.hashbuf[i];
    bcache.hashbuf[i]->next->next = &bcache.buf[i*3+2];
    bcache.hashbuf[i]->next->next->prev = bcache.hashbuf[i]->next;
    bcache.hashbuf[i]->next->next->next = 0;
    bcache.hashbuf[i]->prev = 0;
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *currb = 0;
  // acquire(&bcache.lock);

  // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // acquire(&bcache.lock);
  acquire(&bcache.hashlock[blockno%13]);
  for(b = bcache.hashbuf[blockno%13]; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashlock[blockno%13]);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 若当前 bucket 中存在空白 buf， 直接使用它
  b = bcache.hashbuf[blockno%13];
  while(b != 0){
    if(b->refcnt == 0){
      if(b->blockno == 0){
        // 若当前 bucket 中存在空白 buf， 直接使用它
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.hashlock[blockno%13]);
        acquiresleep(&b->lock);
        return b;
      } else {
        // 若当前 bucket 中无空白 buf，这个 buf 要被拿去比较
        currb = b; 
      }
      break;
    }
    b = b->next;
  }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  uint currbufticks = 0;
  uint minbufticks = 0;
  int minindex = -1;
  int firstgetted = -1;
  struct buf *minb = 0;
  // printf("*********************\n");
  // printf("**%d\n", minindex);
  // 当两个进程同时需要找一个block的时候，这里会死锁
  // 在检视 buckets 、转移buf 的时候必须 hold bcache.lock
  // acquire(&bcache.lock);

  // 若当前 bucket 中不存在空白 buf，查询所有 buckets，得到一个空白 buf 或者是最近没有使用的 buf
  for(int i=0; i<13; i++){
    // 无论如何，当前 bucket 的锁不能提前被释放，一定要等到操作完毕
    if(i == blockno%13)
      continue;
    acquire(&bcache.hashlock[i]);
    // 找到bucket i 中，空白 buf 或者是最近没有使用的 buf
    b = bcache.hashbuf[i];
    while(b != 0){
      if(b->refcnt == 0){
        currbufticks = b->bufticks;
        break;
      }
      b = b->next;
    }
    // 如果没有这样的 buf，进入下一次循环
    if(b == 0){
        release(&bcache.hashlock[i]);
        continue;
    }
    // 如果找到的这个 buf 是空白的，操作、转移 buf
    if(b->blockno == 0){
      if (minindex != -1)
        release(&bcache.hashlock[minindex]);
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      // 转移 buf
      if (b->prev == 0){
        bcache.hashbuf[i] = b->next;
        if(b->next != 0)
          b->next->prev = 0;
      }
      else{
        b->prev->next = b->next;
        if(b->next != 0)
          b->next->prev = b->prev;
      }
      b->next = bcache.hashbuf[blockno%13];
      bcache.hashbuf[blockno%13] = b;
      b->prev = 0;
      if(b->next != 0)
        b->next->prev = b;

      release(&bcache.hashlock[i]);
      // release(&bcache.lock);
      release(&bcache.hashlock[blockno%13]);
      acquiresleep(&b->lock);
      // printf("*********************\n");
      return b;
    }
    // 如果找到的这个 buf 不是空白的，就需要记录 bufticks, 再进行检索
    firstgetted ++;
    if(firstgetted == 0){
      minb = b;
      minbufticks = currbufticks;
      minindex = i;
      // printf("xxx%d,%d\n",b->blockno,b->bufticks);
    } else {
      if(currbufticks < minbufticks){
        minb = b;
        minbufticks = currbufticks;
        release(&bcache.hashlock[minindex]);
        minindex = i;
      } else {
        release(&bcache.hashlock[i]);
      }
    }
    // printf("%d\n", minindex);
  }

  if (minindex != -1){
    // printf("%d**\n", minindex);
    if(currb == 0 || currb->bufticks > minbufticks){
      minb->dev = dev;
      minb->blockno = blockno;
      minb->valid = 0;
      minb->refcnt = 1;
      // 转移 buf
      if (minb->prev == 0){
        bcache.hashbuf[minindex] = minb->next;
        if(minb->next != 0)
          minb->next->prev = 0;
      }
      else{
        minb->prev->next = minb->next;
        if(minb->next != 0)
          minb->next->prev = minb->prev;
      }
      minb->next = bcache.hashbuf[blockno%13];
      bcache.hashbuf[blockno%13] = minb;
      minb->prev = 0;
      if(minb->next != 0)
        minb->next->prev = minb;

      release(&bcache.hashlock[minindex]);
      // release(&bcache.lock);
      release(&bcache.hashlock[blockno%13]);
      acquiresleep(&minb->lock);
      // printf("*********************\n");
      return minb;
    } else {
        currb->dev = dev;
        currb->blockno = blockno;
        currb->valid = 0;
        currb->refcnt = 1;

        release(&bcache.hashlock[minindex]);
        // release(&bcache.lock);
        release(&bcache.hashlock[blockno%13]);
        acquiresleep(&currb->lock);
        // printf("*********************\n");
        return currb;
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
  struct buf *bc;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

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

  // acquire(&bcache.lock);
  // b 会不会被其它进程改变？由于 b->refcont > 0, b暂时不会被其它 block cache
  acquire(&bcache.hashlock[b->blockno%13]);
  b->refcnt --;
  if(b->refcnt == 0) {
    // no one is waiting for it.
    b->bufticks = ticks;
    if(bcache.hashbuf[b->blockno%13]->next != 0){
      // 取出 b
      if (b->prev == 0){
        bcache.hashbuf[b->blockno%13] = b->next;
        if(b->next != 0)
          b->next->prev = 0;
      }
      else{
        b->prev->next = b->next;
        if(b->next != 0)
          b->next->prev = b->prev;
      }
      //把 b 放到最后，目的是让最近用过的 cache 不会被立即分配掉
      bc = bcache.hashbuf[b->blockno%13];
      while (bc->next != 0)
        bc = bc->next;
      bc->next = b;
      b->prev = bc;
      b->next = 0;
    }
  }
  release(&bcache.hashlock[b->blockno%13]);
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  // acquire(&bcache.lock);
  acquire(&bcache.hashlock[b->blockno%13]);
  b->refcnt++;
  release(&bcache.hashlock[b->blockno%13]);
  // release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  // acquire(&bcache.lock);
  acquire(&bcache.hashlock[b->blockno%13]);
  b->refcnt--;
  release(&bcache.hashlock[b->blockno%13]);
  // release(&bcache.lock);
}




