struct buf {
  int valid;   // has data been read from disk?（缓冲区是否包含块的副本）
  int disk;    // does disk "own" buf? (是否将缓存写入到磁盘)
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

