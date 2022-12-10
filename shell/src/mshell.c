#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

void initialize ();

void move_buffer ();

void read_prep ();

void length_check ();

// input info
struct stat stdin_info;
int is_tty;


void sigchld_handler(int signum, siginfo_t *info, void *context) {
    pid_t pid;
    int i;
    do {
        pid = waitpid(-1, &child_status, WNOHANG);
        if (pid > 0) {
            i = 0;
            while (i < foreground_all) {
                if (foreground[i] == pid) {
                    unfinished_foreground--;
                    break;
                }
                i++;
            }
            if (i >= foreground_all) {
                background_notes[finished_background].pid = pid;
                background_notes[finished_background].status = child_status;
                finished_background++;
            }
        }
    } while(pid > 0);
}


int main (int argc, char *argv[]) {

    initialize();

    while (1) {
        // signals_set = {SIGCHLD}

        sigprocmask(SIG_BLOCK, &signals_set, NULL);

        read_prep();

        sigprocmask(SIG_UNBLOCK, &signals_set, NULL);

        read_value = read(0, buffer.write_to_buffer_ptr, MAX_BUFFER_READ);
        if (read_value == -1) {
            perror("read: ");
            exit(EXEC_FAILURE);
        }

        sigprocmask(SIG_BLOCK, &signals_set, NULL);

        buffer.length += read_value;
        buffer.buf[buffer.length] = 0;

        // checking the end of file
        if (read_value == 0) {
            // at the end of file can be a correct command without '\n'
            if (buffer.length > 0)
                handle_line();

            exit(0);
        }

        buffer.end_of_command = strchr(buffer.buf, '\n');

        length_check();

        if (buffer.end_of_command != NULL)
            handle_multi_line();
    }
}


void initialize () {

    // since POSIX.1-2001
    sigchld_action.sa_sigaction = sigchld_handler;
    sigchld_action.sa_flags = SA_SIGINFO;
    sigemptyset(&sigchld_action.sa_mask);
    sigaction(SIGCHLD, &sigchld_action, NULL);

    sigint_action.sa_handler = SIG_IGN;
    sigint_action.sa_flags = 0;
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    sigemptyset(&signals_set);
    sigaddset(&signals_set, SIGCHLD);

    if (fstat(fileno(stdin), &stdin_info) == -1) {
        perror("fstat: ");
        exit(EXEC_FAILURE);
    }

    is_tty = S_ISCHR(stdin_info.st_mode);
    buffer.length = 0;

    finished_background = 0;

}


void move_buffer () {

    memmove(buffer.buf, buffer.end_of_command+1, buffer.length);
    buffer.buf[buffer.length] = 0;

    buffer.end_of_command = strchr(buffer.buf, '\n');

}


void read_prep () {

    if (is_tty) {
        background_report();
        fprintf(stdout, "%s", PROMPT_STR);
        fflush(stdout);
        buffer.write_to_buffer_ptr = buffer.buf;
    } else
        buffer.write_to_buffer_ptr = buffer.buf + buffer.length;

    buffer.begin_new_command = buffer.buf;
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

        buffer.length = read_value - (buffer.end_of_command+1 - buffer.buf);
        move_buffer();
    }
    // current command's '\n' is further than max line length
    // program moves next command to the beginning of buffer
    else if(buffer.end_of_command+1-buffer.buf > MAX_LINE_LENGTH) {
        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);

        buffer.length -= (buffer.end_of_command+1 - buffer.buf);
        move_buffer();
    }

    //after checks new command starts at the beginning of the file
    buffer.begin_new_command = buffer.buf;

}
