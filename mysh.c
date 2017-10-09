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
        int needPipe = FALSE;
        int pipeAccess[2];
        int fileDesc;
        char fileName[128];
        commandHistory++;
        char *redirCommands[MAX_LINE];
        int stdInput = dup(STDIN_FILENO);
        int stdOut = dup(STDOUT_FILENO);


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
            //TODO: Add error handling when there are too many args
            if(inRedir == TRUE || outRedir == TRUE){
                //have to send different things to the execvp command in the child


                //if we are sending out, we have to find the 2 things before the >
                if(outRedir == TRUE){
                    for(int i = 0; i < index; i++){
                        redirCommands[i] = toks[i];
                    }
                    //file name will be at index++
                    strcpy(fileName, toks[index + 1]);
                    fileDesc = open(fileName, O_WRONLY | O_CREAT | O_TRUNC);
                    if(fileDesc < 0){
                        printError();
                        continue;
                    }
                    close(1); //close stdout and reassing w/ dup2
                    dup2(fileDesc, 1);
                }
                else{
                    for(int i = index + 1; i < __numArgs; i++){
                        redirCommands[i - index - 1] = toks[i];
                    }
                    strcpy(fileName, toks[index - 1]);
                    fileDesc = open(fileName, O_RDONLY);
                    close(0); //close the input
                    dup2(fileDesc, 0);
                }
            }
            /*

            needPipe = TRUE;
            if (pipe(pipeAccess) == -1) {
                printError();
                continue;
            }
             */

            //if there is a pipe, nothing is displayed in the console
            int pid = fork();
            if (pid == 0) {
                if(needPipe == TRUE){
                    close(pipeAccess[0]);
                    dup2(pipeAccess[1], 1); //stdout -> pipe
                    dup2(pipeAccess[1], 2); //stderr -> pipe

                    close(pipeAccess[1]);
                }

                if(inRedir == TRUE || outRedir == TRUE) execvp(redirCommands[0], redirCommands);
                execvp(toks[0], toks);

            } else if (pid < 0) {
                printError();
                continue;
            } else {
                //parent
                if(needPipe == FALSE){
                    wait(pid);
                }
                else if (needPipe == TRUE) {
                    char outBuff[1000];
                    close(pipeAccess[1]); //close write end in parent
                    while (read(pipeAccess[0], outBuff, sizeof(outBuff)) != 0) {
                        write(STDIN_FILENO, outBuff, sizeof(outBuff));
                    }
                }
            }

        }
        //TODO: Remove the break so it actually works
        //TODO: Solve issue where if you do commands really fast, "An error has occured" appears next to mysh
        close(0);
        close(1);
        dup2(stdInput, 0);
        dup2(stdOut, 1);
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


void printError(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}