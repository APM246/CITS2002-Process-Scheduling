#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* CITS2002 Project 1 2019
   Name(s):             student-name1 (, student-name2)
   Student number(s):   student-number-1 (, student-number-2)
 */


//  besttq (v1.0)
//  Written by Chris.McDonald@uwa.edu.au, 2019, free for all to copy and modify

//  Compile with:  cc -std=c99 -Wall -Werror -o besttq besttq.c


//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF TRACEFILE CONTENTS (AND HENCE
//  JOB-MIX) THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE
//  CONSTANTS WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES             4
#define MAX_DEVICE_NAME         20
#define MAX_PROCESSES           50
// DO NOT USE THIS - #define MAX_PROCESS_EVENTS      1000
#define MAX_EVENTS_PER_PROCESS	100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

int optimal_time_quantum                = 0;
int total_process_completion_time       = 0;
int optimal_completion_time             = 0;

//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

// --------- DATA STRUCTURES USED TO STORE INFORMATION FROM TRACE FILES ------

char devices[MAX_DEVICES][MAX_DEVICE_NAME];  // STORE DEVICE NAMES
int transfer_rate[MAX_DEVICES];  // STORE TRANSFER RATES [BYTES/SECOND] 
int starting_time[MAX_PROCESSES]; // STORE STARTING TIME OF PROCESSES (SINCE OS REBOOT)
int cumulative_exectime[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]; /* STORE CUMULATIVE EXECUTION TIMES OF EVENTS IN 
EACH PROCESS*/ 
char io_events[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS][MAX_DEVICE_NAME];
int io_data[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int computing_time[MAX_PROCESSES]; //cumulative execution time when processes exit 

int currentProcess = 0; // CURRENT PROCESS AND EVENT BEING ANALYSED
int currentEvent = 0;
int readyQueue[MAX_PROCESSES] = {1}; // keeps track of processes in ready queue, 1st process is already added 
int previous = 1; // most recent process that completed a time quantum (or requested I/O or exited)

void parse_tracefile(char program[], char tracefile[])
{
//  ATTEMPT TO OPEN OUR TRACEFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp    = fopen(tracefile, "r");

    if(fp == NULL) {
        printf("%s: unable to open '%s'\n", program, tracefile);
        exit(EXIT_FAILURE);
    }

    char line[BUFSIZ];
    int  lc     = 0;

//  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while(fgets(line, sizeof line, fp) != NULL) {
        ++lc;

//  COMMENT LINES ARE SIMPLY SKIPPED
        if(line[0] == CHAR_COMMENT) {
            continue;
        }

//  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        char    word0[MAXWORD], word1[MAXWORD], word2[MAXWORD], word3[MAXWORD];
        int nwords = sscanf(line, "%s %s %s %s", word0, word1, word2, word3);

//      printf("%i = %s", nwords, line);

//  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if(nwords <= 0) {
            continue;
        }
//  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS
        if(nwords == 4 && strcmp(word0, "device") == 0) {
            strcpy(devices[lc-1],word1); 
            transfer_rate[lc-1] = atoi(word2);
        }

        else if(nwords == 1 && strcmp(word0, "reboot") == 0) {
            ;   // DO NOTHING
        }

        else if(nwords == 4 && strcmp(word0, "process") == 0) {
            starting_time[atoi(word1) - 1] = atoi(word2); // FOUND THE START OF A PROCESS'S EVENTS
        }

        else if(nwords == 4 && strcmp(word0, "i/o") == 0) {
            cumulative_exectime[currentProcess][currentEvent] = atoi(word1);   
            strcpy(io_events[currentProcess][currentEvent], word2);
            io_data[currentProcess][currentEvent] = atoi(word3);
            currentEvent++;
            
            //  AN I/O EVENT FOR THE CURRENT PROCESS
        }

        else if(nwords == 2 && strcmp(word0, "exit") == 0) {
            currentProcess++;
            currentEvent = 0;
            computing_time[lc-1] = atoi(word1);   //  PRESUMABLY THE LAST EVENT WE'LL SEE FOR THE CURRENT PROCESS
        }

        else if(nwords == 1 && strcmp(word0, "}") == 0) {
            ;   //  JUST THE END OF THE CURRENT PROCESS'S EVENTS
        }
        else {
            printf("%s: line %i of '%s' is unrecognized",
                        program, lc, tracefile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}

#undef  MAXWORD
#undef  CHAR_COMMENT

//  ----------------------------------------------------------------------

int toAdd = 2; //next process waiting to be added to ready queue for the first time

void sortQueue(int currentTime, int queue[])
{
    if (currentProcess == 1) 
    {
        if (computing_time[0] <= 0) queue[0] = 0;
        return;
    }

    else 
    {
        previous = queue[0]; 
        for (int i = 0; i < currentProcess - 1; i++)
        {
            queue[i] = queue[i+1];
        }

        if (currentTime >= starting_time[toAdd])
        {
            queue[currentProcess - 1] = toAdd;
            toAdd++;
            queue[currentProcess] = previous;
        }

        else if (computing_time[previous] > 0)
        {
            queue[currentProcess - 1] = previous;
        }
        else queue[currentProcess - 1] = 0;
    }
}

bool isFinished(int queue[])
{
    for (int i = 0; i < currentProcess; i++)
    {
        if (queue[i] != 0) return false;
    }

    return true; //ready queue is empty 
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    total_process_completion_time = TIME_CONTEXT_SWITCH; // 5 microseconds at start added

    while (!isFinished(readyQueue))
    {
        if (readyQueue[0] != previous) total_process_completion_time += TIME_CONTEXT_SWITCH;
        total_process_completion_time += time_quantum;
        computing_time[readyQueue[0] - 1] -= time_quantum;
        sortQueue(starting_time[0] + total_process_completion_time, readyQueue);
    }
}

//  ----------------------------------------------------------------------

void usage(char program[])
{
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

int main(int argcount, char *argvalue[])
{
    int TQ0 = 0, TQfinal = 0, TQinc = 0;

//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if(argcount == 5) {
        TQ0     = atoi(argvalue[2]);
        TQfinal = atoi(argvalue[3]);
        TQinc   = atoi(argvalue[4]);

        if(TQ0 < 1 || TQfinal < TQ0 || TQinc < 1) {
            usage(argvalue[0]);
        }
    }
//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if(argcount == 3) {
        TQ0     = atoi(argvalue[2]);
        if(TQ0 < 1) {
            usage(argvalue[0]);
        }
        TQfinal = TQ0;
        TQinc   = 1;
    }
//  CALLED INCORRECTLY, REPORT THE ERROR AND TERMINATE
    else {
        usage(argvalue[0]);
    }

//  READ THE JOB-MIX FROM THE TRACEFILE, STORING INFORMATION IN DATA-STRUCTURES
    parse_tracefile(argvalue[0], argvalue[1]);

    //---------------------------

    /*for (int i = 0; i < MAX_PROCESSES; i++)
    {
        printf("\n");
        printf("%i, ", readyQueue[i]);
        printf("\n\n");
    }*/

   //-----------------------------

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
//  WE NEED TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
//  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED

// Simulate with TQ0 first then compare with subsequent TQ values. 
    simulate_job_mix(TQ0);
    optimal_time_quantum = TQ0;
    optimal_completion_time = total_process_completion_time;

    for(int time_quantum=TQ0 ; time_quantum<=TQfinal ; time_quantum += TQinc) {
        simulate_job_mix(time_quantum);
        if (total_process_completion_time < optimal_completion_time)
        {
            optimal_completion_time = total_process_completion_time;
            optimal_time_quantum = time_quantum;
        }
        total_process_completion_time = 0; //reset value
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, optimal_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
