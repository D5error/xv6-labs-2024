#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char *argv[]) {
    // 参数个数如果不为2，则返回异常
    if (argc != 2) {
        fprintf(2, "Usage: sleep ticks\n");
        exit(1);
    }

    // 读取参数n
    int n = atoi(argv[1]);
    
    // 睡眠一段时间，时长为n个tick
    sleep(n);

    exit(0);
}