#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

/* CITS2002 Project 1 2019
   Name(s):             Arun Muthu
   Student number(s):   22704805
 */

//  besttq (v1.0)
//  Written by Chris.McDonald@uwa.edu.au, 2019, free for all to copy and modify

//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF TRACEFILE CONTENTS .  

#define MAX_DEVICES             4
#define MAX_DEVICE_NAME         20
#define MAX_PROCESSES           50
#define MAX_EVENTS_PER_PROCESS	100
#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5
#define MILLION                 1000000

//  VARIABLES CONCERNED WITH FINAL OUTPUT GIVEN

int optimal_time_quantum                = 0; 
int total_process_completion_time       = 0;
int optimal_completion_time             = INT_MAX; //START WITH HIGHEST POSSIBLE VALUE, MAIN() FINDS LOWER VALUES

//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

//  ----------------------------------------------------------------------
// MIN AND MAX FUNCTIONS to compare two values 

#define min(x,y) (x) < (y) ? (x) : (y)
#define max(x,y) (x) < (y) ? (y) : (x)

//  ----------------------------------------------------- GLOBAL VARIABLES 

// DATA STRUCTURES USED TO STORE INFORMATION FROM TRACE FILES ------

char devices[MAX_DEVICES][MAX_DEVICE_NAME];  // STORE DEVICE NAMES
int transfer_rate[MAX_DEVICES];  // STORE TRANSFER RATES [BYTES/SECOND] 
int starting_time[MAX_PROCESSES]; // STORE STARTING TIME OF PROCESSES (SINCE OS REBOOT)

