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
#include <signal.h>
#include <errno.h>


#define INPUT_LENGTH 2048
#define MAX_ARGS      512
volatile sig_atomic_t fg_only_mode = 0;  // 0 = normal, 1 = foreground-only

struct command_line
{
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

void handle_SIGTSTP(int signo) {
    if (fg_only_mode == 0) {
        char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
        write(STDOUT_FILENO, message, strlen(message));
        fg_only_mode = 1;
    } else {
        char* message = "\nExiting foreground-only mode\n: ";
        write(STDOUT_FILENO, message, strlen(message));
        fg_only_mode = 0;
    }
    fflush(stdout);
}

struct command_line *parse_input()
{
    char input[INPUT_LENGTH];
    struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);

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
    struct sigaction ignore_action = {0};
    int childStatus;
    bool has_child_status = false;
    pid_t spawnpid;

    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    while(true)
    {
        while ((spawnpid = waitpid(-1,&childStatus,WNOHANG )) > 0)
        {
            printf("background pid %d is done: ", spawnpid);
            if (WIFEXITED(childStatus) == true)
            {
                printf("exit value %d\n", WEXITSTATUS(childStatus));
            }
            else if (WIFSIGNALED(childStatus) == true)
            {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
            }
            fflush(stdout);
        }

        curr_command = parse_input();

        if (curr_command->argc == 0 || curr_command->argv[0][0] == '#') 
        {
            continue;
        }

        if (strcmp(curr_command->argv[0], "exit") == 0) {
            free(curr_command);
            break;
        }

        if (strcmp(curr_command->argv[0], "cd") == 0 && curr_command->argc == 1 ) 
        {
            int result = chdir(getenv("HOME"));
            if (result == -1)
            {
                perror("chdir failed");
            }
            continue;
        }
        else if (strcmp(curr_command->argv[0], "cd") == 0 && curr_command->argc == 2 ) 
        {
            int result = chdir(curr_command->argv[1]);
            if (result == -1)
            {
                perror("chdir failed");
            }
            continue;
        }

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

        if (fg_only_mode == 1) {
            curr_command->is_bg = false;
        }

        spawnpid = fork();

        switch (spawnpid)
        {
        case -1:
            perror("fork() failed!");
            exit(1);
            break;

        case 0:
            {
                struct sigaction ignore_action_child = {0};
                ignore_action_child.sa_handler = SIG_IGN;
                sigfillset(&ignore_action_child.sa_mask);
                ignore_action_child.sa_flags = 0;
                sigaction(SIGTSTP, &ignore_action_child, NULL);

                if (curr_command->is_bg) {
                    struct sigaction ignore_INT = {0};
                    ignore_INT.sa_handler = SIG_IGN;
                    sigfillset(&ignore_INT.sa_mask);
                    ignore_INT.sa_flags = 0;
                    sigaction(SIGINT, &ignore_INT, NULL);
                } else {
                    struct sigaction default_INT = {0};
                    default_INT.sa_handler = SIG_DFL;
                    sigfillset(&default_INT.sa_mask);
                    default_INT.sa_flags = 0;
                    sigaction(SIGINT, &default_INT, NULL);
                }

                if (curr_command->input_file != NULL)
                {
                    int inputFD = open(curr_command->input_file, O_RDONLY);
                    if (inputFD == -1)
                    {
                        fprintf(stderr, "cannot open %s for input\n", curr_command->input_file);
                        exit(1);
                    }
                    int result = dup2(inputFD, 0);
                    if (result == -1)
                    {
                        perror("source dup2()");
                        exit(2);
                    }
                    close(inputFD);
                }
                else if (curr_command->is_bg == true)
                {
                    int inputFD = open("/dev/null", O_RDONLY);
                    if (inputFD == -1)
                    {
                        perror("Source open()");
                        exit(1);
                    }
                    int result = dup2(inputFD, 0);
                    if (result == -1 )
                    {
                        perror("source dup2()");
                        exit(2);
                    }
                    close(inputFD);
                }

                if (curr_command->output_file != NULL)
                {    
                    int outputFD = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outputFD == -1)
                    {
                        fprintf(stderr, "cannot open %s for output\n", curr_command->output_file);
                        exit(1);
                    }
                    int result = dup2(outputFD, 1);
                    if (result == -1)
                    {
                        perror("source dup2()");
                        exit(2);
                    }
                    close(outputFD);
                }
                else if (curr_command->is_bg == true)
                {
                    int outputFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outputFD == -1)
                    {
                        perror("Source open()");
                        exit(1);
                    }
                    int result = dup2(outputFD, 1);
                    if (result == -1 )
                    {
                        perror("source dup2()");
                        exit(2);
                    }
                    close(outputFD);
                }

                execvp(curr_command->argv[0], curr_command->argv);

                if (errno == ENOENT)
                {
                    fprintf(stderr, "%s: no such file or directory\n", curr_command->argv[0]);
                }
                else
                {
                    perror("execvp failed");
                }
                exit(1);
            }
            break;

        default:
            if (curr_command->is_bg == true && fg_only_mode == 0)
            {
                printf("background pid is %d\n", spawnpid);
                fflush(stdout);
            }
            else
            {
                spawnpid = waitpid(spawnpid, &childStatus, 0 );
                has_child_status = true;

                if (WIFSIGNALED(childStatus)) 
                {
                    int termSignal = WTERMSIG(childStatus);
                    printf("terminated by signal %d\n", termSignal);
                    fflush(stdout);
                }
            }
            free(curr_command);
            break;
        }
    }

    return EXIT_SUCCESS;
}
