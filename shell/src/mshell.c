#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "string.h"

struct Buffer {
    char buf[BUFFER_SIZE];
    int length;
    char * end_of_command;
    char * begin_new_command;
    char * write_to_buffer_ptr;
}buffer;

void run_child(pipelineseq * ln) {
    command * com = pickfirstcommand(ln);
    argseq * args = com->args;
    //redirseq * redirs = com->redirs;

    argseq * current = args;
    int i = 1;
    do{
        i++;
        current = current->next;
    } while (args != current);

    char ** args_array = (char **)malloc(i * sizeof(char *));

    current = args;
    i = 0;
    do{
        args_array[i] = current->arg;
        i++;
        current = current->next;
    } while (args != current);
    args_array[i] = NULL;


    if (execvp(args_array[0],args_array) == -1) {
        switch (errno) {
            case ENOENT:
                fprintf(stderr, "%s%s", args_array[0], BAD_ADDRESS_ERROR_STR);
                break;
            case EACCES:
                fprintf(stderr, "%s%s", args_array[0], PERMISSION_ERROR_STR);
                break;
            default:
                fprintf(stderr, "%s%s", args_array[0], EXEC_ERROR_STR);
        }
        exit(EXEC_FAILURE);
    }
}

int main (int argc, char *argv[]) {

    pipelineseq *ln;

    //process info
    pid_t child_pid;
    int status;

    //number of bytes read by read function
    ssize_t read_value;

    //input info
    struct stat stdin_info;
    int is_tty;

    if (fstat(fileno(stdin), &stdin_info) == -1) {
        perror("fstat: ");
        exit(EXEC_FAILURE);
    }

    is_tty = S_ISCHR(stdin_info.st_mode);

    buffer.length = 0;
    while (1) {
        if (is_tty) {
            fprintf(stdout, "%s", PROMPT_STR);
            fflush(stdout);
            buffer.write_to_buffer_ptr = buffer.buf;
        } else {
            buffer.write_to_buffer_ptr = buffer.buf + buffer.length;
        }

        read_value = read(0, buffer.write_to_buffer_ptr, MAX_BUFFER_READ);
        if (read_value == -1) {
            perror("read: ");
            exit(EXEC_FAILURE);
        }

        //checking the end of file
        if (read_value == 0) {
            if (buffer.length > 0){
                buffer.buf[buffer.length] = 0;
                ln = parseline(buffer.buf);
                if (ln == NULL) {
                    fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
                }
                child_pid = fork();
                if (child_pid == 0) {
                    run_child(ln);
                }
                else{
                    waitpid(child_pid, &status, 0);
                    return 0;
                }
            }
            return 0;
        }

        buffer.length += read_value;
        buffer.buf[buffer.length] = 0;

        buffer.end_of_command = strchr(buffer.buf, '\n');

        if (buffer.end_of_command == NULL && buffer.length > MAX_LINE_LENGTH) {
            do {
                read_value = read(0, buffer.buf, MAX_BUFFER_READ);
                buffer.buf[read_value] = 0;
                buffer.end_of_command = strchr(buffer.buf, '\n');
            } while(buffer.end_of_command == NULL);
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            buffer.length = read_value - (buffer.end_of_command+1-buffer.buf);
            memmove(buffer.buf, buffer.end_of_command+1, buffer.length);
            buffer.buf[buffer.length] = 0;
            buffer.end_of_command = strchr(buffer.buf, '\n');
        }
        else if(buffer.end_of_command+1-buffer.buf > MAX_LINE_LENGTH) {
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            buffer.length -= (buffer.end_of_command+1-buffer.buf);
            memmove(buffer.buf, buffer.end_of_command+1, buffer.length);
            buffer.buf[buffer.length] = 0;
            buffer.end_of_command = strchr(buffer.buf, '\n');
        }

        buffer.begin_new_command = buffer.buf;
        while(buffer.end_of_command != NULL) {
            *buffer.end_of_command = 0;
            ln = parseline(buffer.begin_new_command);
            if (ln == NULL) {
                fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
            }

            child_pid = fork();
            if (child_pid == 0) {
                run_child(ln);
            }
            else{
                waitpid(child_pid, &status, 0);
            }
            buffer.length -= (buffer.end_of_command+1) - buffer.begin_new_command;
            buffer.begin_new_command = buffer.end_of_command+1;
            buffer.end_of_command = strchr(buffer.begin_new_command, '\n');
        }
        if(buffer.begin_new_command != buffer.buf)
            memmove(buffer.buf, buffer.begin_new_command, buffer.length);
    }
}