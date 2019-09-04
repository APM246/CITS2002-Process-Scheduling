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

//  ----------------------------------------------------------------------

    // MIN AND MAX FUNCTIONS to compare two values 

#define min(x,y) (x) < (y) ? (x) : (y)
#define max(x,y) (x) < (y) ? (y) : (x)

// --------- DATA STRUCTURES USED TO STORE INFORMATION FROM TRACE FILES ------

char devices[MAX_DEVICES][MAX_DEVICE_NAME];  // STORE DEVICE NAMES
int transfer_rate[MAX_DEVICES];  // STORE TRANSFER RATES [BYTES/SECOND] 
int starting_time[MAX_PROCESSES]; // STORE STARTING TIME OF PROCESSES (SINCE OS REBOOT)

char io_events[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS][MAX_DEVICE_NAME];
int io_data[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int cumulative_exectime[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]; /* STORE CUMULATIVE EXECUTION TIMES OF EVENTS IN 
EACH PROCESS*/ 
int total_exectime[MAX_PROCESSES]; // KEEPS TRACK OF AMOUNT OF TIME EACH PROCESS HAS SPENT ON THE CPU

int totalProcesses = 0; // CURRENT PROCESS AND EVENT BEING ANALYSED. Also number of processes in total.
int readyQueue[MAX_PROCESSES] = {1}; // keeps track of processes in ready queue, 1st process is already added
int blockedQueue[MAX_PROCESSES];
int currentEvent_of_each_process[MAX_PROCESSES];
int previous = 1; // most recent process that completed a time quantum (or requested I/O or exited)
int number_of_exited_processes = 0;
int number_of_active_processes = 1;  // processes currently rotating between ready and running 
// set to 1 automatically (process 1)

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
    int device_counter = 0; //number of devices 
    int n_events = 0; //number of events for each process

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
            strcpy(devices[device_counter],word1); 
            transfer_rate[device_counter] = atoi(word2);
            device_counter++;
        }

        else if(nwords == 1 && strcmp(word0, "reboot") == 0) {
            ;   // DO NOTHING
        }

        else if(nwords == 4 && strcmp(word0, "process") == 0) {
            starting_time[totalProcesses] = atoi(word2); // FOUND THE START OF A PROCESS'S EVENTS
        }

        else if(nwords == 4 && strcmp(word0, "i/o") == 0) {
            cumulative_exectime[totalProcesses][n_events] = atoi(word1);   
            strcpy(io_events[totalProcesses][n_events], word2);
            io_data[totalProcesses][n_events] = atoi(word3);
            n_events++;
            
            //  AN I/O EVENT FOR THE CURRENT PROCESS
        }

        else if(nwords == 2 && strcmp(word0, "exit") == 0) {
            cumulative_exectime[totalProcesses][n_events] = atoi(word1); 
            totalProcesses++;
            n_events = 0; 
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

//  two functions which reset readyQueue and currentEvent_of_each_process 
// for each new simulation of job mix (with TQ varying)

void reset_readyQueue()
{
    readyQueue[0] = 1;

    for (int i = 1; i < MAX_PROCESSES; i++)
    {
        readyQueue[i] = 0;
    }
}

void reset_currentEvent_of_each_process()
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        currentEvent_of_each_process[i] = 1;
    }
}

//  ----------------------------------------------------------------------

int toAdd = 1; //next process waiting to be added to ready queue for the first time

// manages the Ready Queue after each time quantum (or when a process exits/becomes blocked)
void sortQueue(int system_time)
{
	int n_active_processes = number_of_active_processes; //keep copy for later 
	previous = readyQueue[0];

	if (cumulative_exectime[readyQueue[0] - 1][0] <= 0 && number_of_active_processes != 0)
	{
		number_of_active_processes--;
		number_of_exited_processes++; 
		previous = 0;
	}

  
    for (int i = 0; i < totalProcesses - 1; i++)
    {
        int nextvalue;
        if((nextvalue = readyQueue[i+1]) != 0) readyQueue[i] = nextvalue;
    }

    if (system_time >= starting_time[toAdd] && toAdd < totalProcesses) //only add processes if limit has not been reached
     {
            if (number_of_active_processes != 0)
            {
                readyQueue[n_active_processes - 1] = toAdd + 1;
                readyQueue[n_active_processes] = previous;
            }
            else readyQueue[0] = toAdd + 1;
            
            toAdd++;
            number_of_active_processes++;
     }

    else if (cumulative_exectime[previous - 1][0] > 0)
    {
        readyQueue[number_of_active_processes - 1] = previous;
    }

    else readyQueue[number_of_active_processes] = 0;
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    reset_currentEvent_of_each_process();
    total_process_completion_time = TIME_CONTEXT_SWITCH; // 5 microseconds at start added

    while (number_of_exited_processes < totalProcesses) 
    {
        if (number_of_active_processes == 0)
        {
            total_process_completion_time = starting_time[toAdd] - starting_time[0];
        }

        else
        { 
            if (readyQueue[0] != previous) total_process_completion_time += TIME_CONTEXT_SWITCH;
            int executiontime = min(time_quantum, cumulative_exectime[readyQueue[0] - 1][0]);
            total_process_completion_time += executiontime;
            cumulative_exectime[readyQueue[0] - 1][0] -= executiontime;
        }

        sortQueue(starting_time[0] + total_process_completion_time);   
    }
}

// RESET ALL VARIABLES INCLUDING ARRAYS AND SCALAR VARIABLES. THIS FUNCTION IS CALLED
// WHEN A NEW SIMULATION IS RUN (SAME JOB MIX BUT DIFFERENT TIME QUANTUM)
void reset_everything(char program[], char tracefile[])
{
    total_process_completion_time = 0; 
    number_of_exited_processes = 0;
    number_of_active_processes = 1;
    totalProcesses = 0;
    previous = 1;
    toAdd = 1;
    parse_tracefile(program, tracefile);
    reset_currentEvent_of_each_process();
    reset_readyQueue();
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

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
//  WE NEED TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
//  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED

// Simulate with TQ0 first then compare with subsequent TQ values. 
    simulate_job_mix(TQ0);
    optimal_time_quantum = TQ0;
    optimal_completion_time = total_process_completion_time;
    reset_everything(argvalue[0], argvalue[1]);

    for (int time_quantum=TQ0 ; time_quantum<=TQfinal ; time_quantum += TQinc) 
    {
        simulate_job_mix(time_quantum);
        if (total_process_completion_time < optimal_completion_time)
        {
            optimal_completion_time = total_process_completion_time;
            optimal_time_quantum = time_quantum;
        }

        // RESET ALL RELEVANT VARIABLES [arrays, scalar variables, etc]
        reset_everything(argvalue[0], argvalue[1]);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, optimal_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4 
