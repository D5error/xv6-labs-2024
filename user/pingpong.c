#include "kernel/types.h"
#include "user/user.h"
#include <stddef.h>  // 最标准的定义NULL的头文件

int main(int argc, char *argv[]) {
    // 参数个数如果不为1，则返回异常
    if (argc != 1) {
        fprintf(2, "Usage: pingpong\n");
        exit(1);
    }

    // 创建管道
    int ping_pipe[2];
    int pong_pipe[2];
    if (pipe(ping_pipe) < 0 || pipe(pong_pipe) < 0) {
        fprintf(2, "pipe creation failed\n");
        exit(1);
    }

    // 创建子进程
    int child_pid = fork();
    int curr_pid = getpid();
    if (child_pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    char buf[1];

    // 父进程
    if (child_pid != 0) {
        // 发送一个字节到管道中
        buf[0] = '6';
        write(ping_pipe[1], buf, 1);
        close(ping_pipe[1]);

        // 读取管道
        read(pong_pipe[0], buf, 1);
        close(pong_pipe[0]);
        fprintf(1, "%d: received pong\n", curr_pid);

        // 等待子进程结束
        wait(NULL);
    }

    // 子进程
    else {
        // 读取管道
        read(ping_pipe[0], buf, 1);
        close(ping_pipe[0]);
        fprintf(1, "%d: received ping\n", curr_pid);

        // 发送
        buf[0] = '8';
        write(pong_pipe[1], buf, 1);
        close(pong_pipe[1]);
    }
    exit(0);
}
