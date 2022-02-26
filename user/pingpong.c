#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define READEND 0
#define WRITEEND 1

int
main(int argc, char *argv[])
{
    int pid;
    char buf[5];
    int fd[2];

    pipe(fd);

    if ((pid = fork()) < 0) {
        fprintf(STDERR, "fork error\n");
    } else if (pid == 0) {
        // child process
        read(fd[READEND], buf, 4);
        fprintf(STDOUT, "%d: received %s\n", getpid(), buf);
        write(fd[WRITEEND], "pong", 4);
    } else {
        // parent process
        write(fd[WRITEEND], "ping", 4);
        read(fd[READEND], buf, 4);
        fprintf(STDOUT, "%d: received %s\n", getpid(), buf);
    }
    exit(0);
}
