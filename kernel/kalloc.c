// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);


// 运行时的物理内存分配只使用了 kernel end ---- PHYSTOP 部分
// 并不是全部RAM
// 内核栈, 页表, pipe buffer, 用户进程的地址空间 都在这个范围!!!!!!!!
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// 锁 + 链表
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");

  // 初始化freelist
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // 妙啊, 强行修改释放页内的值
  // 这样的话, 会让悬挂指针更早出现问题 => 更容易debug和发现悬挂问题
  memset(pa, 1, PGSIZE);

  // 妙啊
  // 把pa强转为 run*
  // 这相当于将链表的各个元素存在自己的空闲page里的
  r = (struct run*)pa;

  acquire(&kmem.lock);
  // 头插法
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  // 若没有空闲物理页了, 返回空
  // 这里是可以考虑扩展的 => 将物理页换到磁盘
  return (void*)r;
}
