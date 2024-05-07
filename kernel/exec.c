#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();


  //=============================== 处理ELF ===========================
  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;

  // 分配页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  // 遍历PH, 分配物理内存, 建立映射, 最后将ELF文件load到分配的物理内存中
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;

    // 这个检查很tricky, 防止64位无符号整数溢出, 即memsz很大
    // 意义是????
    // 这个版本的xv6的内核有单独页表, 不会破坏内核本身
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;

    // sz是虚拟地址起点, vaddr + memsz是虚拟地址终点
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;

    // 分配的是memsz, 加载的只有filesz
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;
  //=============================== 处理ELF ===========================




  //=============================== 处理栈, 维护proc信息 ===========================
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.
  // 分配两页虚拟地址, 建立映射, 一页guard, 一页stack
  // sz是当前已经分配的虚拟空间大小(处理ELF文件之后的)
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  // 这是因为uvm分配时, R和U权限是默认的, 因此这里清除guard页面的U权限
  uvmclear(pagetable, sz-2*PGSIZE);
  // 栈指针(此时是栈顶)
  sp = sz;
  // 栈底, 超出即溢栈
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  // 默认argv是null结尾的
  // 参数字符串大小是未知的
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    
    // +1, null结尾
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;

    // kernel to user 复制参数过去
    // argv是在内核栈上??? 
    // 那么用户在调用该系统调用时, 也需要将参数赋值到内核栈上????
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    
    // 记录每个参数在栈上的起点
    ustack[argc] = sp;
  }
  // 细节以0结尾, argc = N + 1, 即参数个数
  // 0,1,2,...,N,N+1
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  // 此时sp还在n个参数的地方, 即用户地址空间的 argument N的位置
  // ustack有argc + 1个element
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  // 对应 address of argument
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  // 应该是address of arg 0
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  // 维护进程变量
  // 这里只修改了 页表, size, sp, epc => 也就是说改变了虚拟地址空间, memory
  // 也就是其他量对于exec来说没变!!!!!!!!
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  //=============================== 处理栈, 维护proc信息 ===========================

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
// [va, va + sz]这段虚拟地址空间已经写入了页表, 即已经分配相应的物理页面
// 目的, 加载程序到物理memory
// ip为文件, va为虚拟地址起点, offset为文件内起点(偏移)
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    
    // 最后一页不一定写得满
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}
