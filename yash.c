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
    int ampOrfgOrbgProcess;
} job_t;

job_t *jobArr;
int jobPtr;
job_t *recentlyFinishedJobs;
int RFJPtr;
int yash_pgid;
int ampersandFlag;

void printJobs()
{
    //printf("inside printJobs\n");
    int i;
    for (i = 0; i < jobPtr; i++)
    {
        //printf("runningOrStopped: %d", jobArr[i].state);
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
        printf("[%d] %s %s    %s\n", jobArr[i].jobId, minusOrPlus, runningOrStopped, jobArr[i].jString);
    }
    return;
}

void clearContentsOfStructSlot(int index)
{
    jobArr[index].jobId = 0;
    jobArr[index].state = 0;
    jobArr[index].jString = NULL;

    jobArr[index].pid = 0;
    jobArr[index].pgid = 0;
    //jobArr[jobPtr].argv = NULL;

    jobArr[index].pid2 = 0;
    jobArr[index].pgid2 = 0;
    //jobArr[jobPtr].argv2 = NULL;

    jobArr[index].nProc = 0;
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
            //printf("didn't work\n");
            return 1;
        }
    }
    else
    {
        if (jobPtr == 0)
        {
            jobArr[jobPtr].jobId = 1;
        }
        else
        {
            jobArr[jobPtr].jobId = jobArr[jobPtr - 1].jobId + 1;
        }
        jobArr[jobPtr].state = 2;
        jobArr[jobPtr].jString = strdup(jString);
        jobArr[jobPtr].pid = pid;
        jobArr[jobPtr].pgid = pid;
        // jobArr[jobPtr].argv = (char **)malloc(50 * sizeof(char *));
        // jobArr[jobPtr].argv = arg;
        jobArr[jobPtr].nProc = 1;

        if (strstr(jString, "&") == NULL) //if there is no ampersand flag
        {
            tcsetpgrp(0, pid);
        }
        else
        {
            jobArr[jobPtr].ampOrfgOrbgProcess = 1;
            printf("[%d] %d\n", jobArr[jobPtr].jobId, pid);
        }
        jobPtr++;

        return 0;
    }
    return 0;
}

