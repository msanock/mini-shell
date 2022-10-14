#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "string.h"


int run_child(pipelineseq * ln) {
    command * com = pickfirstcommand(ln);
    argseq * args = com->args;
    //redirseq * redirs = com->redirs;

    //had better change for counting args and changing for dynamic array
    //char ** args_array = (char **)malloc(MAX_ARGS * sizeof(char *));
    char * args_array[MAX_ARGS];

    argseq * current = args;
    int i = 0;
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
        return EXEC_FAILURE;
    }

    return 0;
}


int
main (int argc, char *argv[])
{
	pipelineseq * ln;
	//command *com;

    //we store information on child process below
    pid_t child_pid;
    int status;

    //storing value of read()
    ssize_t read_value;

	char buf[MAX_LINE_LENGTH];



	while (1) {
        fprintf(stdout, "%s", PROMPT_STR);
        fflush(stdout);

        //without this buffer gets overloaded for some reason
        memset(buf, 0, MAX_LINE_LENGTH);

        read_value = read(0, buf, MAX_LINE_LENGTH);

        //part responsible for CNTR-D end ENTER
        if (read_value == 0) {
            return 0;
        }
        if (read_value == 1) {
            continue;
        }


        if (buf[read_value - 1] != '\n') {
            while (1) {
                read_value = read(0, buf, MAX_LINE_LENGTH);
                if(buf[read_value-1] == '\n') {
                    break;
                }
            }
            fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
            continue;
        }

		ln = parseline(buf);
        if (ln == NULL) {
            fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
        }
		//printparsedline(ln);

        child_pid = fork();
        if (child_pid == 0) {
            break;
        }
        else{
            waitpid(child_pid, &status, 0);
        }

	}

    //
    return run_child(ln);
}
