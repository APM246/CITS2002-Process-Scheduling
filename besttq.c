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
#define MILLION                 1000000


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
int currentEvent_of_each_process[MAX_PROCESSES] = {0};
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

// three functions which reset readyQueue, currentEvent_of_each_process and total execution time of each process
// called for each new simulation of job mix (with TQ varying)

void reset_readyQueue(void)
{
    readyQueue[0] = 1;

    for (int i = 1; i < MAX_PROCESSES; i++)
    {
        readyQueue[i] = 0;
    }
}

void reset_blockedQueue(void)
{
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		blockedQueue[i] = 0;
	}
}

void reset_currentEvent_of_each_process(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        currentEvent_of_each_process[i] = 0;
    }
}

void reset_total_exectime(void)
{
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		total_exectime[i] = 0;
	}
}

//  ----------------------------------------------------------------------

// RETURNS EVENT NUMBER OF FINAL EVENT OF A SPECIFIED PROCESS 
int get_final_event(int process)
{
	if (process < 0) return 0;

	for (int i = 0; i < MAX_EVENTS_PER_PROCESS; i++)
	{
		if (cumulative_exectime[process][i] == 0)
		{
			return i - 1;
		}
	}

	return MAX_EVENTS_PER_PROCESS - 1; //final event is at index MAX_EVENTS_PER_PROCESS - 1.
}

bool isEmpty_blockedQueue(void)
{
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		if (blockedQueue[i] != 0) return false;
	}

	return true;
}

// adds a process to the blocked queue. Priority: 
void append_blockedQueue(int currentProcess)
{
	number_of_active_processes--;
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		if (blockedQueue[i] == 0)
		{
			blockedQueue[i] = currentProcess + 1;
			break;
		}
	}
	//currentEvent_of_each_process[currentProcess]++; //process will move onto next I/O event if it has not exited yet
}

int device_number(char device_name[])
{
	int device_number = 0; 

	for (int i = 0; i < MAX_DEVICES; i++)
	{   
		if (strcmp(device_name, devices[i]) == 0)
		{
			device_number = i;
			break;
		}
	}

	return device_number;
}

int transfer__rate(int device_number)
{
	return transfer_rate[device_number];
}

int highest_transferRate; // transfer rate of process about to perform I/O. 

int index; //index of blocked queue process 

// FINDS PROCESS WITH HIGHEST PRIORITY TO PERFORM I/O OPERATIONS
// CLEAN UP INITIALISATION OF HIGHEST TRANSFERRATE (INITIALISE TO ZERO AND COMPARE WITH I STARTING AT 0)
int prioritized_process()
{
	int device;
	//int index;
	int highest_prioritized_process = blockedQueue[0];
	highest_transferRate = transfer__rate(device_number(io_events[blockedQueue[0] - 1][currentEvent_of_each_process[blockedQueue[0] - 1]]));
	int i = 0;

	while (i < MAX_PROCESSES && blockedQueue[i] != 0)
	{
		device = device_number(io_events[blockedQueue[i] - 1][currentEvent_of_each_process[blockedQueue[i] - 1]]);
		if (transfer__rate(device) > highest_transferRate)
		{
			highest_transferRate = transfer__rate(device);
			highest_prioritized_process = blockedQueue[i];
			index = i;
		}
		i++;
	}

	return highest_prioritized_process;  // NEED TO REMOVE PROCESS FROM BLOCKED QUEUE EVENTUALLY AND CLEANUP USING INDEX VARIABLE 
}


/* I/O OPERATIONS PERFORMED HERE. available_time represents the allocated time that the computer can perform io events. 
 available_time is either the time from a process moving to CPU and executing for 
 the appropriate time. If there are no active processes then available_time is set to large value (processes can take
 as much time as they need to perform io). This function calculates which blocked processes will finish their I/O operations in that time. */
void sort_blockedQueue(int available_time)
{
	double timespent = 0;
	int process;

	while (timespent < available_time && !isEmpty_blockedQueue())
	{
		process = prioritized_process();
		int bytes = io_data[process - 1][currentEvent_of_each_process[process - 1]];
		if ((bytes*MILLION/highest_transferRate) < (available_time - timespent))    // CLEAN UP
		{
			timespent += (bytes*MILLION/highest_transferRate);
			io_data[process - 1][currentEvent_of_each_process[process - 1]] -= bytes;
		}
		else
		{
			timespent = available_time - timespent;
			io_data[process - 1][currentEvent_of_each_process[process - 1]] -= ((available_time - timespent) * highest_transferRate)/MILLION;
		}
	
		// REMOVE FROM BLOCKED QUEUE IF NO MORE BYTES TO BE TRANSFERRED 
			if (io_data[process - 1][currentEvent_of_each_process[process - 1]] <= 0)
			{
				// time_context_switch required because process goes from ready to running again
				if (number_of_active_processes == 0) total_process_completion_time += timespent + TIME_CONTEXT_SWITCH;
				blockedQueue[index] = 0;   //REMOVE FROM BLOCKED QUEUE
				readyQueue[number_of_active_processes] = process;
				number_of_active_processes++;
				currentEvent_of_each_process[process - 1]++;
			}
	}
}

