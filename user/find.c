#include "kernel/types.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"


void find(char *path, char *filename) {
    int fd;
    struct stat st;
    struct dirent de;
    char buf[512], *p;

    // 访问路径
    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取路径信息
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 在文件夹中查找文件名为filename的文件
    switch(st.type) {
        // 文件
        case T_FILE:
            // 匹配文件名
            if (strcmp(path + strlen(path) - strlen(filename), filename) == 0) {
                printf("%s\n", path);
            }
            break;
        
        // 目录
        case T_DIR:
            // 路径长度太长则返回错误
            if (strlen(path) + 1 + DIRSIZ > sizeof buf) {
                printf("find: path too long\n");
                break;                
            }
            
            // 添加斜杠
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';

            // 读取目录项
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                // 跳过空目录项和"." ".."目录
                if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {            
                    continue;
                }

                // 递归查询
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                find(buf, filename);
            }
    }
    close(fd);
}


int main(int argc, char *argv[]) {
    // 参数个数如果不为3，则返回异常
    if (argc != 3) {
        fprintf(2, "Usage: find directory filename\n");
        exit(1);
    }

    find(argv[1], argv[2]);

    exit(0);
}