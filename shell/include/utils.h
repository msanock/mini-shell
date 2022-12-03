#ifndef _UTILS_H_
#define _UTILS_H_


#include "siparse.h"
#include "config.h"
//#include "siparse.c"

//struct for storing notes of background processes
typedef struct note {
    int pid;
    int status;
} note;

//buffer for storing input
struct Buffer {
    char buf[BUFFER_SIZE];
    int length;
    char * end_of_command;
    char * begin_new_command;
    char * write_to_buffer_ptr;
} buffer;

//depending on pipeline
int is_background_process;

// process info
pid_t child_pid;

//foreground processes
int foreground_all;
int foreground[100];
int unfinished_foreground;

//status of child process
int status;

//maks of signals
sigset_t set;

//background processes
int finished_background;
note background_notes[100];

// number of bytes read by read function
ssize_t read_value;

void printcommand(command *, int);
void printpipeline(pipeline *, int);
void printparsedline(pipelineseq *);

command * pickfirstcommand(pipelineseq *);

char ** get_command_args (argseq *);

void handle_redirs (redirseq *);

void run_child_process (char **);

void handle_line ();

void handle_pipeline(pipeline *);

int handle_command_in_pipeline(command *, int *, int);


#endif /* !_UTILS_H_ */