int toAdd = 1; //next process waiting to be added to ready queue for the first time

// manages the Ready Queue after each time quantum (or when a process exits/becomes blocked)
void sort_readyQueue(int system_time)    // REPLACE WITH VARIABLES FOR NEATNESS AND ALSO ALL COMMENTS NEED TO BE IN CAPS
{
	int currentProcess = readyQueue[0] - 1; // process that was most recently at front of readyQueue (before it executed)
	int n_active_processes = number_of_active_processes; //keep copy for later 
	int unchanged_currentEvent = currentEvent_of_each_process[currentProcess]; // keep copy for later use (not affected by changes)
	previous = readyQueue[0];
	int finalevent = get_final_event(currentProcess);
	bool ready_to_block = false;

	// WHEN A PROCESS EXITS 
	if (total_exectime[currentProcess] >= cumulative_exectime[currentProcess][finalevent] && number_of_active_processes != 0)
	{
		number_of_active_processes--;
		number_of_exited_processes++; 
		previous = 0;  //process will be removed from readyQueue
	}

	// WHEN A PROCESS IS TO BE BLOCKED (APPENDING TO BLOCKED QUEUE OCCURS AT END OF FUNCTION)
	else if (finalevent != 0 && total_exectime[currentProcess] >= cumulative_exectime[currentProcess][currentEvent_of_each_process[currentProcess]])
	{
		ready_to_block = true; 
		previous = 0;  //process will be removed from readyQueue
	}

	// MOVES EACH PROCESS 1 INDEX DOWN (1 STEP CLOSER TO FRONT OF READY QUEUE)

    for (int i = 0; i < totalProcesses - 1; i++)
    {
        int nextvalue;
        if((nextvalue = readyQueue[i+1]) != 0) readyQueue[i] = nextvalue;
    }

	// THE REMAINING CODE IN THIS FUNCTION DECIDES HOW THE BACK OF THE READY QUEUE SHOULD BE. 
	// PRIORITY OF PROCESSES: NEW PROCESSES > JUST UNBLOCKED PROCESSES > PROCESSES THAT JUST EXECUTED 
	

	if (system_time >= starting_time[toAdd] && toAdd < totalProcesses) //only add processes if limit has not been reached
	{
		do
		{
			if (number_of_active_processes != 0)
			{
				readyQueue[n_active_processes - 1] = toAdd + 1;
				readyQueue[n_active_processes] = previous;
			}
			else readyQueue[0] = toAdd + 1;

			toAdd++;
			number_of_active_processes++;
			n_active_processes++;
		} 
		while (system_time >= starting_time[toAdd] && toAdd < totalProcesses); // more processes might also be ready to enter RQ
	}

	else if (total_exectime[currentProcess] < cumulative_exectime[currentProcess][unchanged_currentEvent])
	{
		readyQueue[number_of_active_processes - 1] = previous;
	}

	else readyQueue[number_of_active_processes] = 0; 

	if (ready_to_block)
	{
		append_blockedQueue(currentProcess);
		ready_to_block = false;
	}  // REMOVE REMOVE unchanged_currentEvent

}

// process executes on CPU. Called by simulate_job_mix()
void execute(int time_quantum, int currentProcess, bool isEmpty_readyQueue)
{
	int executiontime; //time spent on CPU or time in which CPU is idle (readyQueue is empty and so time moves forward)
	if ((currentProcess + 1) != previous) total_process_completion_time += TIME_CONTEXT_SWITCH;

	if (!isEmpty_readyQueue)
	{
		executiontime = min(time_quantum, cumulative_exectime[currentProcess][currentEvent_of_each_process[currentProcess]] - total_exectime[currentProcess]);
	}
	else
	{
		sort_blockedQueue(100000000 + TIME_CONTEXT_SWITCH); //give arbitrary large value (CPU is idle for however long io processing takes)
		return;
	}

	if (!isEmpty_blockedQueue()) sort_blockedQueue(executiontime + TIME_CONTEXT_SWITCH);
	total_process_completion_time += executiontime;
	total_exectime[currentProcess] += executiontime;
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    total_process_completion_time = TIME_CONTEXT_SWITCH; // 5 microseconds at start added

    while (number_of_exited_processes < totalProcesses) 
    {
		int currentProcess = readyQueue[0] - 1; //process at front of readyQueue

        if (number_of_active_processes == 0)
        {
			if (!isEmpty_blockedQueue())
			{
				//readyQueue[0] = 0; //remove 
				execute(time_quantum, currentProcess, true);

			}
			else
			{
				int jumpforward = starting_time[toAdd] - starting_time[0];
				total_process_completion_time = jumpforward;  //jump forward in time to next process (since nothing happens in
				// between)
			}
        }

        else
        { 
			execute(time_quantum, currentProcess, false);
        }

		//printf("\n");
		//printf("value: %i\n", starting_time[0] + total_process_completion_time);
        sort_readyQueue(starting_time[0] + total_process_completion_time);   
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
	reset_total_exectime();
    reset_readyQueue();
	reset_blockedQueue();
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
