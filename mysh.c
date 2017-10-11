#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 128
#define TRUE 0
#define FALSE 1

static int getLine(char *input, char **toks, char *tok);
static void printError();
static void insertProcess(int process);
static void exitProgram();
static void printPrompt(int counter);

int processes[20]; //keep track of 20 background processes
char error_message[30] = "An error has occurred\n";
int main() {
    int commandHistory = 0;

    while(1){
        //pseudobooleans for redirection
        int inRedir = FALSE;
        int outRedir = FALSE;
        int needPipe = FALSE;
        int background = FALSE;
        int pipeAccess[2];
        int fileDesc;
        char fileName[128];
        commandHistory++;
        char *redirCommands[MAX_LINE]; //using as redir and 1st part ofpip
        char *pipe2[MAX_LINE]; //second part of the pipe
        int stdInput = dup(STDIN_FILENO);
        int stdOut = dup(STDOUT_FILENO);
        fflush(stdout);

        //printf("mysh (%i)> ", commandHistory);
        printPrompt(commandHistory);
        char *toks[MAX_LINE];
        char *tok;
        char buff[10000];
        if(fgets(buff, 10000, stdin) == NULL){
            printError();
            continue;
        }
        if(strcmp(buff, "\n") == 0){
            //printError();
            commandHistory--;
            continue;
        }
        if(strlen(buff) > MAX_LINE){
            printError();
            continue;
        }
        //format the command
        int __numArgs = 0;
        __numArgs = getLine(buff, toks, tok);
        if(__numArgs == 0){
            //printError();
            commandHistory--;
            continue;
        }
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
                free(home);
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

            //look for < and > redirections and | piping
            int index = 0;
            int needContinue = FALSE;
            while(toks[index] != NULL){
                if(strcmp(toks[index], "<") == 0){
                    inRedir = TRUE;
                    if(toks[index+2] != NULL){
                        printError();
                        needContinue = TRUE;
                    }
                    break;
                }
                if(strcmp(toks[index], ">") == 0){
                    outRedir = TRUE;
                    if(toks[index+2] != NULL){
                        printError();
                        needContinue = TRUE;
                    }
                    break;
                }
                if(strcmp(toks[index], "|") == 0){
                    needPipe = TRUE;
                    if(index == 0 || toks[index + 1] == NULL){
                        printError();
                        needContinue = TRUE;
                    }
                    break;
                }
                index++;
            }
            if(needContinue == TRUE) continue;
            //look for background process &
            int backgroundIndex = 0;
            while(toks[backgroundIndex] != NULL){
                if(strcmp(toks[backgroundIndex], "&") == 0){
                    background = TRUE;
                    toks[backgroundIndex] = NULL;
                }
                backgroundIndex++;
            }

            if(inRedir == TRUE || outRedir == TRUE || needPipe == TRUE){
                //have to send different things to the execvp command in the child
                if(outRedir == TRUE || inRedir == TRUE){
                    for(int i = 0; i < index; i++){
                        redirCommands[i] = toks[i];
                    }
                    //file name will be at index++
                    strcpy(fileName, toks[index + 1]);
                    if(outRedir == TRUE){
                        fileDesc = open(fileName, O_WRONLY | O_CREAT | O_TRUNC);
                        if(fileDesc < 0){
                            printError();
                            continue;
                        }
                        close(1); //close stdout and reassing w/ dup2
                        dup2(fileDesc, 1);
                    }
                    else if (inRedir == TRUE){
                        fileDesc = open(fileName, O_RDONLY);
                        if(fileDesc < 0){
                            printError();
                            continue;
                        }
                        close(0); //close the input
                        dup2(fileDesc, 0);
                    }
                }
                else {
                    //piping
                    if (pipe(pipeAccess) == -1) {
                        printError();
                        continue;
                    }
                    for(int i = 0; i < __numArgs; i++){
                        if(i < index){
                            redirCommands[i] = toks[i];
                        }
                        else if(i > index){
                            pipe2[i] = toks[i];
                        }
                    }

                }
            }


            //if there is a pipe, nothing is displayed in the console
            int pid = fork();
            if (pid == 0) {
                if(needPipe == TRUE){
                    close(STDIN_FILENO);
                    close(pipeAccess[0]);
                    dup2(pipeAccess[1], 1); //stdout -> pipe
                    dup2(pipeAccess[1], 2); //stderr -> pipe

                }
                if(inRedir == TRUE || outRedir == TRUE || needPipe == TRUE) {
                    if(execvp(redirCommands[0], redirCommands) == -1){
                        printError();
                        exit(1);
                    }
                }
                else {
                    if (execvp(toks[0], toks) == -1) {
                        printError();
                        exit(1);
                    }
                }

            } else if (pid < 0) {
                printError();
                continue;
            } else {
                //parent
                if (background == TRUE) {
                    insertProcess(pid);
                }
                else if(needPipe == FALSE){
                    int status;
                    if(waitpid(pid, &status, 0) != pid){
                        printError();
                    }
                }
                else {

                    close(pipeAccess[1]); //close write end in parent

                    //need to fork again
                    int child = fork();
                    if(child == 0) {
                        close(STDIN_FILENO);
                        close(pipeAccess[1]);
                        //in the child process
                        dup2(pipeAccess[0], STDIN_FILENO); //pipe other stdout to stdin of child
                        if(execvp(pipe2[0], pipe2) == -1){
                            printError();
                            exit(0);
                        }
                    }
                    else if (child < 0){
                        printError();
                        continue;
                    }
                    else{
                        int status2;
                        waitpid(child, &status2, 0);
                        kill(pid, SIGINT); //kill parent process. Ignore output, this means the that child is finished
                        dup2(STDIN_FILENO, pipeAccess[0]);
                        dup2(STDOUT_FILENO, pipeAccess[1]);
                    }
                }
            }

        }
        //all this closing is probably unnessiary but its thorough
        close(0);
        close(1);
        close(pipeAccess[0]);
        close(pipeAccess[1]);
        fflush(stdout);
        dup2(stdInput, 0);
        dup2(stdOut, 1);
    }

    exitProgram();
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
    write(STDERR_FILENO, error_message, strlen(error_message));
    fflush(stdout);
}


void insertProcess(int process) {
    for(int i = 0; i < 20; i++){
        //check if the process at i is complete first
        if(processes[i] != NULL){
            int arrayStatus;
            if(waitpid(processes[i], &arrayStatus, WNOHANG) != processes[i]){
                //we can evict the existing processID and place the new one
                processes[i] = process;
                break;
            }
        }
        if(processes[i] == NULL){
            processes[i] = process;
            break;
        }
    }
}

void exitProgram(){
    for(int i =0; i < 20; i++){
        if(processes[i] != NULL) {
            kill(processes[i], SIGINT);
        }
    }
}

void printPrompt(int counter){
    char first[] = "mysh (";
    char end[] = ")> ";
    char intBuff[100];
    snprintf(intBuff, sizeof(int), "%i", counter);
    int outputLen = strlen(first) + strlen(end) + strlen(intBuff);
    char output[outputLen];
    strcpy(output, first);
    strcat(output, intBuff);
    strcat(output, end);
    write(STDOUT_FILENO, output, outputLen);
}
