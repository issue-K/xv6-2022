#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char *path, char *s) {
  // printf("find => %s\n", path);
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  // 填充stat结构体
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    break;
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      fprintf(2, "find: path too long");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if (strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0) {
        continue;
      }
      char *temp = p;
      memmove(temp, de.name, DIRSIZ);
      temp[DIRSIZ] = 0;
      temp += strlen(de.name);
      *temp++ = '/';
      *temp++ = 0;
      if (strcmp(de.name, s) == 0) {
        printf("%s\n", buf);
      }
      find(buf, s);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[]) {
    if (argc !=3 ) {
        fprintf(2, "find parm != 3\n");
        exit(1);
    }
    char *dir = argv[1];
    dir = dir + strlen(dir);
    *dir++ = '/';
    *dir++ = 0;
    find(argv[1], argv[2]);
    exit(0);
}