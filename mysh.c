#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE 128
#define BIN_DIR "/bin"

#define TRUE 0
#define FALSE 1

static int getLine(char *input, char **toks, char *tok);
static void printError();
static int execute(char **toks, int __numargs);



int main() {
    int commandHistory = 0;

    while(1){
        //pseudobooleans for redirection
        int inRedir = FALSE;
        int outRedir = FALSE;
        char fileName[128];
        commandHistory++;
        char *redirCommands[MAX_LINE];


        printf("mysh (%i)> ", commandHistory);
        char *toks[MAX_LINE];
        char *tok;
        char buff[10000];
        if(fgets(buff, MAX_LINE, stdin) == NULL){
            printError();
            continue;
        }
        if(strcmp(buff, "\n") == 0){
            printError();
            continue;
        }
        //format the command
        int __numArgs = 0;
        __numArgs = getLine(buff, toks, tok);

        //doing built ins
        //breaking upon "exit"
        if(strcmp(toks[0], "exit") == 0){
            if(toks[1] != NULL){
                printError();
                continue;
            }
            else break;
        }
        else if (strcmp(toks[0], "pwd") == 0){
            //pwd shouldn't have anything behind it
            if(toks[1] != NULL){
                printError();
                continue;
            }
            else{
                //create buffer
                char *buf2;
                buf2 = malloc(sizeof(char) * 512);
                getcwd(buf2, 512);
                printf("%s\n", buf2);
                free(buf2);
            }
        }
        else if(strcmp(toks[0], "cd") == 0){
            if(toks[1] == NULL) {
                char *home = malloc(sizeof(char) * 512);
                strcpy(home, getenv("HOME"));
                if(chdir(home) != 0){
                    printError();
                    continue;
                }
            }
            else{
                if(toks[2] != NULL){
                    printError();
                    continue;
                }
                if(chdir(toks[1]) != 0){
                    printError();
                    continue;
                }
            }
        }
        /*
        else if(execute(toks, __numArgs) == 1){
            continue;
        }
        */
        else {
            /*
            int pipeAccess[2];
            if (pipe(pipeAccess) == -1) {
                printError();
                continue;
            }
             */

            //look for < and > redirections
            int index = 0;
            while(toks[index] != NULL){
                if(strcmp(toks[index], "<") == 0){
                    inRedir = TRUE;
                    break;
                }
                if(strcmp(toks[index], ">") == 0){
                    outRedir = TRUE;
                    break;
                }
                index++;
            }

            if(inRedir == TRUE || outRedir == TRUE){
                //have to send different things to the execvp command in the child

                //if we are sending out, we have to find the 2 things before the >
                if(outRedir == TRUE){
                    for(int i = 0; i < index; i++){
                        redirCommands[i] = toks[i];
                    }
                    //file name will be at index++
                    strcpy(fileName, toks[index + 1]);
                }
                else{
                    for(int i = index + 1; i < __numArgs; i++){
                        redirCommands[i - index - 1] = toks[i];
                    }
                    strcpy(fileName, toks[index - 1]);
                }
            }

            int pid = fork();
            if (pid == 0) {
                //close(pipeAccess[0]); //closing read

                //dup2(pipeAccess[1], 1); //stdout -> pipe
                //dup2(pipeAccess[1], 2); //stderr -> pipe

                //dup2(pipeAccess[1], STDIN_FILENO);
                //close(pipeAccess[1]);

                if(inRedir == TRUE || outRedir == TRUE) execvp(redirCommands[0], redirCommands);
                execvp(toks[0], toks);

            } else if (pid < 0) {
                printError();
                continue;
            } else {
                //parent
                wait(pid);
                /*
                char outBuff[1000];
                close(pipeAccess[1]); //close write end in parent
                while (read(pipeAccess[0], outBuff, sizeof(outBuff)) != 0) {
                    write(STDIN_FILENO, outBuff, sizeof(outBuff));
                }
                */

            }
        }


        /*
         * Basic structure is as follows:
         * 1) Read in what the user wants up to a 513 char line
         * 2) Do error handling on the input
         * 3) do pid = fork() to create another process
         * 4) if pid > 0, you are in the child. Use pipe() and dup2() to pipe the stdout and error handling of the child to the parent
         * 5) Error handle the fork
         * 6) Process the command if we are using the parent process
         */


        //TODO: Remove the break so it actually works
        //TODO: Solve issue where if you do commands really fast, "An error has occured" appears next to mysh
        fflush(stdout);
    }

    //upon exit we need to kill background tasks
    return 0;
}

int getLine(char *input, char **toks, char *tok){
    tok = strtok(input, " \t\n");
    int index = 0;
    while(tok != NULL){
        toks[index] = tok;
        tok = strtok(NULL, " \t\n");
        index++;
    }
    toks[index] = NULL; //null terminate
    return index;
}

int execute(char **toks, int __numargs){
    //look for redirection
    //if(strcmp(toks[1],">") == 0) outRedir = TRUE;
    //else if (strcmp(toks[1], "<") == 0) inRedir == TRUE;
    int pipeThing[2];
    if(pipe(pipeThing) == -1){
        printError();
        return 1;
    }
    int pid = fork();
    if(pid == 0){
        //in the child process
        close(pipeThing[0]);
        dup2(pipeThing[1], STDIN_FILENO);

        if(execvp(toks[0], toks) < 0){
            //TODO: Error handling
            printf(toks[0],stderr);
        }

    }
    else if (pid < 0){
        //ERROR WITH PID
        printError();
        return 1;
    }
    else{
        wait(pid);
        close(pipeThing[1]);
        dup2(STDIN_FILENO, pipeThing[0]);

    }
    return 0;
}

void printError(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}