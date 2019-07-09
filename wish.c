#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#define MAX_SIZE 1024
#define SIZE 16

void shellPrompt();
void writeError();
int  wish_get_args(char ***commands, char *line);
void wish_process_args(char **args, int argCount, char **path, int redirect);
void wish_setup_and_run_args(char ***commands, int commandCount, char **path);

char error_message[30] = "An error has occurred\n";
char *pathString;

int main(int argc, char *argv[]) 
{
	char **path = (char**) malloc(sizeof(char*));
	path[0] = (char*) malloc(sizeof(char) * 512);
	path[0] = "/bin/";
	while(1)
	{
		pathString = strdup(path[0]);
		char ***commands = (char***) malloc(sizeof(char**) * SIZE);
		for (int i = 0; i < SIZE; ++i)
		{
			commands[i] = (char**) malloc(sizeof(char*) * SIZE);
			for (int j = 0; j < SIZE; ++j)
			{
				commands[i][j] = (char*) malloc(sizeof(char) * SIZE);
			}
		}
		path[0] = strdup(pathString);
		int commandCount;
		char *line = NULL;
		size_t length = 0;
		// Interactive Mode.
		if (argc == 1)
		{
			while (1)
			{
				shellPrompt();
				if ((getline(&line, &length, stdin)) == -1)
				{
					exit(0);
				}
				commandCount = wish_get_args(commands, line);
				wish_setup_and_run_args(commands, commandCount, path);
			}
		}
		// Batch Mode.
		else if (argc == 2)
		{
			FILE *file = fopen(argv[1], "r");
			if (file == NULL)
			{
				writeError();
				exit(1);
			}
			while (1)
			{
				if ((getline(&line, &length, file)) == -1)
				{
					exit(0);
				}
				commandCount = wish_get_args(commands, line);
				wish_setup_and_run_args(commands, commandCount, path);
			}
		}
		// Invalid number of args.
		else
		{
			writeError();
			exit(1);
		}
		free(commands);
	}
	free(path);
	return 0;
}

void shellPrompt()
{
	printf("wish> ");
	fflush(stdout);
}

void writeError()
{
	write(STDERR_FILENO, error_message, strlen(error_message));
}

int wish_get_args(char ***commands, char *line)
{
	// Parse for parallel command characters.
	char *commandArray[SIZE];
	int commandCount = 0;
	char *command = strtok(line, "&");
	while (command != NULL)
	{
		commandArray[commandCount] = command;
		commandCount++;
		command = strtok(NULL, "&");
	}
	// Parse '>' and whitespace characters.
	for (int i = 0; i < commandCount; ++i)
	{
		int redirCount = 0;
		int argIndex = 0;
		int validFile = 0;
		char *currCommand = commandArray[i];
		// Check to see if command has multiple redirection ops.
		for (int j = 0; currCommand[j] != '\0'; ++j)
		{
			if (currCommand[j] == '>')
			{
				redirCount++;
			}
		}
		// Actually parse out '>' and whitespace.
		char *redirOp;
		char *token = strtok_r(currCommand, ">", &redirOp);
		while (token != NULL)
		{
			validFile = 0;
			char *whitespace;
			char *token2 = strtok_r(token, " \n\t", &whitespace);
			while (token2 != NULL)
			{
				commands[i][argIndex] = token2;
				argIndex++;
				validFile++;
				token2 = strtok_r(NULL, " \n\t", &whitespace);
			}
			token = strtok_r(NULL, ">", &redirOp);
		}
		// Set NULL after last arg so call to exec() works later on.
		commands[i][argIndex] = NULL;

		// No redirection.
		if (redirCount == 0)
		{
			commands[i][argIndex + 1] = "false";
		}
		// Redirection.
		else if (redirCount == 1 && validFile == 1)
		{
			commands[i][argIndex + 1] = "true";
		}
		// Invalid number of '>' characters.
		else
		{
			writeError();
			return -1;
		}
	}
	return commandCount;
}

void wish_process_args(char **args, int argCount, char **path, int redirect)
{
	// No args. Do nothing.
	if (args[0] == NULL)
	{
		return;	
	}
	// Handle built-in command "path".
	else if (strcmp(args[0], "path") == 0)
	{
		path[0] = strdup("");
		for (int i = 1; args[i] != NULL; ++i)
		{
			char *pathToAdd = strdup(args[i]);
			char *currPath = strdup(path[0]);
			args[i] = strcat(pathToAdd, "/ ");
			path[0] = strcat(currPath, args[i]);
		}
	}
	// Handle built-in command "cd".
	else if (strcmp(args[0], "cd") == 0)
	{
		int retVal;
		if (args[1] != NULL && argCount == 2)
		{
			retVal = chdir(args[1]);
		}
		// Improper use of cd.
		else
		{
			retVal = -1;
		}
		// Check return value of chdir.
		if (retVal == -1)
		{
			writeError();
		}
	}
	// Handle built-in command "exit"
	else if (strcmp(args[0], "exit") == 0)
	{
		if (argCount != 1)
		{
			writeError();
		}
		else
		{
			exit(0);
		}
	}
	// Handle other commands.
	else
	{
		int index = 0;
		char *pathExtensions[SIZE];
		char pathDup[512];
		int i;
		for (i = 0; path[0][i] != '\0'; ++i)
		{
			pathDup[i] = path[0][i];
		}
		pathDup[i] = '\0';
		char *pathExt = strtok(pathDup, " ");
		while (pathExt != NULL)
		{
			pathExtensions[index] = pathExt;
			index++;
			pathExt = strtok(NULL, " ");
		}

		int canExec = 0;
		char *binAbsPath = NULL;
		for (int i = 0; i < index; ++i)
		{
			char storedPathExt[512];
			strcpy(storedPathExt, pathExtensions[i]);
			//strcat(pathExtensions[i], args[0]);
			strcat(storedPathExt, args[0]);
//			if (access(pathExtensions[i], X_OK) == 0)
			if (access(storedPathExt, X_OK) == 0)
			{
				canExec = 1;
				//binAbsPath = pathExtensions[i];
				binAbsPath = storedPathExt;
				break;
			}
		}
		if (canExec)
		{
			int rc = fork();
			// Child.
			if (rc == 0)
			{
				// Handle File redirection.
				if (redirect)
				{
					int fd = open(args[argCount - 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
					// Redirect standard out.
					dup2(fd, 1);
					// Redirect standard error.
					dup2(fd, 2);
					close(fd);
					// Remove redirection factors from args array.
					args[argCount + 1] = NULL;
					args[argCount - 1] = NULL;
					argCount--;
				}
				// Execute binary.
				if (execv(binAbsPath, args) == -1)
				{
					writeError();
					exit(1);
				}
			}
			// Parent.
			else
			{
				rc = (int) wait(NULL);
			}
		}
		else
		{
			writeError();
		}
	}
}

void wish_setup_and_run_args(char ***commands, int commandCount, char **path)
{
	if (commandCount == -1)
	{
		return;
	}
	for (int i = 0; i < commandCount; ++i)
	{	
		// Check for valid redirection.
		int argCount = 0;
		for (int j = 0; commands[i][j] != NULL; ++j)
		{
			argCount++;
		}
		if (strcmp(commands[i][argCount + 1], "false") == 0)
		{
			wish_process_args(commands[i], argCount, path, 0);
		}
		else if (strcmp(commands[i][argCount + 1], "true") == 0)
		{
			wish_process_args(commands[i], argCount, path, 1);
		}
		else
		{
			writeError();	
		}
	}
}
