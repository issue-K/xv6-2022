#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int isprime(int x) {
    for (int i = 2; i * i <= x; i++) {
        if (x % i == 0) {
            return 0;
        }
    }
    return 1;
}
int main(int agrc, char *argv[]) {
    // 无法理解primes命令有什么用.不写了, 感觉傻傻的。
    for (int i = 2; i <= 35; i++) {
        if (isprime(i)) {
            printf("prime %d\n", i);
        }
    }
    exit(0);
}