#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define STDIN 0
#define MAXBUF 1024

void execute(char * cmd, char * params[MAXARG])
{
    if (fork() == 0) {
        // child process
        exec(cmd, params);
    }
}

int main(int argc, char *argv[])
{
    char * cmd = argv[1];
    int cmd_index = argc - 1;
    char buf[MAXBUF];
    char * p_buf = buf;
    // cmd_params is an array which contains pointers to string
    char * cmd_params[MAXARG];
    /*char * p = buf;*/

    if (argc < 2) {
        fprintf(2, "Usage: argv cmd [cmd ...]\n");
        exit(1);
    } else {
        for (int i = 0; i < argc - 1; i++) {
            cmd_params[i] = argv[i+1];
        }
    }
    // fill up the exe_list
    while (read(STDIN, p_buf, 1) > 0) {
        if (*p_buf == '\n') {
            *p_buf = 0;
            char * arg = (char *) malloc(sizeof(buf));
            strcpy(arg, buf);
            cmd_params[cmd_index] = arg;
            // clear buf and set p to the start of the buf
            memset(buf, 0, MAXBUF);
            // fork and exec
            execute(cmd, cmd_params);
            // reset the status
            for (int i = argc - 1; i <= cmd_index; i++) {
                // in case of cmd_params == NULL, will cause usertrap(): unexpected scause
                /*if (cmd_params[i])*/
                    free(cmd_params[i]);
            }
            cmd_index = argc - 1;
            p_buf = buf;
        } else if (*p_buf == ' ') {
            *p_buf = 0;
            char * arg = (char *) malloc(sizeof(buf));
            strcpy(arg, buf);
            cmd_params[cmd_index] = arg;
            cmd_index++;
            // clear buf and set p to the start of the buf
            memset(buf, 0, MAXBUF);
            p_buf = buf;
        } else {
            p_buf++;
        }
    }
    if (strlen(buf) != 0) {
        char * arg = (char *) malloc(sizeof(buf));
        strcpy(arg, buf);
        cmd_params[cmd_index] = arg;
        execute(cmd, cmd_params);
    }
    /*wait((int *) 0);*/
    exit(0);
} 
