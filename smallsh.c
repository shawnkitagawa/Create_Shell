/**
 * A sample program for parsing a command line. If you find it useful,
 * feel free to adapt this code for Assignment 4.
 * Do fix memory leaks and any additional issues you find.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>



#define INPUT_LENGTH 2048
#define MAX_ARGS		 512


struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token){
		if(!strcmp(token,"<")){
			curr_command->input_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,">")){
			curr_command->output_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,"&")){
			curr_command->is_bg = true;
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

int main()
{
	struct command_line *curr_command;
	int childStatus;
	bool has_child_status = false;

	while(true)
	{
		curr_command = parse_input();

		// comments and blank line 
		if (curr_command->argc == 0 || curr_command->argv[0][0] == '#') 
		{
    		continue;
		}

		//may have addition step 
		if (strcmp(curr_command->argv[0], "exit") == 0) {
			free(curr_command);
			break;
		}

		//handle cd

		//when no argument 
		if (strcmp(curr_command->argv[0], "cd") == 0 && curr_command->argc == 1 ) 
		{
			int result = chdir(getenv("HOME"));

			if (result == -1)
			{
				perror("chdir failed");
			}

			continue;
		}

		// when there is argument
		else if (strcmp(curr_command->argv[0], "cd") == 0 && curr_command->argc == 2 ) 
		{
			int result = chdir(curr_command->argv[1]);
			if (result == -1)
			{
				perror("chdir failed");
			}

			continue;
		}
		

		// handle status
		if (strcmp(curr_command->argv[0], "status") == 0) 
		{

			if (has_child_status == false)
			{
				printf("exit value 0\n");
			}
			else if (WIFEXITED(childStatus) == true)
			{
				printf("exit value %d\n", WEXITSTATUS(childStatus));

			}
			else if (WIFSIGNALED(childStatus) == true)
			{
				printf("terminated by signal %d\n", WTERMSIG(childStatus));
			}
			fflush(stdout);
			continue;
		}



		pid_t spawnpid = 5;

		spawnpid = fork();

		switch (spawnpid)
		{
		case -1:
		// case when the fork failed
			perror("fork() failed!");
			exit(1);
			break;

		case 0:
		// case when the child process is running 
			printf("CHILD(%d) running %s command\n", getpid(), curr_command->argv[0]);

			if (curr_command->input_file != NULL)
			{

				// handling input redirection 
				int inputFD = open(curr_command->input_file, O_RDONLY);
				if (inputFD == -1)
				{
					perror("Source open()");
					exit(1);
				}

				printf("File descriptor of input file = %d\n", inputFD); 
				fflush(stdout);

				int result = dup2(inputFD, 0);
				if (result == -1)
				{
					perror("source dup2()");
					exit(2);
				}
				close(inputFD);

			}


			if (curr_command->output_file != NULL)
			{	
				// handling output redirection

				int outputFD = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (outputFD == -1)
				{
					perror("source Open()");
					exit(1);
				}

				printf("File descriptor of output file = %d\n", outputFD); 
				fflush(stdout);

				int result = dup2(outputFD, 1);
				if (result == -1)
				{
					perror("source dup2()");
					exit(2);
				}

				close(outputFD);

			}


				execvp(curr_command->argv[0], curr_command->argv);
				perror("execvp failed");
				exit(1);

			break;
			
			
		default:

			// handle background ( work on this later )
			if (curr_command->is_bg == true)
			{
				printf("background pid is %d\n", spawnpid);
				fflush(stdout);

			}
			else
			{
				spawnpid = waitpid(spawnpid, &childStatus, 0 );
				has_child_status = true;
				if (strcmp(curr_command->argv[0], "status") == 0) 
				{

				}
				printf("PARENT(%d): child(%d) terminated. Now parent is exiting\n", getpid(), spawnpid);


			}
			free(curr_command);
			break;
		}


	}
	return EXIT_SUCCESS;
}