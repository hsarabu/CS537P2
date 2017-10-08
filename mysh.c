#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LINE 128
#define BIN_DIR "/bin"

static void getLine(char *input, char **toks, char *tok, int __numargs);
static void printError();
static int execute(char **toks, int __numargs);
int main() {
    int commandHistory = 0;

    while(1){
        commandHistory++;
        printf("mysh (%i)> ", commandHistory);
        char *toks[MAX_LINE];
        char *tok;
        char buff[10000];
        if(fgets(buff, MAX_LINE, stdin) == NULL){
            printError();
            continue;
        }
        //format the command
        int __numArgs = 0;
        getLine(buff, toks, tok, __numArgs);

        //doing built ins
        //breaking upon "exit"
        if(strcmp(toks[0], "exit") == 0){
            if(toks[1] != NULL){
                printError();
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
        }
        else if(execute(toks, __numArgs) == 1){
            continue;
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

        fflush(stdout);
        //TODO: Remove the break so it actually works
    }

    //upon exit we need to kill background tasks
    return 0;
}

void getLine(char *input, char **toks, char *tok, int __numargs){
    tok = strtok(input, " \t\n");
    int index = 0;
    while(tok != NULL){
        toks[index] = tok;
        tok = strtok(NULL, " \t\n");
        index++;
    }
    toks[index] = NULL; //null terminate
    __numargs = index;
}

int execute(char **toks, int __numargs){
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

        if(execvp(toks, __numargs)){
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