int performRedirection(char **instruction, char **inOrOut, char **filesToBeRedirected, int sharedSize, char *jString)
{
    //printf("we are in redirect function \n");
    int status;
    int cloneSTDIN = 0;
    int performCloneSTDIN; // equals 1: file exists for STDIN, equals 0: otherwise
    int cloneSTDOUT;
    int performCloneSTDOUT;
    int cloneSTDERR;
    int performCloneSTDERR;

    int i;
    for (i = 0; i < sharedSize; i++)
    {
        if (strcmp(inOrOut[i], ">") == 0)
        {
            //printf("inside of >: FTBR is %s\n", filesToBeRedirected[i]);
            // printf("inOurOut val: %s\n", inOrOut[i]);
            // printf("filesToBeRedirected: %s\n", filesToBeRedirected[i]);
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
            performCloneSTDOUT = 1;
            // else{
            //     dup2(cloneSTDOUT, 1);
            //     close(cloneSTDOUT);
            // }
            //printf("STDOUT -> %d\n", cloneSTDOUT);
        }
        else if (strcmp(inOrOut[i], "<") == 0)
        {
            // printf("inOurOut val: %s\n", inOrOut[i]);
            // printf("filesToBeRedirected: %s\n", filesToBeRedirected[i]);
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
                performCloneSTDIN = 1;
                // dup2(cloneSTDIN, 0);
                // close(cloneSTDIN);
            }
            else
            {
                printf("Sorry, that file doesn't exist.\n");
            }
            //printf("STDIN -> %d", cloneSTDIN);
        }
        else if (strcmp(inOrOut[i], "2>") == 0)
        {
            cloneSTDERR = open(filesToBeRedirected[i], O_WRONLY);
            performCloneSTDERR = 1;
            // dup2(cloneSTDERR, 2);
            // close(cloneSTDERR);
            //printf("STDERR -> %d", cloneSTDERR);
        }
    }

    // printf("instruction[0]: %s\n", instruction[0]);
    // int r = 0;
    // while (instruction[r] != NULL)
    // {
    //     // printf("instruction: %s\n", instruction[r]);
    //     // printf("isAlpha: %d\n", isalpha(instruction[r][0]));
    //     if (isalpha(instruction[r][0]) == 0)
    //     {
    //         instruction[r] = NULL;
    //     }
    //     r++;
    // }

    // int v;
    // for (v = 0; v < sizeof(instruction); v++)
    // {
    //     printf("%d: %s\n", v, instruction[v]);
    // }

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
        // signal(SIGINT, SIG_DFL);
        // signal(SIGTSTP, SIG_DFL);
        // signal(SIGQUIT, SIG_DFL);
        // signal(SIGTTIN, SIG_DFL);
        // signal(SIGTTOU, SIG_DFL);
        if (performCloneSTDIN == 1)
        {
            dup2(cloneSTDIN, 0);
            close(cloneSTDIN);
        }
        if (performCloneSTDOUT == 1)
        {
            dup2(cloneSTDOUT, 1);
            close(cloneSTDOUT);
        }
        if (performCloneSTDERR == 1)
        {
            dup2(cloneSTDERR, 2);
            close(cloneSTDERR);
        }

        if (execvp(instruction[0], instruction) < 0)
        {
            //printf("Didn't work");
            return 0;
        }
    }
    else
    {
        if (jobPtr == 0)
        {
            jobArr[jobPtr].jobId = 1;
        }
        else
        {
            jobArr[jobPtr].jobId = jobArr[jobPtr - 1].jobId + 1;
        }
        jobArr[jobPtr].state = 2;
        jobArr[jobPtr].jString = strdup(jString);
        jobArr[jobPtr].pid = pid;
        jobArr[jobPtr].pgid = pid;
        jobArr[jobPtr].nProc = 1;

        if (strstr(jString, "&") == NULL) //if there is no ampersand flag
        {
            tcsetpgrp(0, pid);
        }
        else
        {
            jobArr[jobPtr].ampOrfgOrbgProcess = 1;
            printf("[%d] %d\n", jobArr[jobPtr].jobId, pid);
        }
        jobPtr++;
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

int performPipeWithRedirection(char **leftChild, char **rightChild, char *jString)
{
    printf("we are in pipe with redirect function\n");
    int pipefd[2];
    int status;
    int done = 0;
    pid_t lcpid;
    pid_t rcpid;

    int i = 0;
    while (leftChild[i] != NULL)
    {
        printf("%s\n", leftChild[i]);
        if (isalpha(leftChild[i][0]) == 0 && strstr(leftChild[i], "<") == NULL && strstr(leftChild[i], ">") == NULL && strstr(leftChild[i], "2>") == NULL)
        {
            leftChild[i] = NULL;
        }
        i++;
    }

    int j = 0;
    while (rightChild[j] != NULL)
    {
        printf("%s\n", rightChild[j]);
        if (isalpha(rightChild[j][0]) == 0 && strstr(rightChild[j], "<") == NULL && strstr(rightChild[j], ">") == NULL && strstr(rightChild[j], "2>") == NULL)
        {
            rightChild[j] = NULL;
        }
        j++;
    }

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
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                if (execvp(instruction[0], instruction) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            // else
            // {
            //     wait(NULL);
            //     free(instruction);
            //     free(inOrOut);
            //     free(filesToBeRedirected);
            //     return 0;
            // }
            free(instruction);
            free(inOrOut);
            free(filesToBeRedirected);
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
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                if (execvp(leftChild[0], leftChild) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            // else
            // {
            //     wait(NULL);
            //     return 0;
            // }
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
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                if (execvp(instruction[0], instruction) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            // else
            // {
            //     wait(NULL);
            //     free(instruction);
            //     free(inOrOut);
            //     free(filesToBeRedirected);
            //     return 0;
            // }
            free(instruction);
            free(inOrOut);
            free(filesToBeRedirected);
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
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                if (execvp(rightChild[0], rightChild) < 0)
                {
                    //printf("Didn't work");
                    return 0;
                }
            }
            // else
            // {
            //     wait(NULL);
            //     return 0;
            // }
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);

    //tcsetpgrp(0, lcpid);
    if (jobPtr == 0)
    {
        jobArr[jobPtr].jobId = 1;
    }
    else
    {
        jobArr[jobPtr].jobId = jobArr[jobPtr - 1].jobId + 1;
    }
    jobArr[jobPtr].state = 2;
    jobArr[jobPtr].jString = strdup(jString);
    jobArr[jobPtr].pid = lcpid;
    //printf("left cpid: %d\n", lcpid);
    jobArr[jobPtr].pgid = lcpid;
    jobArr[jobPtr].pid2 = rcpid;
    //printf("right cpid: %d\n", rcpid);
    jobArr[jobPtr].pgid2 = lcpid;
    jobArr[jobPtr].nProc = 2;

    if (strstr(jString, "&") == NULL) //if there is no ampersand flag
    {
        tcsetpgrp(0, lcpid);
    }
    else
    {
        jobArr[jobPtr].ampOrfgOrbgProcess = 1;
        printf("[%d] %d\n", jobArr[jobPtr].jobId, jobArr[jobPtr].pgid);
    }
    //printf("jString: %s\n", jString);
    jobPtr++;

    // int retVal = waitpid(rcpid, &status, WUNTRACED);

    // tcsetpgrp(0, yash_pgid);
    // waitpid(-1, &status, 0);
    // waitpid(-1, &status, 0);
    return 0;
}

int returnJobIndexByPID(pid_t pid)
{
    int i;
    for (i = 0; i < jobPtr; i++)
    {
        if (jobArr[i].pid == pid || jobArr[i].pid2 == pid)
        {
            return i;
        }
    }
}

void handler(int sig)
{
    int status;
    if (ampersandFlag == 0)
    {
        // int i;
        // for (i = 0; i < jobPtr; i++)
        // {
        //     printf("Job %d: %s", i, jobArr[i].jString);
        // }
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        //printf("pid of pipe: %d\n", pid);
        int returnedJobIndex = returnJobIndexByPID(pid);
        //printf("index of returnedJob: %d\n", returnedJobIndex);
        if (jobArr[returnedJobIndex].nProc == 2)
        {
            if (jobArr[returnedJobIndex].state == 2)
            {
                jobArr[returnedJobIndex].state = 0; //halfway
            }
            else if (jobArr[returnedJobIndex].state == 0)
            {
                jobArr[returnedJobIndex].state = 3;
                tcsetpgrp(0, yash_pgid);
            }
        }
        else
        {
            jobArr[returnedJobIndex].state = 3;
            tcsetpgrp(0, yash_pgid);
        }

        //printf("pid inside handler: %d\n", jobArr[returnedJobIndex].pid);
        if (WIFEXITED(status))
        {
            //normal exit
            //printf("after -- in WIFEXITED: %d\n", jobPtr);
            //printf("done executing\n");
            // printf("normal exit jobPtr after clear: %d\n", jobPtr);
            // printf("returnedJobIndex: %d\n", returnedJobIndex);
            // if (jobArr[returnedJobIndex].nProc == 1)
            // {
            //     free(jobArr[returnedJobIndex].argv);
            // }
            // else
            // {
            //     free(jobArr[returnedJobIndex].argv);
            //     free(jobArr[returnedJobIndex].argv2);
            // }
            if (jobArr[returnedJobIndex].state == 3)
            {
                if (strstr(jobArr[returnedJobIndex].jString, "&") != NULL)
                {
                    recentlyFinishedJobs[RFJPtr] = jobArr[returnedJobIndex];
                    //printf("RFJ %d: %s\n", RFJPtr, recentlyFinishedJobs[RFJPtr].jString);
                    RFJPtr++;
                }

                // printf("normal exit jobPtr before clear: %d\n", jobPtr);
                if (returnedJobIndex == jobPtr - 1)
                {
                    //printf("clearContents block: %d vs %d\n", returnedJobIndex, jobPtr - 1);
                    clearContentsOfStructSlot(returnedJobIndex);
                }
                else
                {
                    int i;
                    for (i = returnedJobIndex; i < jobPtr - 1; i++)
                    {
                        // jobArr[i].jobId = jobArr[i + 1].jobId;
                        // jobArr[i].state = jobArr[i + 1].state;
                        // jobArr[i].jString = jobArr[i + 1].jString;

                        // jobArr[i].pid = jobArr[i + 1].pid;
                        // jobArr[i].pgid = jobArr[i + 1].pgid;
                        // //add args1 here

                        // jobArr[i].pid2 = jobArr[i + 1].pid2;
                        // jobArr[i].pgid2 = jobArr[i + 1].pgid2;
                        // //add args2 here

                        // jobArr[i].nProc = jobArr[i + 1].nProc;
                        // jobArr[i].ampOrfgOrbgProcess = jobArr[i + 1].ampOrfgOrbgProcess;
                        jobArr[i] = jobArr[i + 1];
                        // printf("jobArr[i].jString: %s", jobArr[i].jString);
                    }
                }
                jobPtr--;
            }
        }
        else if (WIFSIGNALED(status))
        {
            // printf("after -- in WIFSIGNALED: %d\n", jobPtr);
            // printf("you did a ^C\n");
            //printf("^C jobPtr before clear: %d", jobPtr);
            // if (jobArr[returnedJobIndex].nProc == 1)
            // {
            //     free(jobArr[returnedJobIndex].argv);
            // }
            // else
            // {
            //     free(jobArr[returnedJobIndex].argv);
            //     free(jobArr[returnedJobIndex].argv2);
            // }
            clearContentsOfStructSlot(returnedJobIndex);
            jobPtr--;
            //printf("^C jobPtr after clear: %d", jobPtr);
        }
        else if (WIFSTOPPED(status))
        {
            //printf("you did a ^Z\n");
            jobArr[returnedJobIndex].state = 1;
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
    recentlyFinishedJobs = (job_t *)malloc(20 * sizeof(job_t));
    RFJPtr = 0;
    jobPtr = 0;
    while (1)
    {
        while (tcgetpgrp(0) != yash_pgid)
        {
            //printf("hi");
        }
        ampersandFlag = 0;
        char name[128];
        char **arguments = (char **)malloc(100 * sizeof(char *));
        int index = 1;
        printf("# ");
        if (fgets(name, 128, stdin) == 0)
        {
            free(arguments);
            break;
        }
        printf("name: %s", name);
        if (strcmp(name, "\n") == 0)
        {
            free(arguments);
            continue;
        }

        int r;
        //printf("RFJPTR: %d\n", RFJPtr);
        for (r = 0; r < RFJPtr; r++)
        {
            printf("[%d] - Done    %s\n", recentlyFinishedJobs[r].jobId, recentlyFinishedJobs[r].jString);
        }
        RFJPtr = 0;

        if (strcmp(name, "fg\n") == 0)
        {
            int tempStatus;
            //printf("we are in fg");
            if (jobPtr == 0) //nothing in jobArr
            {
                continue;
            }
            else
            {
                job_t job = jobArr[jobPtr - 1];
                printf("%s", job.jString);
                //jobPtr--;
                printf("jobstring: %s", job.jString);
                tcsetpgrp(0, job.pgid);
                kill(job.pgid, SIGCONT);
                waitpid(-1, &tempStatus, WUNTRACED);
                printf("after wait");
                tcsetpgrp(0, yash_pgid);
            }
            free(arguments);
            continue;
        }

        if (strcmp(name, "bg\n") == 0)
        {
            int tempStatus;
            //printf("we are in bg");
            if (jobPtr == 0) //nothing in jobArr
            {
                continue;
            }
            else
            {
                job_t job = jobArr[jobPtr - 1];
                printf("[%d] + %s &", job.state, job.jString);
                jobArr[jobPtr - 1].state = 2;
                strcat(jobArr[jobPtr - 1].jString, "&");
                //jobPtr--;
                //printf("jobstring: %s", job.jString);
                //tcsetpgrp(0, job.pgid);
                kill(job.pgid, SIGCONT);
                //waitpid(-1, &tempStatus, WUNTRACED);
                //printf("after wait");
                //tcsetpgrp(0, yash_pgid);
            }
            free(arguments);
            continue;
        }

        char *jString = malloc(sizeof(char *));
        strcpy(jString, name);

        if (strstr(jString, "jobs") != NULL)
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

        printf("%d, %d", index, i);

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

            int retVal = performPipeWithRedirection(leftChild, rightChild, jString);
            //printf("%d\n", retVal);
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
                //printf("instruction: %s\n", instruction[argumentsIndex]);
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

            int retVal = performRedirection(instruction, inOrOut, filesToBeRedirected, sharedIndex, jString);
            //printf("%d\n", retVal);
            free(instruction);
            free(inOrOut);
            free(filesToBeRedirected);
        }
        else
        {
            // printf("right before performExec(jString): %s\n", jString);
            // printf("right before performExec(name): %s\n", name);
            int retVal = performExec(arguments, jString);
            if (retVal == 1)
            {
                tcsetpgrp(0, yash_pgid);
            }
        }

        free(arguments);
        free(jString);
    }

    free(jobArr);
    free(recentlyFinishedJobs);
    return 0;
}