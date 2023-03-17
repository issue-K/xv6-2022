#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int main(int argc,char *argv[]) {
    if (argc != 1) {
        fprintf(2, "parm != 0");
        exit(1);
    }
    int p[2];
    pipe(p); // p[0]读, p[1]写
    int pid = fork();
    if(pid > 0) {
        // 父进程
        char msg;
        write(p[1], "c", 1);
        read(p[0], &msg, 1);
        printf("%d: received pong\n", getpid());
    } else if (pid == 0){
        // 子进程
        char msg;
        read(p[0], &msg, 1);
        printf("%d: received ping\n", getpid());
        write(p[1], "l", 1);
        exit(0);
    } else {
        fprintf(2, "fork error\n");
        exit(1);
    }
    exit(0);
}