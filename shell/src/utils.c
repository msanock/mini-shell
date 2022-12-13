#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "utils.h"
#include "config.h"
#include "builtins.h"

command * pickfirstcommand(pipelineseq * ppls)
{
	if ((ppls==NULL)
		|| (ppls->pipeline==NULL)
		|| (ppls->pipeline->commands==NULL)
		|| (ppls->pipeline->commands->com==NULL))	return NULL;

	return ppls->pipeline->commands->com;
}

void handle_multi_line () {

    // as long as there is command which ends with '\n' execute it
    while (buffer.end_of_command != NULL) {
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

void handle_line (){

    if (*buffer.begin_new_command == 0 || *buffer.begin_new_command == '#')
        return;

    ln = parseline(buffer.begin_new_command);
    if (ln == NULL)
        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);

    pipelineseq * current = ln;

    do {
        handle_pipeline(current->pipeline);

        current = current->next;
    } while (current != ln);

}

void handle_pipeline (pipeline * ps) {
    int file_descriptors[2];

    is_background_process = (ps->flags == INBACKGROUND);

    file_descriptors[0] = 0;
    file_descriptors[1] = 1;

    commandseq * current = ps->commands;

    // signals_set = {SIGCHLD}
    sigprocmask(SIG_UNBLOCK, &signals_set, NULL);

    do {
        if (handle_command_in_pipeline(current->com, file_descriptors, (current->next != ps->commands))){
            if (file_descriptors[0] != 0) close(file_descriptors[0]);
            if (file_descriptors[1] != 1) close(file_descriptors[1]);

            return;
        }

        current = current->next;
    } while (current != ps->commands);


    if (file_descriptors[0] != 0) close(file_descriptors[0]);
    if (file_descriptors[1] != 1) close(file_descriptors[1]);


    sigemptyset(&signals_set);
    // signals_set = { }

    while (unfinished_foreground)
        sigsuspend(&signals_set);

    sigaddset(&signals_set, SIGCHLD);
    // signals_set = {SIGCHLD}

    foreground_all = 0;

}

int handle_command_in_pipeline (command* com, int * file_descriptors, int has_next) {

    if (com == NULL)
        return 0;

    argseq * args = com->args;
    redirseq * redirs = com->redirs;

    int input;
    int old_output;

    char ** args_array = get_command_args(args);
    fptr builtin_fun = is_builtin(args_array[0]);

    if (builtin_fun != NULL) {
        if (builtin_fun(args_array)) {
            fprintf(stderr, BUILTIN_ERROR_STR, args_array[0]);

            return BUILTIN_FAILURE;
        }

        return BUILTIN_SUCCESS;
    }

    input = file_descriptors[0];
    old_output = file_descriptors[1];

    pipe(file_descriptors);

    child_pid = fork();

    if (child_pid == 0) {

        sigchld_action.sa_handler = SIG_DFL;
        sigchld_action.sa_flags = 0;

        sigint_action.sa_handler = SIG_DFL;

        sigaction(SIGCHLD, &sigchld_action, NULL);
        sigaction(SIGINT, &sigint_action, NULL);

        sigfillset(&signals_set);
        sigprocmask(SIG_UNBLOCK, &signals_set, NULL);
        sigemptyset(&signals_set);

        if (is_background_process)
            setsid();

        // input should be the output of previous child (or 0 if first)
        dup2(input, 0);
        if (input != 0)
            close(input);

        // next child's input, not needed here
        close(file_descriptors[0]);

        // if there is next child, output will be next child's input, otherwise descriptor should be closed
        if (has_next)
            dup2(file_descriptors[1], 1);


        close(file_descriptors[1]);

        // descriptor that previous child wrote to
        if (old_output != 1)
            close(old_output);

        handle_redirs(redirs);

        run_child_process(args_array);

    } else {

        if (!is_background_process) {
            foreground[foreground_all++] = child_pid;
            unfinished_foreground++;
        }
        if (input != 0) {
            close(input);
            close(old_output);
        }

    }

    return 0;
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

void handle_redirs(redirseq * redirs) {
    redirseq * current = redirs;
    int new_descriptor;
    int flags;

    if (redirs == NULL)
        return;

    do {
        if (IS_RIN(current->r->flags))
            flags = O_RDONLY;

        else {
            flags = O_WRONLY | O_CREAT;
            if (IS_RAPPEND(current->r->flags))
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;
        }

        new_descriptor = open(current->r->filename, flags, 0644);

        if (new_descriptor == -1){
            switch (errno) {
                case ENOENT:
                    fprintf(stderr, "%s%s", current->r->filename, BAD_ADDRESS_ERROR_STR);
                    break;

                case EACCES:
                    fprintf(stderr, "%s%s", current->r->filename, PERMISSION_ERROR_STR);
                    break;

                case EPERM:
                    fprintf(stderr, "%s%s", current->r->filename, PERMISSION_ERROR_STR);
                    break;
            }

            exit(WRONG_REDIR);
        }

        if (IS_RIN(current->r->flags))
            dup2(new_descriptor, 0);
        else
            dup2(new_descriptor, 1);

        close(new_descriptor);

        current = current->next;
    } while (redirs != current);

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

void background_report(){

    for (int i = 0; i < finished_background; i++) {
        fprintf(stdout, "Background process %d terminated. ", background_notes[i].pid);

        if (WIFEXITED(background_notes[i].status))
            fprintf(stdout, "(%s%d)\n", BACKGROUND_EXITED, WEXITSTATUS(background_notes[i].status));
        else if (WIFSIGNALED(background_notes[i].status))
            fprintf(stdout, "(%s%d)\n", BACKGROUND_KILLED, WTERMSIG(background_notes[i].status));
    }

    fflush(stdout);
    finished_background = 0;

}

