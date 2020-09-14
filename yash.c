#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

typedef struct Job
{
    int jobId;     //set dynamically in printJobs
    int state;     //Running(2), Stopped (1)
    char *jString; //string of job itself

    pid_t pid;
    pid_t pgid;
    char **argv;

    pid_t pid2;
    pid_t pgid2;
    char **argv2;

    int nProc; //number of processes in this process group
} job_t;

job_t *jobArr;
int jobPtr;
int yash_pgid;
int ampersandFlag;

void printJobs()
{
    printf("inside printJobs\n");
    int i;
    for (i = 0; i < jobPtr; i++)
    {
        char *minusOrPlus = "";
        if (i == jobPtr - 1)
        {
            minusOrPlus = "+";
        }
        else
        {
            minusOrPlus = "-";
        }

        char *runningOrStopped = "";
        if (jobArr[i].state == 1)
        {
            runningOrStopped = "Stopped";
        }
        else
        {
            runningOrStopped = "Running";
        }
        printf("[%d] %s %s    %s\n", i + 1, minusOrPlus, runningOrStopped, jobArr[i].jString);
    }
    return;
}

void clearContentsOfStructSlot()
{
    jobArr[jobPtr].jobId = 0;
    jobArr[jobPtr].state = 0;
    jobArr[jobPtr].jString = NULL;

    jobArr[jobPtr].pid = 0;
    jobArr[jobPtr].pgid = 0;
    //jobArr[jobPtr].argv = NULL;

    jobArr[jobPtr].pid2 = 0;
    jobArr[jobPtr].pgid2 = 0;
    //jobArr[jobPtr].argv2 = NULL;

    jobArr[jobPtr].nProc = 0;
}

int performExec(char **arg, char *jString)
{
    int status;
    pid_t pid;
    pid = fork();

    if (pid == -1)
    {
        printf("Could not fork");
        return 0;
    }
    else if (pid == 0)
    {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        if (execvp(arg[0], arg) < 0)
        {
            return 0;
        }
    }
    else
    {
        jobArr[jobPtr].state = 2;
        jobArr[jobPtr].jString = strdup(jString);
        jobArr[jobPtr].pid = pid;
        jobArr[jobPtr].pgid = pid;
        jobArr[jobPtr].nProc = 1;
        jobPtr++;

        if (ampersandFlag == 0) //if there is no ampersand flag
        {
            tcsetpgrp(0, pid); //only do these 2 if no ampersand in instruction
        }
        //int val = waitpid(pid, &status, WUNTRACED);

        return 0;
    }
    return 0;
}

int performRedirection(char **instruction, char **inOrOut, char **filesToBeRedirected, int sharedSize)
{
    printf("we are in redirect function \n");
    int status;
    int cloneSTDIN;
    int dontCloneSTDIN; // equals 1: file exists for STDIN, equals 0: otherwise
    int cloneSTDOUT;
    int cloneSTDERR;

    int i;
    for (i = 0; i < sharedSize; i++)
    {
        if (strcmp(inOrOut[i], ">") == 0)
        {
            //printf("inside of >: FTBR is %s\n", filesToBeRedirected[i]);
            cloneSTDOUT = open(filesToBeRedirected[i], O_WRONLY);
            if (cloneSTDOUT == -1)
            {
                FILE *ptr;
                ptr = fopen(filesToBeRedirected[i], "w");
                fclose(ptr);
                cloneSTDOUT = open(filesToBeRedirected[i], O_WRONLY);
                // dup2(cloneSTDOUT, 1);
                // close(cloneSTDOUT);
            }
            // else{
            //     dup2(cloneSTDOUT, 1);
            //     close(cloneSTDOUT);
            // }
            //printf("STDOUT -> %d\n", cloneSTDOUT);
        }
        else if (strcmp(inOrOut[i], "<") == 0)
        {
            FILE *ptr;
            int exists = -1;
            if (ptr = fopen(filesToBeRedirected[i], "r"))
            {
                fclose(ptr);
                exists = 1;
            }
            else
            {
                exists = 0;
            }
            if (exists == 1)
            {
                cloneSTDIN = open(filesToBeRedirected[i], O_RDONLY);
                // dup2(cloneSTDIN, 0);
                // close(cloneSTDIN);
            }
            else
            {
                printf("Sorry, that file doesn't exist.\n");
                dontCloneSTDIN = 1;
            }
            //printf("STDIN -> %d", cloneSTDIN);
        }
        else if (strcmp(inOrOut[i], "2>") == 0)
        {
            cloneSTDERR = open(filesToBeRedirected[i], O_WRONLY);
            // dup2(cloneSTDERR, 2);
            // close(cloneSTDERR);
            //printf("STDERR -> %d", cloneSTDERR);
        }
    }

    pid_t pid;
    pid = fork();

    if (pid == -1)
    {
        printf("Could not fork");
        return 0;
    }
    else if (pid == 0)
    {
        setpgid(0, 0);
        if (dontCloneSTDIN != 1)
        {
            dup2(cloneSTDIN, 0);
        }
        dup2(cloneSTDOUT, 1);
        dup2(cloneSTDERR, 2);
        close(cloneSTDIN);
        close(cloneSTDOUT);
        close(cloneSTDERR);
        if (execvp(instruction[0], instruction) < 0)
        {
            //printf("Didn't work");
            return 0;
        }
    }
    else
    {
        tcsetpgrp(0, pid); //only do these 2 if no ampersand in instruction
        int val = waitpid(pid, &status, WUNTRACED);

        tcsetpgrp(0, yash_pgid);
        return val;
        // wait(NULL);
        // return;
    }

    return 0;
}

