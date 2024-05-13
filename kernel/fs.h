// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 11 // => 一个inode有12个直接块
#define NINDIRECT (BSIZE / sizeof(uint)) // => 1个间接块, 这个块里存了块地址
#define NINDIRECT2 NINDIRECT * NINDIRECT
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT2)

// On-disk inode structure
// disk上的数据结构, 内存保存的inode信息更多
struct dinode {
  // file/directory/special file(eg. device) type=0表示未使用
  short type;           // File type 

  // 设备号到底是什么????????
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)

  // 多少目录的entry引用该inode, 等于0时表示释放该inode和data
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
// b这个块所在的bitmap块号
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// directory entry
// inum=0表示entry为空 => ialloc中inum确实从1开始分配
// 名字最多14个字符, 不支持长名字 => linux如何支持长名文件???????
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

