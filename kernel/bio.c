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
  struct buf buf[NBUF];// 初始化用到(内核数据固定长度, 这里没有动态分配), 实际操作不会用这个静态数组

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;


// main.c调用
// 最终结果: 0, 1, 2,...,NBUF-1 构成双向链表
// 0->next = head
// (NBUF-1)->prev = head
// head->prev = 0
// head->next = NBUF-1

// head->next为最近新使用过的
// head->prev为最远使用的 => LRU的victim
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // 初始化静态数组, 链表连接
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
// 这里只是找个buffer位置, 不一定是cached
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 拿锁, 确保同一个dev的同一个blockno在buffer cache中只有一个对应
  acquire(&bcache.lock);

  // Is the block already cached?
  // 相当细节啊, 从MRU开始检查, 利用局部性
  // refcnt表示多少caller在等待这个buffer!!!!!!!
  // brelse会让refcnt--
  // 注意细节: 先让refcnt++, 再拿锁 => 同时只有一个caller在读写, 但refcnt可以大于1 => 等待brelse放锁
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);

      // 细节: 先放了bcache的锁再拿的buffer的锁
      // 原因在于, 放了bcache的锁之后, refcnt > 0, 因此一定不会被另一个block占用
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    // 注意这里找的是refcnt=0的buffer
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
}

// Return a locked buf with the contents of the indicated block.
// 无论读写, bread一定是caller的起点, brelse一定是终点, 这也是为什么brelse中才放锁和处理LRU的原因
// 将dev-blockno的数据读到buffer cache中, 返回其对应的buffer
// 注意refcnt
// caller在调用这个函数后, 可以独自对buffer进行读/写 => 直接用读写b->data
// caller如果写了 b->data, 必须在释放b之前调用bwrite(b)
// caller用完buffer之后, 必须调用brelse(b)
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
// caller用完buffer, 必须调用brelse
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
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


