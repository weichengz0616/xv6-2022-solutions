// file description layer
struct file {
  // 提供了抽象, file自己根据type读写
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;

  // 可能有多个进程指向同一file, 一个进程多个描述符指向同一file(重定向)
  // 也可能多个file指向同一inode, 多个进程独立打开同一文件, 导致offset不同
  int ref; // reference count

  // 打开文件的形式 => 标记位
  char readable;
  char writable;

  // 不同类型使用不同的字段
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number

  // 多少C指针指向该inode, =0时在itable中释放. 区分nlink
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+2];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
