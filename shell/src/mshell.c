#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "string.h"


int
main(int argc, char *argv[])
{
	pipelineseq * ln;
	command *com;

    //we store information on child process below
    pid_t child_pid;
    int status;

    //storing value of read()
    ssize_t read_value;

	char buf[MAX_LINE_LENGTH];

    //?????????????????????
    memset(buf, 0, MAX_LINE_LENGTH);

	while (1) {
        fprintf(stdout, "%s", PROMPT_STR);
        fflush(stdout);

        read_value = read(0, buf, MAX_LINE_LENGTH);

        //part responsible for CNTR-D end ENTER
        if (read_value == 0) {
            return 0;
        }
        if (read_value == 1) {
            continue;
        }


        if (buf[read_value - 1] != '\n'){
            while (1) {
                read_value = read(0, buf, MAX_LINE_LENGTH);
                if(buf[read_value-1] == '\n'){
                    break;
                }
            }
            fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
            continue;
        }

		ln = parseline(buf);
        if(ln == NULL){
            fprintf(stderr,"%s\n", SYNTAX_ERROR_STR);
        }
		//printparsedline(ln);

        child_pid = fork();
        if(child_pid == 0){
            break;
        }
        else{
            waitpid(child_pid, &status, 0);
        }

	}
    execvP(pickfirstcommand(ln), )


    return 0;
}