char io_events[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS][MAX_DEVICE_NAME];
double io_data[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
double io_data_copy[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int cumulative_exectime[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]; //STORE CUMULATIVE EXECUTION TIMES OF EVENTS IN EACH PROCESS 

// SCALAR VARIABLES KEEPING TRACK OF VARIOUS INFORMATION

int total_exectime[MAX_PROCESSES]; // KEEPS TRACK OF AMOUNT OF TIME EACH PROCESS HAS SPENT ON THE CPU
int totalProcesses = 0; 
int readyQueue[MAX_PROCESSES] = {1}; //INDEX 0 IN READY QUEUE REPRESENTS PROCESS THAT IS EXECUTING ON CPU 
int blockedQueue[MAX_PROCESSES];
int currentEvent_of_each_process[MAX_PROCESSES] = {0};
int previous = 1; // MOST RECENT PROCESS THAT EXECUTED ON CPU
int number_of_exited_processes = 0;
int number_of_active_processes = 1;  // NUMBER OF PROCESSES IN READY AND RUNNING STATE 
bool first_iteration = true;
int toAdd = 1; //NEXT PROCESS WAITING TO BE ADDED TO READY QUEUE FOR THE FIRST TIME
int prioritized_process;  //BLOCKED PROCESS WITH HIGHEST PRIORITY 
int highest_transferRate; 
bool new_dataBus_owner; 

/* NOTE: Processes are represented in readyQueue and blockedQueue as process 1, process 2, process 3, etc. 
   However when accessing arrays such as starting_time, one must be taken off the process number since index 
   starts at 0. E.g. for a process's starting time, starting_time[currentProcess - 1] would be written
*/

//  -----------------------------------------------------

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

		else if (nwords == 4 && strcmp(word0, "i/o") == 0) {
			cumulative_exectime[totalProcesses][n_events] = atoi(word1);
			strcpy(io_events[totalProcesses][n_events], word2);
			io_data[totalProcesses][n_events] = atoi(word3);
			io_data_copy[totalProcesses][n_events] = atoi(word3);
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

//  ----------------------------------------------------- RESET FUNCTIONS 
// CALLED FOR EACH NEW SIMULATION OF JOB MIX (WITH TQ VARYING)

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

//  ----------------------------------------------------- HELPER FUNCTIONS 

// RETURNS NUMBER OF FINAL EVENT OF A SPECIFIED PROCESS (EXIT [VALUE] IS CONSIDERED AN EVENT)
int get_final_event(int process)
{
	if (process < 0) return 0;

	for (int i = 0; i < MAX_EVENTS_PER_PROCESS; i++)
	{
		if (cumulative_exectime[process][i] == 0) return i - 1;
	}

	return MAX_EVENTS_PER_PROCESS - 1; //FINAL EVENT MUST BE AT INDEX MAX_EVENTS_PER_PROCESS - 1
}

bool isEmpty_blockedQueue(void)
{
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		if (blockedQueue[i] != 0) return false;
	}

	return true;
}

void append_blockedQueue(int currentProcess)
{
	number_of_active_processes--;
	int j = 0;

	// MOVE ALL PROCESSES ONE SPOT DOWN IF POSSIBLE. E.G. {1,0,2} BECOMES {1,2,0}
	// THIS PREVENTS NEW BLOCKED PROCESS BEING ADDED TO INDEX 1, I.E. {1,3,2}
	while (j < MAX_PROCESSES - 1)
	{
		int original; 

		if (blockedQueue[j] == 0)
		{
			original = j;
			while (blockedQueue[j++] == 0 && j != MAX_PROCESSES) {}
			blockedQueue[original] = blockedQueue[j - 1];
			blockedQueue[j - 1] = 0;
			j = original + 1;
		}

		else j++;
	}

	// APPEND TO BLOCKED QUEUE
	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		if (blockedQueue[i] == 0)
		{
			blockedQueue[i] = currentProcess + 1;
			break;
		}
	}
}

void remove_blockedQueue(int process)
{
	for (int i = 0; i < MAX_PROCESSES; i++) 
	{
		if (blockedQueue[i] == process)
		{
			blockedQueue[i] = 0;
			break;
		}
	}
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

// FINDS PROCESS WITH HIGHEST PRIORITY TO ACQUIRE DATA BUS
int get_prioritizedProcess(void)
{
	int device; 

	if (!new_dataBus_owner && io_data[prioritized_process][currentEvent_of_each_process[prioritized_process]] < io_data_copy[prioritized_process][currentEvent_of_each_process[prioritized_process]])
	{
		return prioritized_process + 1;
	}

	highest_transferRate = 0;

	for (int i = 0; i < MAX_PROCESSES; i++)
	{
		if (blockedQueue[i] == 0) continue;

		int process = blockedQueue[i] - 1;
		device = device_number(io_events[process][currentEvent_of_each_process[process]]);
		if (transfer_rate[device] > highest_transferRate)
		{
			highest_transferRate = transfer_rate[device];
			prioritized_process = process;
		}
	}

	return prioritized_process + 1;   
}

//  ----------------------------------------------------- MAIN FUNCTIONS

/* I/O OPERATIONS PERFORMED HERE. AVAILABLE_TIME REPRESENTS THE ALLOCATED TIME THAT THE COMPUTER CAN PERFORM IO EVENTS. 
 AVAILABLE_TIME IS EITHER THE TIME QUANTUM OR THE AMOUNT OF TIME BEFORE THE CURRENTLY EXECUTING PROCESS IS BLOCKED/EXITS. 
 IF THERE ARE NO ACTIVE PROCESSES THEN AVAILABLE_TIME IS SET TO LARGE VALUE (PROCESSES CAN TAKE AS MUCH TIME AS THEY NEED 
 TO PERFORM IO) */
void sort_blockedQueue(int available_time, bool isDifferentProcess)
{
	double timespent = 0;
	int blocked_process;

	while (timespent < available_time && !isEmpty_blockedQueue())
	{
		blocked_process = get_prioritizedProcess() - 1;  

		// IF PROCESS DOES NOT NEED TO REACQUIRE BUS AND DIFFERENT PROCESS EXECUTING ON CPU,
		// MORE AVAILABLE TIME FOR I/O PROCESSING
		if (number_of_active_processes != 0 && !new_dataBus_owner && isDifferentProcess) available_time += TIME_CONTEXT_SWITCH;

		// IF NO PROCESS RUNNING ON CPU, ACQUIRING BUS WOULD CONTRIBUTE TO TOTAL_PROCESS_COMPLETION_TIME
		else if (number_of_active_processes == 0 && new_dataBus_owner) total_process_completion_time += TIME_ACQUIRE_BUS;

	    double bytes = io_data[blocked_process][currentEvent_of_each_process[blocked_process]];
		if ((bytes*MILLION/highest_transferRate) < (available_time - timespent))    
		{
			timespent += ceil(bytes*MILLION/highest_transferRate);   
			io_data[blocked_process][currentEvent_of_each_process[blocked_process]] -= bytes;
		}
		else
		{
			io_data[blocked_process][currentEvent_of_each_process[blocked_process]] -= ((available_time - timespent) * highest_transferRate)/MILLION;
			timespent = available_time - timespent;
		}

		new_dataBus_owner = false;
	
		// REMOVE FROM BLOCKED QUEUE IF NO MORE BYTES TO BE TRANSFERRED 
		if (io_data[blocked_process][currentEvent_of_each_process[blocked_process]] <= 0)
		{
			currentEvent_of_each_process[blocked_process]++;
			remove_blockedQueue(blocked_process + 1);   
			new_dataBus_owner = true;
			number_of_active_processes++;

			if (number_of_active_processes == 1)
			{
				// TIME CONTEXT SWITCH REQUIRED BECAUSE PROCESS GOES FROM READY TO RUNNING
				total_process_completion_time += timespent + TIME_CONTEXT_SWITCH;
				readyQueue[0] = blocked_process + 1;
				break;
			}
			else readyQueue[number_of_active_processes - 1] = blocked_process + 1;
		}
	}
}

// MANAGES THE READY QUEUE AFTER EACH TIME QUANTUM (OR WHEN A PROCESS BECOMES BLOCKED/EXITS)
void sort_readyQueue(int system_time)    
{
	int currentProcess = readyQueue[0] - 1; // PROCESS THAT WAS MOST RECENTLY AT FRONT OF READYQUEUE (BEFORE IT EXECUTED)
	int n_active_processes = number_of_active_processes; // KEEP COPY FOR LATER 
	int old_prioritized_process = get_prioritizedProcess();
	previous = readyQueue[0];
	int finalevent = get_final_event(currentProcess);
	bool ready_to_block = false;

	// WHEN A PROCESS EXITS 
	if (total_exectime[currentProcess] >= cumulative_exectime[currentProcess][finalevent] && number_of_active_processes != 0)
	{
		number_of_active_processes--;
		number_of_exited_processes++; 
		previous = 0;  
	}

	// WHEN A PROCESS IS TO BE BLOCKED (APPENDING TO BLOCKED QUEUE OCCURS FURTHER BELOW)
	else if (finalevent != 0 && total_exectime[currentProcess] >= cumulative_exectime[currentProcess][currentEvent_of_each_process[currentProcess]])
	{
		ready_to_block = true; 
	}

	// MOVES EACH PROCESS 1 INDEX DOWN (1 STEP CLOSER TO FRONT OF READY QUEUE)
    for (int i = 0; i < totalProcesses - 1; i++)
    {
        int nextvalue;
		if ((nextvalue = readyQueue[i + 1]) != 0)
		{
			readyQueue[i] = nextvalue;
			readyQueue[i + 1] = 0;
		}
    }

	// PROCESS THAT JUST EXECUTED ADDED TO BACK OF READY QUEUE 
	if (total_exectime[currentProcess] < cumulative_exectime[currentProcess][currentEvent_of_each_process[currentProcess]])
	{
		readyQueue[number_of_active_processes - 1] = previous;
	}

	// PROCESS REMOVED DUE TO BEING BLOCKED OR EXITING
	else
	{
		readyQueue[number_of_active_processes] = 0;
		if (number_of_active_processes - 1 != 0) previous = 0; 
	}

	// WHEN A BRAND NEW PROCESS IS TO BE ADDED TO READY QUEUE (NEW -> READY)
	// MORE PROCESSES MIGHT BE READY TO ENTER RQ, THUS WHILE LOOP
	while (system_time >= starting_time[toAdd] && toAdd < totalProcesses)
	{
			if (number_of_active_processes != 0)
			{
				readyQueue[n_active_processes - 1] = toAdd + 1;
				if (ready_to_block) readyQueue[n_active_processes] = 0;
				else readyQueue[n_active_processes] = previous;
			}
			else readyQueue[0] = toAdd + 1;

			toAdd++;
			number_of_active_processes++;
			n_active_processes++;
	}

	if (ready_to_block)
	{
		append_blockedQueue(currentProcess);
		ready_to_block = false;
	}  

	if (first_iteration && !isEmpty_blockedQueue())
	{
		first_iteration = false;
		new_dataBus_owner = true;
	}							
	else if (get_prioritizedProcess() == old_prioritized_process && !new_dataBus_owner) new_dataBus_owner = false;
	else new_dataBus_owner = true;
}

// PROCESS EXECUTES ON CPU
void execute(int time_quantum, int currentProcess, bool isEmpty_readyQueue)
{
	bool isDifferentProcess = false;
	int executiontime; // TIME SPENT ON CPU
	if ((currentProcess + 1) != previous)     
	{
		total_process_completion_time += TIME_CONTEXT_SWITCH;
		isDifferentProcess = true;
	}

	if (!isEmpty_readyQueue)
	{
		executiontime = min(time_quantum, cumulative_exectime[currentProcess][currentEvent_of_each_process[currentProcess]] - total_exectime[currentProcess]);
		if (!isEmpty_blockedQueue()) sort_blockedQueue(executiontime, isDifferentProcess);
		total_process_completion_time += executiontime;
		total_exectime[currentProcess] += executiontime;
	}
	else sort_blockedQueue(INT_MAX, isDifferentProcess); // GIVE LARGE VALUE (CPU IS IDLE FOR HOWEVER LONG I/O PROCESSING TAKES)
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    total_process_completion_time = TIME_CONTEXT_SWITCH; 

    while (number_of_exited_processes < totalProcesses) 
    {
		int currentProcess = readyQueue[0] - 1; // PROCESS THAT WILL EXECUTE ON CPU

        if (number_of_active_processes == 0)
        {
			if (!isEmpty_blockedQueue()) execute(time_quantum, currentProcess, true);
			else total_process_completion_time = starting_time[toAdd] - starting_time[0];  //JUMP FORWARD IN TIME (SINCE NOTHING HAPPENS)
        }

		else execute(time_quantum, currentProcess, false);

        sort_readyQueue(starting_time[0] + total_process_completion_time);   
    }
}

// RESET ALL VARIABLES INCLUDING ARRAYS AND SCALAR VARIABLES. THIS FUNCTION IS CALLED BEFORE A NEW SIMULATION IS
// RUN (SAME JOB MIX BUT DIFFERENT TIME QUANTUM)
void reset_everything(char program[], char tracefile[])
{
    total_process_completion_time = 0; 
    number_of_exited_processes = 0;
    number_of_active_processes = 1;
    totalProcesses = 0;
    previous = 1;
    toAdd = 1;
	new_dataBus_owner = true;
	first_iteration = true;
	prioritized_process = 0;
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
    for (int time_quantum=TQ0 ; time_quantum<=TQfinal ; time_quantum += TQinc) 
    {
        simulate_job_mix(time_quantum);
        if (total_process_completion_time <= optimal_completion_time)
        {
            optimal_completion_time = total_process_completion_time;
            optimal_time_quantum = time_quantum;
        }

        reset_everything(argvalue[0], argvalue[1]);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, optimal_completion_time);

    exit(EXIT_SUCCESS);

}
//  vim: ts=8 sw=4 