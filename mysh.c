#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 128
#define BIN_DIR "/bin"

#define TRUE 0
#define FALSE 1

static int getLine(char *input, char **toks, char *tok);
static void printError();
static void insertProceess(int process);
static void removeProcess(int process);

int processes[20]; //keep track of 20 background processes

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

        //TODO: Fix the stdout flushing
        printf("mysh (%i)> ", commandHistory);
        fflush(stdout);

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

            //look for < and > redirections and | piping
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
                if(strcmp(toks[index], "|") == 0){
                    needPipe = TRUE;
                    break;
                }
                index++;
            }
            //look for background process &
            int backgroundIndex = 0;
            while(toks[backgroundIndex] != NULL){
                if(strcmp(toks[index], "&") == 0){
                    background = TRUE;
                    toks[index] = NULL;
                }
                backgroundIndex++;
            }

            //TODO: Add error handling when there are too many args
            if(inRedir == TRUE || outRedir == TRUE || needPipe == TRUE){
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
                else if (inRedir == TRUE){
                    for(int i = index + 1; i < __numArgs; i++){
                        redirCommands[i - index - 1] = toks[i];
                    }
                    strcpy(fileName, toks[index - 1]);
                    fileDesc = open(fileName, O_RDONLY);
                    close(0); //close the input
                    dup2(fileDesc, 0);
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
                    close(pipeAccess[0]);
                    dup2(pipeAccess[1], 1); //stdout -> pipe
                    dup2(pipeAccess[1], 2); //stderr -> pipe

                }
                //TODO: Should probably exit if the execvp command returns false or something
                if(inRedir == TRUE || outRedir == TRUE) {
                    execvp(redirCommands[0], redirCommands);
                }
                execvp(toks[0], toks);

            } else if (pid < 0) {
                printError();
                continue;
            } else {
                //parent
                if(needPipe == FALSE){
                    wait(pid);
                }
                else if (background == FALSE) {
                    wait(NULL);
                    insertProceess(pid);
                }
                else {
                    close(pipeAccess[1]); //close write end in parent

                    //need to fork again
                    int child = fork();
                    if(child == 0) {
                        //in the child process
                        dup2(pipeAccess[0], 0); //pipe other stdout to stdin of child
                        close(pipeAccess[0]);
                        execvp(pipe2[0], pipe2);
                    }
                    else{
                        wait(child);
                        kill(pid, SIGINT); //kill parent process. Ignore output, this means the that child is finished
                    }
                }
            }

        }
        //TODO: Remove the break so it actually works
        close(0);
        close(1);
        close(pipeAccess[0]);
        close(pipeAccess[1]);
        fflush(stdout);
        dup2(stdInput, 0);
        dup2(stdOut, 1);
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


void insertProcess(int process) {
    //importing in array of max [2] of ints. 0 is the parent, 1 is child if there are any
    //we need to keep track of the children for each parent in case a child process dies with piping
    for(int i = 0; i < 20; i++){
        //check if the process at i is complete first
        waitpid(processes[i], )
        if(processes[i] == NULL){
            processes[i] = process;
        }
    }
}
