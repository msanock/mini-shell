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
            case EFAULT:
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

    //we store information on child process below
    pid_t child_pid;
    int status;

    //storing value of read()
    ssize_t read_value;
    int length = 0;
    char * end_of_command;
    char * begin_new_command;

    char buf[BUFFER_SIZE];
    char * write_to_buffer_ptr;

    struct stat stdin_info;
    int is_tty;

    if (fstat(fileno(stdin), &stdin_info) == -1) {
        perror("fstat: ");
        exit(EXEC_FAILURE);
    }

    is_tty = S_ISCHR(stdin_info.st_mode);

    while (1) {
        if (is_tty) {
            fprintf(stdout, "%s", PROMPT_STR);
            fflush(stdout);
            write_to_buffer_ptr = buf;
        } else {
            write_to_buffer_ptr = buf + length;
        }
        read_value = read(0, write_to_buffer_ptr, MAX_LINE_LENGTH);
        if (read_value == -1) {
            perror("read: ");
            exit(EXEC_FAILURE);
        }
        if (read_value == 0) {
            return 0;
        }

        length += read_value;
        buf[length] = 0;

        end_of_command = strchr(buf, '\n');
        
        if (end_of_command == NULL && length > MAX_LINE_LENGTH) {
            do {
                read_value = read(0, buf, MAX_LINE_LENGTH);
                buf[read_value] = 0;
                end_of_command = strchr(buf, '\n');
            } while(end_of_command == NULL);
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            length = read_value - (end_of_command+1-buf);
            memmove(buf, end_of_command+1, length);
            //buf[length] = 0;
            end_of_command = strchr(buf, '\n');
        } else if(end_of_command+1-buf > MAX_LINE_LENGTH) {
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            length -= (end_of_command+1-buf);
            memmove(buf, end_of_command+1, length);
            //buf[length] = 0;
            end_of_command = strchr(buf, '\n');
        }

        begin_new_command = buf;
        while(end_of_command != NULL) {
            *end_of_command = 0;
            ln = parseline(begin_new_command);
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
            begin_new_command = end_of_command+1;
            length -= begin_new_command - buf;
            end_of_command = strchr(begin_new_command, '\n');
        }
        if(begin_new_command != buf) memmove(buf, begin_new_command, length);
    }
}