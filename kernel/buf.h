struct buf {
  // 标记位
  int valid;   // has data been read from disk? => 是否有有效内容
  int disk;    // does disk "own" buf? => disk是否被更新?????

  // 记录了这个buffer对应的设备号和块号
  // 注意函数virtio_disk_rw(struct buf *b, int write)本身参数是不带这些信息的
  uint dev; // 设备号
  uint blockno; // 设备里的blcok号

  struct sleeplock lock;

  // 多少caller要使用这个buffer
  // bread++ brelse--
  uint refcnt;

  // 用于链表LRU
  struct buf *prev; // LRU cache list
  struct buf *next;

  // 数据
  uchar data[BSIZE];
};