int containsFileRedirectCharacter(char **str)
{
    int index = 0;
    while (str[index] != NULL)
    {
        if (strstr(str[index], "<") != NULL || strstr(str[index], ">") != NULL)
        {
            return 1;
        }
        index++;
    }

    return 0;
}

void arrayParseForPipeRedirects(char **child, char **instruction, char **inOrOut, char **filesToBeRedirected)
{
    int argumentsIndex = 0;
    while (strstr(child[argumentsIndex], "<") == NULL && strstr(child[argumentsIndex], ">") == NULL && strstr(child[argumentsIndex], "2>") == NULL)
    {
        instruction[argumentsIndex] = child[argumentsIndex];
        argumentsIndex++;
    }

    int sharedIndex = 0;
    inOrOut[sharedIndex] = child[argumentsIndex];
    argumentsIndex++;
    while (child[argumentsIndex] != NULL)
    {
        if (strlen(child[argumentsIndex]) == 1)
        {
            inOrOut[sharedIndex] = child[argumentsIndex];
        }
        else
        {
            filesToBeRedirected[sharedIndex] = child[argumentsIndex];
            sharedIndex++;
        }

        argumentsIndex++;
    }

    //replacement for performRedirection(instruction, inOrOut, filesToBeRedirected, sharedIndex)
    int cloneSTDIN;
    int dontCloneSTDIN; // equals 1: file exists for STDIN, equals 0: otherwise
    int cloneSTDOUT;
    int cloneSTDERR;

    int i;
    for (i = 0; i < sharedIndex; i++)
    {
        if (strcmp(inOrOut[i], ">") == 0)
        {
            //printf("inside of >: FTBR is %s\n", filesToBeRedirected[i]);
            cloneSTDOUT = open(filesToBeRedirected[i], O_WRONLY);
            if (cloneSTDOUT == -1)
            {
                FILE *ptr;
                ptr = fopen(filesToBeRedirected[i], "w");
                fclose(ptr);
                cloneSTDOUT = open(filesToBeRedirected[i], O_WRONLY);
                dup2(cloneSTDOUT, 1);
                close(cloneSTDOUT);
            }
            else
            {
                dup2(cloneSTDOUT, 1);
                close(cloneSTDOUT);
            }
            //printf("STDOUT -> %d\n", cloneSTDOUT);
        }
        else if (strcmp(inOrOut[i], "<") == 0)
        {
            FILE *ptr;
            int exists = -1;
            if (ptr = fopen(filesToBeRedirected[i], "r"))
            {
                fclose(ptr);
                exists = 1;
            }
            else
            {
                exists = 0;
            }
            if (exists == 1)
            {
                cloneSTDIN = open(filesToBeRedirected[i], O_RDONLY);
                dup2(cloneSTDIN, 0);
                close(cloneSTDIN);
            }
            else
            {
                printf("Sorry, that file doesn't exist.\n");
                dontCloneSTDIN = 1;
            }
            //printf("STDIN -> %d", cloneSTDIN);
        }
        else if (strcmp(inOrOut[i], "2>") == 0)
        {
            cloneSTDERR = open(filesToBeRedirected[i], O_WRONLY);
            dup2(cloneSTDERR, 2);
            close(cloneSTDERR);
            //printf("STDERR -> %d", cloneSTDERR);
        }
    }

    return;
}

