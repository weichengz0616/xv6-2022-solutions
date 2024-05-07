// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12]; // 保留, 一般设为0
  ushort type; // ELF的类型, NONE or 可重定位 or 可执行 or ...
  ushort machine; // 目标体系结构???
  uint version; // 版本号
  uint64 entry; // 程序入口点, 即开始执代码的地方
  uint64 phoff; // Program Header Table offset 程序头表开始的地方
  uint64 shoff; // Section Header Table offset 节头表
  uint flags; 
  ushort ehsize; // elf header 大小
  ushort phentsize; // PH中一个条目的大小
  ushort phnum; // PH条目个数
  ushort shentsize; // SH条目大小
  ushort shnum; // SH条目个数
  ushort shstrndx; // 节名字符串表所在节表中的索引
};

// Program section header
struct proghdr {
  uint32 type;
  uint32 flags;
  uint64 off; // 起点
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz; // memsz可能大于filesz => 因为有初始化为0的data
  uint64 align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
