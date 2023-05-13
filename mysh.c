#include <stdio.h> // for IO
#include <stdlib.h>// for memory allocation
#include <string.h> // for string functions
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/types.h> // for pipes
#include <sys/wait.h> // for wait

// Global Variables
int pipeInput = 0;
int childPipedOutput = 0;

char * parseToken(char* prompt, int * promptIndex)
{
	//printf("Entering parseToken\n");
	char * token = malloc(sizeof(char) * 100);
	int tokenIndex = 0;
	if(prompt[*promptIndex] != '"' && prompt[*promptIndex] != '\'')
	{
		while(*promptIndex < strlen(prompt) && prompt[*promptIndex] != ' ')
		{
			if(prompt[*promptIndex] != '\n')
			{
				//printf("WRITING CHAR:%c\n",prompt[*promptIndex]);
				// store the current character and advance the index
				token[tokenIndex++] = prompt[*promptIndex];
			}
			*promptIndex+=1;
		}
	}
	else if(prompt[*promptIndex] == '"')
	{
		// Move past first quotation mark
		*promptIndex+=1;
		while(*promptIndex < strlen(prompt) && prompt[*promptIndex] != '"')
		{
			if(prompt[*promptIndex] != '\n')
			{
				//printf("WRITING CHAR:%c\n",prompt[*promptIndex]);
				// store the current character and advance the index
				token[tokenIndex++] = prompt[*promptIndex];
			}
			*promptIndex+=1;
		}
		// Move past the last quotation mark
		*promptIndex+=1;
	}
	else
	{
		// Move past first quotation mark
		*promptIndex+=1;
		while(*promptIndex < strlen(prompt) && prompt[*promptIndex] != '\'')
		{
			if(prompt[*promptIndex] != '\n')
			{
				//printf("WRITING CHAR:%c\n",prompt[*promptIndex]);
				// store the current character and advance the index
				token[tokenIndex++] = prompt[*promptIndex];
			}
			*promptIndex+=1;
		}
		// Move past the last quotation mark
		*promptIndex+=1;
	}
	
	// Manually null terminate this string
	token[tokenIndex] = '\0';
	//printf("Exiting parseToken\n");
	return token;
}

int getNextTokenIndex(char* prompt, int * tokenIndex)
{
	//printf("Entering getNextTokenIndex\n");
	//printf("Token Index: %d, prompt Length: %ld", *tokenIndex, strlen(prompt));
	if(*tokenIndex < strlen(prompt))
	{
		// Advance prompt_index to the beginning of the command or past the end of the prompt string
		while(prompt[*tokenIndex] == ' ' || prompt[*tokenIndex] == '\n')
		{
			*tokenIndex+=1;
		}	
	}
	//printf("Exiting getNextTokenIndex\n");
}

