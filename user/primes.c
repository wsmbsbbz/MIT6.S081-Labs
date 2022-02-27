#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int);

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    if (fork() == 0) {
        // child process
        close(p[1]);
        // recurse
        prime(p[0]);
    } else {
        // parent process
        close(p[0]);
        for (int i = 2; i < 36; i++) {
            write(p[1], &i, sizeof(int));
        }
        close(p[1]);
    }
    wait((int *) 0);
    exit(0);
}

void prime(int input_fd)
{
    int base;
    if (read(input_fd, &base, sizeof(int)) == 0) {
        exit(0);
    }
    fprintf(1, "prime %d\n", base);

    int p[2];
    int num;
    int eof;
    pipe(p);

    if (fork() == 0) {
        // child process
        close(p[1]);
        prime(p[0]);
    } else {
        // parent process
        do {
            eof = read(input_fd, &num, sizeof(num));
            if (num % base) {
                write(p[1], &num, sizeof(int));
            }
        } while (eof);
        close(p[1]);
    }
    wait((int *) 0);
    exit(0);
}
