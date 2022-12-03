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

void 
printcommand(command *pcmd, int k)
{
	int flags;

	printf("\tCOMMAND %d\n",k);
	if (pcmd==NULL){
		printf("\t\t(NULL)\n");
		return;
	}

	printf("\t\targv=:");
	argseq * argseq = pcmd->args;
	do{
		printf("%s:", argseq->arg);
		argseq= argseq->next;
	}while(argseq!=pcmd->args);

	printf("\n\t\tredirections=:");
	redirseq * redirs = pcmd->redirs;
	if (redirs){
		do{	
			flags = redirs->r->flags;
			printf("(%s,%s):",redirs->r->filename,IS_RIN(flags) ?"<": IS_ROUT(flags) ?">": IS_RAPPEND(flags)?">>":"??");
			redirs= redirs->next;
		} while (redirs!=pcmd->redirs);	
	}

	printf("\n");
}

void
printpipeline(pipeline * p, int k)
{
	int c;
	command ** pcmd;

	commandseq * commands= p->commands;

	printf("PIPELINE %d\n",k);
	
	if (commands==NULL){
		printf("\t(NULL)\n");
		return;
	}
	c=0;
	do{
		printcommand(commands->com,++c);
		commands= commands->next;
	}while (commands!=p->commands);

	printf("Totally %d commands in pipeline %d.\n",c,k);
	printf("Pipeline %sin background.\n", (p->flags & INBACKGROUND) ? "" : "NOT ");
}

void
printparsedline(pipelineseq * ln)
{
	int c;
	pipelineseq * ps = ln;

	if (!ln){
		printf("%s\n",SYNTAX_ERROR_STR);
		return;
	}
	c=0;

	do{
		printpipeline(ps->pipeline,++c);
		ps= ps->next;
	} while(ps!=ln);

	printf("Totally %d pipelines.",c);
	printf("\n");
}

command *
pickfirstcommand(pipelineseq * ppls)
{
	if ((ppls==NULL)
		|| (ppls->pipeline==NULL)
		|| (ppls->pipeline->commands==NULL)
		|| (ppls->pipeline->commands->com==NULL))	return NULL;
	
	return ppls->pipeline->commands->com;
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

    do {
        if(handle_command_in_pipeline(current->com, file_descriptors, (current->next != ps->commands)))
            return;

        current = current->next;
    } while (current != ps->commands);

    close(file_descriptors[0]);
    close(file_descriptors[1]);

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    while(unfinished_foreground){
        sigemptyset(&set);
        sigsuspend(&set);
    }
}

int handle_command_in_pipeline (command* com, int * file_descriptors, int has_next) {

    argseq * args = com->args;
    redirseq * redirs = com->redirs;

    int input;
    int old_output;

    if (*com->args->arg == 0 || *com->args->arg == '#'){
        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
        return -1;
    }

    char ** args_array = get_command_args(args);

    fptr builtin_fun = is_builtin(args_array[0]);

    if (builtin_fun != NULL) {
        if (builtin_fun(args_array)) {
            fprintf(stderr, BUILTIN_ERROR_STR, args_array[0]);
            return -1;
        }
        return 1;
    }

    input = file_descriptors[0];
    old_output = file_descriptors[1];

    pipe(file_descriptors);

    child_pid = fork();

    if (child_pid == 0) {
        signal(SIGINT, SIG_DFL);
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_UNBLOCK, &set, NULL);

        if (is_background_process)
            setsid();

        dup2(input, 0);
        if(old_output!= 1) close(old_output);

        close(file_descriptors[0]);

        if (has_next) {
            dup2(file_descriptors[1], 1);
        } else {
            close(file_descriptors[1]);
        }

        handle_redirs(redirs);

        run_child_process(args_array);
    } else{
        if (!is_background_process){
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
    if (redirs == NULL)
        return;

    redirseq * current = redirs;
    int new_descriptor;
    int flags = 0;


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