void parse(char* prompt, int prompt_index, int in_fd) 
{
	//printf("LENGTH OF PROMPT: %lu\n", strlen(prompt));
	int numArgs = 1;
	int argIndex = 0;
	char ** args;
	char * arg;
	char * command;
	char * redirectInputFile;
	char * redirectOutputFile;
	int redirectInput = 0;
	int redirectOutput = 0;
	int pipeOutput = 0;
	int fd_redirectionInput;
	int fd_redirectionOutput;
	int fd_pipe[2];
	
	//if the last child proccess piped its output, prepare to take it's results as input using in_fd
	if(childPipedOutput)
	{
		pipeInput = 1;
		//Reset childPipedOutput
		childPipedOutput = 0;
	}
	
	// Get the starting index of the first token (assuming its command)
	getNextTokenIndex(prompt, &prompt_index);
	command = parseToken(prompt, &prompt_index);
	//printf("Parsed Command:%s\n",command);
	//printf("PROMPT INDEX After PArsing Command: %d\n", prompt_index);
	//Increment number of args
	numArgs +=1;
	// Get the next non-whitespaceCharacter
	getNextTokenIndex(prompt, &prompt_index);
	//printf("PROMPT INDEX: %d\n", prompt_index);
	argIndex = prompt_index;
	
	//Count the number of arguments in the prompt
	while(prompt_index < strlen(prompt) && prompt[prompt_index] != '|' && prompt[prompt_index] != '<' && prompt[prompt_index] != '>')
	{
		arg = parseToken(prompt, &prompt_index);
		//printf("Arg to be Added: %s\n",arg);
		getNextTokenIndex(prompt, &prompt_index);
		numArgs += 1;
	}
	
	// Create Array of args
	args = (char**)calloc(numArgs, sizeof(char*));
	// add comand
	char path[20] = "/usr/bin/";
	strcat(path,command);
	command = path;
	args[0] = calloc(50, sizeof(char));
	args[0] = command;
	int i = 1;
	
	//Build our Argument Array
	while(argIndex < strlen(prompt) && prompt[argIndex] != '|' && prompt[argIndex] != '<' && prompt[argIndex] != '>')
	{
		arg = parseToken(prompt, &argIndex);
		//printf("ADDING ARGUMENT: %s\n", arg);
		args[i] = calloc(50, sizeof(char));
		args[i] = arg;
		i++;
		getNextTokenIndex(prompt, &argIndex);	
	}
	
	// Add Null terminated parameter
	args[i] = NULL;
	
	// Pipe Operator
	if(prompt[prompt_index] == '|')
	{
		prompt_index +=1;
		pipeOutput = 1;
		childPipedOutput=1;
		
	}
	// Prepare the redirection Input
	if(prompt[prompt_index] == '<')
	{
		prompt_index +=1;
		getNextTokenIndex(prompt, &prompt_index);
		redirectInputFile = parseToken(prompt, &prompt_index);
		redirectInput = 1;
		getNextTokenIndex(prompt, &prompt_index);
		
	}
	// Prepare the Redirection Output
	if(prompt[prompt_index] == '>')
	{
		prompt_index +=1;
		getNextTokenIndex(prompt, &prompt_index);
		redirectOutputFile = parseToken(prompt, &prompt_index);
		redirectOutput = 1;
		getNextTokenIndex(prompt, &prompt_index);
	}
	
	pipe(fd_pipe);
	//printf("fd_pipe[0]: %d\nfd_pipe[1]: %d\nin_fd: %d\n", fd_pipe[0],fd_pipe[1],in_fd);
	int pid = fork();
	if(pid == 0)
	{
		//printf("\tIN CHILD PROCESS\n");
		//close(fd_pipe[0]);
		if(redirectInput)
		{
			//printf("\tREDIRECTING INPUT\n");
			// Open the numbers.txt file. Give user permision to r/w/e
			fd_redirectionInput = open(redirectInputFile, O_RDONLY, S_IRWXU);
			dup2(fd_redirectionInput, STDIN_FILENO);
			close(fd_redirectionInput);
		}
		else if(pipeInput)
		{
			//printf("\tPIPE INPUT\n");
			dup2(in_fd, STDIN_FILENO);
			close(in_fd);
		}
		if(redirectOutput)
		{
			//printf("\tREDIRECTING OUTPUT\n");
			// Open the numbers.txt file. Give user permision to r/w/e
			//printf("\tFile output Name: %s\n", redirectOutputFile);
			fd_redirectionOutput = open(redirectOutputFile, O_WRONLY | O_CREAT, S_IRWXU);
			//printf("\tFile output FD: %d\n", fd_redirectionOutput);
			dup2(fd_redirectionOutput, STDOUT_FILENO);
			close(fd_redirectionOutput);
		}
		else if(pipeOutput)
		{
			//printf("\tPIPE OUTPUT\n");
			dup2(fd_pipe[1], STDOUT_FILENO);
		}
		
		
		execv(args[0], args);
		printf("EXECV FAILED\n");
	}
	else
	{
		wait(NULL);
		//printf("IN PARENT PROCESS\n");
		//printf("fd_pipe[0]: %d\nfd_pipe[1]: %d\nin_fd: %d\n", fd_pipe[0],fd_pipe[1],in_fd);
		close(fd_pipe[1]);
		//close(in_fd);
		//printf("fd_pipe[0]: %d\nfd_pipe[1]: %d\nin_fd: %d\n", fd_pipe[0],fd_pipe[1],in_fd);
		if(prompt_index < strlen(prompt))
		{
			//printf("ENTERING NEXT PARSE\n");
			parse(prompt,prompt_index, fd_pipe[0]);
		}
		//printf("EXITING PARSE\n");
	}
}

void testParse()
{
	//printf("---START testParse:\n");

	char * prompt;
	
	//prompt = " cat country.txt city.txt | egrep 'g' | sort | more > countryCitygSorted.txt";
	//prompt = "cat country.txt city.txt | more";
	//prompt = "ls | sort";
	//prompt = "cat country.txt city.txt | sort | sort | more";
	//prompt = "cat country.txt city.txt | egrep 'g' | more";
	//prompt = "cat country.txt city.txt | egrep 'g' | sort | more";
	//prompt = "egrep 'g' | more";
	//prompt = "cat country.txt city.txt | sort | more > test.txt";
	//prompt = "sort < city.txt > test2.txt";
	//prompt = "egrep g test.txt > test3.txt";
	//prompt = "cat country.txt city.txt | tail | sort -r | wc > test4.txt";
	//prompt = "ls";
	
	//printf("PROMPT: %s\n", prompt);
	parse(prompt,0, STDIN_FILENO);
	
	//printf("---END testParse:\n");
}

void runTests()
{
	testParse();
}

int run()
{
	int exit = -1;
	int MAX_PROMPT_LENGTH = 250;
	char prompt[MAX_PROMPT_LENGTH];
	printf("MyShPrompt> ");
	
	//Get command from STDIN
	fgets(prompt,MAX_PROMPT_LENGTH,stdin);
	
	//printf("prompt:%s\n", prompt);
	if(strcmp(prompt, "exit\n") == 0)
	{
		exit = 1;
	}
	else
	{
		//Begin Parsing
		parse(prompt,0,STDIN_FILENO);
	}
	
	return exit;
}

int main()
{
	for(int i = 0; i < 1; i++)
	{
		i += run();	
	}
}


