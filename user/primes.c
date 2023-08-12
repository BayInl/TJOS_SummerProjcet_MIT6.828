#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1

const uint INT_LEN = sizeof(int);

void primes(int* lpipe)
{
    int prime;
    close(lpipe[WR]);
    if (read(lpipe[RD], &prime, INT_LEN) == 0) // 管道空
        exit(0);
    printf("prime %d\n", prime);

    if (fork() == 0) { // 子进程
        // 创建新管道
        int rpipe[2];
        pipe(rpipe);
        // 筛选并写入新管道
        int num;

        while (read(lpipe[RD], &num, INT_LEN) != 0) {
            if (num % prime != 0)
                write(rpipe[WR], &num, INT_LEN);
        }
        close(lpipe[RD]);
        // 递归
        primes(rpipe);
    } else { // 父进程
        wait(0);
    }
}
int main(int argc, char const* argv[])
{
    int p[2];
    pipe(p);

    for (int i = 2; i <= 35; ++i) // 写入初始数据
        write(p[WR], &i, INT_LEN);

    primes(p);

    exit(0);
}