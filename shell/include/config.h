#ifndef _CONFIG_H_
#define _CONFIG_H_

#define MAX_LINE_LENGTH 2048
#define MAX_BUFFER_READ 2049
#define BUFFER_SIZE 4100
#define MAX_ARGS (MAX_LINE_LENGTH/2)


#define SYNTAX_ERROR_STR "Syntax error."
#define BAD_ADDRESS_ERROR_STR ": no such file or directory\n"
#define PERMISSION_ERROR_STR ": permission denied\n"
#define EXEC_ERROR_STR ": exec error\n"
#define BUILTIN_ERROR_STR "Builtin %s error.\n"

#define EXEC_FAILURE 127

#define WRONG_REDIR -1

#define PROMPT_STR "$ "



#endif /* !_CONFIG_H_ */
