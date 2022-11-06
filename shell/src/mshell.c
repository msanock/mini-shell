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
#include "builtins.h"


struct Buffer {
    char buf[BUFFER_SIZE];
    int length;
    char * end_of_command;
    char * begin_new_command;
    char * write_to_buffer_ptr;
} buffer;

// input info
struct stat stdin_info;
int is_tty;

// process info
pid_t child_pid;
int status;

// number of bytes read by read function
ssize_t read_value;

pipelineseq *ln;

char ** get_command_args (argseq *);

void run_child_process (char **);

void handle_line ();

void move_buffer ();

void read_prep ();

void length_check ();

void handle_multi_line ();


int main (int argc, char *argv[]) {

    if (fstat(fileno(stdin), &stdin_info) == -1) {
        perror("fstat: ");
        exit(EXEC_FAILURE);
    }
    is_tty = S_ISCHR(stdin_info.st_mode);
    buffer.length = 0;

    while (1) {

        read_prep();

        read_value = read(0, buffer.write_to_buffer_ptr, MAX_BUFFER_READ);
        if (read_value == -1) {
            perror("read: ");
            exit(EXEC_FAILURE);
        }

        buffer.length += read_value;
        buffer.buf[buffer.length] = 0;

        // checking the end of file
        if (read_value == 0) {
            // at the end of file can be a correct command without '\n'
            if (buffer.length > 0)
                handle_line();

            return 0;
        }

        buffer.end_of_command = strchr(buffer.buf, '\n');

        length_check();

        //after checks new command starts at the beginning of the file
        buffer.begin_new_command = buffer.buf;//delete

        if (buffer.end_of_command != NULL)
            handle_multi_line();

    }
}

void read_prep () {

    if (is_tty) {
        fprintf(stdout, "%s", PROMPT_STR);
        fflush(stdout);
        buffer.write_to_buffer_ptr = buffer.buf;
    } else
        buffer.write_to_buffer_ptr = buffer.buf + buffer.length;


    buffer.begin_new_command = buffer.buf;

}

void handle_line (){

    if(*buffer.begin_new_command == 0 || *buffer.begin_new_command == '#')
        return;

    ln = parseline(buffer.begin_new_command);
    if (ln == NULL) {
        fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
    }

    command * com = pickfirstcommand(ln);
    argseq * args = com->args;

    char ** args_array = get_command_args(args);

    fptr builtin_fun = is_builtin(args_array[0]);

    if (builtin_fun != NULL) {
        if (builtin_fun(args_array))
            fprintf(stderr, BUILTIN_ERROR_STR, args_array[0]);
    } else{
        child_pid = fork();

        if (child_pid == 0)
            run_child_process(args_array);
        else
            waitpid(child_pid, &status, 0);
    }

}

void run_child_process (char ** args_array) {

    // handling errors that may occur
    if (execvp(args_array[0], args_array) == -1) {
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

void length_check () {

    // current command is too long and there isn't '\n' in buffer
    // first, program finds end of current command, than it moves to next command
    // to the beginning of buffer.
    // special case: the end of command may be EOF
    if (buffer.end_of_command == NULL && buffer.length > MAX_LINE_LENGTH) {

        do {
            read_value = read(0, buffer.buf, MAX_BUFFER_READ);
            buffer.buf[read_value] = 0;

            buffer.end_of_command = strchr(buffer.buf, '\n');
        } while (buffer.end_of_command == NULL && !feof(stdin));

        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);

        if (feof(stdin))
            exit(0);

        buffer.length = read_value;
    }
    // current command's '\n' is further than max line length
    // program moves next command to the beginning of buffer
    else if(buffer.end_of_command+1-buffer.buf > MAX_LINE_LENGTH)
        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);


    buffer.length -= (buffer.end_of_command+1 - buffer.buf);
    move_buffer();

    //after checks new command starts at the beginning of the file
    buffer.begin_new_command = buffer.buf;

}

void handle_multi_line () {

    //as long as there is command which ends with '\n' execute it
    while(buffer.end_of_command != NULL) {
        *buffer.end_of_command = 0;

        handle_line();

        //moving to the next command
        buffer.length -= (buffer.end_of_command+1) - buffer.begin_new_command;
        buffer.begin_new_command = buffer.end_of_command+1;

        buffer.end_of_command = strchr(buffer.begin_new_command, '\n');
    }

    //move what's left to the beginning of buffer
    memmove(buffer.buf, buffer.begin_new_command, buffer.length);

}

void move_buffer () {

    memmove(buffer.buf, buffer.end_of_command+1, buffer.length);
    buffer.buf[buffer.length] = 0;

    buffer.end_of_command = strchr(buffer.buf, '\n');

}

char ** get_command_args(argseq * args) {

    // counting number of args, additional memory for NULL
    argseq * current = args;

    int i = 1;
    do {
        i++;
        current = current->next;
    } while (args != current);

    char **args_array = (char **) malloc(i * sizeof(char *));

    // assigning values
    current = args;
    i = 0;
    do {
        args_array[i++] = current->arg;
        current = current->next;
    } while (args != current);

    args_array[i] = NULL;

    return args_array;

}
