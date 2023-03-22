// - 使用fork和exec来对每一行输入调用该命令。在父进程中使用wait等待子进程完成命令。
// - 要读取单个输入行，每次读取一个字符，直到出现换行符('\n')。
// - kernel/param.h声明了MAXARG，如果你需要声明argv数组，它可能会很有用。
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
int eqs(char s) {
    return (s) == ' ';
}
int eqn(char s) {
    return (s) == '\n';
}
void cmd(char *cmd_name, char *argv[]) {
    int pid = fork();
    if (pid == 0) {
        exec(cmd_name, argv);
    } else if (pid > 0) {
        wait(&pid);
    } else {
        fprintf(2, "fork error.\n");
    }
}
int readcmd(int init_c, char *init_parm[]) {
    int argc = 1;
    char *argv[MAXARG];
    argv[0] = "cl's xargs.";
    for (int i = 2; i < init_c; i++) {
        argv[argc++] = init_parm[i];
    }
    char buf[512], c; // 读取参数的缓冲区
    char *head = buf; // 单词的开头位置
    char *p = buf; // 当前的读取位置
    while(read(0, &c, 1) == 1) {
        if (eqs(c) || eqn(c)) {
            // 标志着一个单词的结束
            if (p != head) {
                *p++ = 0;
                argv[argc++] = head;
                //printf("参数为%s ", head);
                head = p;
            }
        } else {
            *p++ = c;
        }
        if (eqn(c)) {
            argv[argc] = 0;
            cmd(init_parm[1], argv);
            return 1;
        }
    }
    return 0;
}
int main(int argc, char *argv[]) {
    // 从标准输入中一直读取
    //printf("begin: ");
    while(readcmd(argc, argv)) { }
    exit(0);
}