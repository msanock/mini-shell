#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>

#include "builtins.h"
#include "config.h"

int exit_shell (char * []);
int echo (char * []);
int change_directory (char  * []);
int kill_process (char * []);
int list_directory (char * []);
int undefined (char * []);


builtin_pair builtins_table[]={
	{"exit",	&exit_shell},
	{"lecho",	&echo},
	{"lcd",		&change_directory},
	{"lkill",	&kill_process},
	{"lls",		&list_directory},
	{NULL,NULL}
};

fptr is_builtin(char * command) {
    builtin_pair * current_builtin = builtins_table;

    while(current_builtin->name != NULL){
        if (strcmp(current_builtin->name, command) == 0)
            return current_builtin->fun;

        current_builtin++;
    }
    return NULL;
}

int count_args(char * argv[]) {
    int i = 0;

    while (argv[i])
        i++;

    return i;
}

int exit_shell (char  * argv[]) {
    exit(0);
}

int echo (char * argv[]) {

    int i =1;
    if (argv[i]) printf("%s", argv[i++]);
    while  (argv[i])
        printf(" %s", argv[i++]);

    printf("\n");
    fflush(stdout);
    return 0;

}

int change_directory (char  * argv[]) {

    int argc = count_args(argv);
    char * path = getenv("HOME");

    if (argc > 2)
        return BUILTIN_ERROR;


    if (argc == 2)
        path = argv[1];

    return chdir(path);
}


int kill_process (char * argv[]) {

    int argc = count_args(argv);
    int signal = SIGTERM;
    pid_t pid = 0;
    char * c;

    if (argc == 1 || argc > 3)
        return BUILTIN_ERROR;

    if (argc == 2)
        pid = strtol(argv[1], &c, 0);
    else {
        if(argv[1][0] != '-')
            return BUILTIN_ERROR;

        signal = strtol(argv[1]+1, &c, 0);

        if(*c != '\0')
            return BUILTIN_ERROR;

        pid = strtol(argv[2], &c, 0);
    }

    if(*c != '\0')
        return BUILTIN_ERROR;

    return kill(pid, signal);
}

int list_directory (char* argv[]) {

    int argc = count_args(argv);

    char path[PATH_MAX];
    struct dirent * directory;
    DIR * directory_ptr;


    if(argc > 1)
        return BUILTIN_ERROR;

    if(getcwd(path, PATH_MAX) == NULL)
        return BUILTIN_ERROR;

    directory_ptr = opendir(path);

    if (directory_ptr) {
        directory = readdir(directory_ptr);

        while (directory != NULL) {
            if(*(directory->d_name) != '.')
                printf("%s\n", directory->d_name);
            directory = readdir(directory_ptr);
        }
        fflush(stdout);

        closedir(directory_ptr);
    } else
        return BUILTIN_ERROR;

    return 0;
}

int undefined (char * argv[]) {
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