int performPipeWithRedirection(char **leftChild, char **rightChild)
{
    printf("we are in pipe with redirect function\n");
    int pipefd[2];
    int status;
    int done = 0;
    pid_t lcpid;
    pid_t rcpid;

    pipe(pipefd);
    lcpid = fork();
    if (lcpid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);

        if (containsFileRedirectCharacter(leftChild))
        {
            char **instruction = (char **)malloc(100 * sizeof(char *));
            char **inOrOut = (char **)malloc(100 * sizeof(char *));
            char **filesToBeRedirected = (char **)malloc(100 * sizeof(char *));

            arrayParseForPipeRedirects(leftChild, instruction, inOrOut, filesToBeRedirected);
            if (lcpid == -1)
            {
                printf("Could not fork");
                return 0;
            }
            else if (lcpid == 0)
            {
                setpgid(0, 0);
                if (execvp(instruction[0], instruction) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            else
            {
                wait(NULL);
                free(instruction);
                free(inOrOut);
                free(filesToBeRedirected);
                return 0;
            }
        }
        else
        {
            if (lcpid == -1)
            {
                printf("Could not fork");
                return 0;
            }
            else if (lcpid == 0)
            {
                setpgid(0, 0);
                if (execvp(leftChild[0], leftChild) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            else
            {
                wait(NULL);
                return 0;
            }
        }
    }

    rcpid = fork();
    if (rcpid == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);

        if (containsFileRedirectCharacter(rightChild))
        {
            char **instruction = (char **)malloc(100 * sizeof(char *));
            char **inOrOut = (char **)malloc(100 * sizeof(char *));
            char **filesToBeRedirected = (char **)malloc(100 * sizeof(char *));

            arrayParseForPipeRedirects(rightChild, instruction, inOrOut, filesToBeRedirected);
            if (rcpid == -1)
            {
                printf("Could not fork");
                return 0;
            }
            else if (rcpid == 0)
            {
                setpgid(0, lcpid);
                if (execvp(instruction[0], instruction) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            else
            {
                wait(NULL);
                free(instruction);
                free(inOrOut);
                free(filesToBeRedirected);
                return 0;
            }
        }
        else
        {
            if (rcpid == -1)
            {
                printf("Could not fork");
                return 0;
            }
            else if (rcpid == 0)
            {
                setpgid(0, lcpid);
                if (execvp(rightChild[0], rightChild) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            else
            {
                wait(NULL);
                return 0;
            }
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);

    tcsetpgrp(0, lcpid);
    int retVal = waitpid(rcpid, &status, WUNTRACED);

    tcsetpgrp(0, yash_pgid);
    // waitpid(-1, &status, 0);
    // waitpid(-1, &status, 0);
    return retVal;
}

void handler(int sig)
{
    int status;
    if (ampersandFlag == 0)
    {
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        tcsetpgrp(0, yash_pgid);
        printf("inside handler: %d\n", pid);
        if (WIFEXITED(status))
        {
            clearContentsOfStructSlot();
            jobPtr--;
            printf("after -- in WIFEXITED: %d\n", jobPtr);
            printf("done executing\n");
        }
        else if (WIFSIGNALED(status))
        {
            clearContentsOfStructSlot();
            jobPtr--;
            printf("after -- in WIFSIGNALED: %d\n", jobPtr);
            printf("you did a ^C\n");
        }
        else if (WIFSTOPPED(status))
        {
            jobArr[jobPtr].state = 1;
            printf("you did a ^Z\n");
        }
    }

    return;
}

int main()
{
    signal(SIGCHLD, handler);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    setpgid(0, 0); //set process group of main yash shell
    yash_pgid = getpgrp();
    tcsetpgrp(0, yash_pgid);

    jobArr = (job_t *)malloc(20 * sizeof(job_t));
    jobPtr = 0;
    while (1)
    {
        while (tcgetpgrp(0) != yash_pgid)
        {
        }
        char name[100];
        char **arguments = (char **)malloc(100 * sizeof(char *));
        int index = 1;
        printf("# ");
        if (fgets(name, 128, stdin) == 0)
        {
            break;
        }

        char *jString = name;
        int a = strlen(jString) - 1;
        if (jString[a] == '\n')
        {
            jString[a] = '\0';
        }
        if (strcmp(jString, "jobs") == 0)
        {
            printJobs();
            continue;
        }

        char *tk = strtok(name, " ");
        arguments[0] = tk;

        while (tk != NULL)
        {
            //printf("%s", tk);
            tk = strtok(NULL, " ");
            arguments[index] = tk;
            index++;
        }

        int i = 0;
        int redirectFlag = 0;
        int pipeFlag = 0;
        ampersandFlag = 0;
        while (arguments[i] != NULL)
        {
            if (strstr(arguments[i], "<") != NULL || strstr(arguments[i], ">") != NULL || strstr(arguments[i], "2>") != NULL)
            {
                redirectFlag = 1;
            }

            if (strstr(arguments[i], "|") != NULL)
            {
                pipeFlag = 1;
            }

            if (strstr(arguments[i], "&") != NULL)
            {
                ampersandFlag = 1;
                arguments[i] = NULL;
            }

            if (ampersandFlag == 0)
            {
                int a = strlen(arguments[i]) - 1;
                if (arguments[i][a] == '\n')
                {
                    arguments[i][a] = '\0';
                }
            }

            i++;
        }

        if (pipeFlag == 1)
        {
            char **leftChild = (char **)malloc(100 * sizeof(char *));
            char **rightChild = (char **)malloc(100 * sizeof(char *));

            int argumentsIndex = 0;
            while (strstr(arguments[argumentsIndex], "|") == NULL)
            {
                leftChild[argumentsIndex] = arguments[argumentsIndex];
                argumentsIndex++;
            }

            argumentsIndex++;

            int indexOfRightChild = 0;
            while (arguments[argumentsIndex] != NULL)
            {
                rightChild[indexOfRightChild] = arguments[argumentsIndex];
                argumentsIndex++;
                indexOfRightChild++;
            }

            int retVal = performPipeWithRedirection(leftChild, rightChild);
            printf("%d\n", retVal);
            free(leftChild);
            free(rightChild);
        }
        else if (redirectFlag == 1)
        {
            char **instruction = (char **)malloc(100 * sizeof(char *));
            char **inOrOut = (char **)malloc(100 * sizeof(char *));
            char **filesToBeRedirected = (char **)malloc(100 * sizeof(char *));

            int argumentsIndex = 0;
            while (strstr(arguments[argumentsIndex], "<") == NULL && strstr(arguments[argumentsIndex], ">") == NULL && strstr(arguments[argumentsIndex], "2>") == NULL)
            {
                instruction[argumentsIndex] = arguments[argumentsIndex];
                argumentsIndex++;
            }

            int sharedIndex = 0;
            inOrOut[sharedIndex] = arguments[argumentsIndex];
            argumentsIndex++;
            while (arguments[argumentsIndex] != NULL)
            {
                if (strlen(arguments[argumentsIndex]) == 1)
                {
                    inOrOut[sharedIndex] = arguments[argumentsIndex];
                }
                else
                {
                    filesToBeRedirected[sharedIndex] = arguments[argumentsIndex];
                    sharedIndex++;
                }

                argumentsIndex++;
            }

            int retVal = performRedirection(instruction, inOrOut, filesToBeRedirected, sharedIndex);
            printf("%d\n", retVal);
            free(instruction);
            free(inOrOut);
            free(filesToBeRedirected);
        }
        else
        {
            int retVal = performExec(arguments, jString);
        }

        free(arguments);
    }

    free(jobArr);
    return 0;
}