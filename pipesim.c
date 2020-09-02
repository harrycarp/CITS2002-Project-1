#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

/* CITS2002 Project 1 2020
   Name:                Harry Devon Carpenter
   Student number(s):   22723303
 */


//  MAXIMUM NUMBER OF PROCESSES OUR SYSTEM SUPPORTS (PID=1..20)
#define MAX_PROCESSES                       20

//  MAXIMUM NUMBER OF SYSTEM-CALLS EVER MADE BY ANY PROCESS
#define MAX_SYSCALLS_PER_PROCESS            50

//  MAXIMUM NUMBER OF PIPES THAT ANY SINGLE PROCESS CAN HAVE OPEN (0..9)
#define MAX_PIPE_DESCRIPTORS_PER_PROCESS    10

//  TIME TAKEN TO SWITCH ANY PROCESS FROM ONE STATE TO ANOTHER
#define USECS_TO_CHANGE_PROCESS_STATE       5

//  TIME TAKEN TO TRANSFER ONE BYTE TO/FROM A PIPE
#define USECS_PER_BYTE_TRANSFERED           1


//  ---------------------------------------------------------------------

//  YOUR DATA STRUCTURES, VARIABLES, AND FUNCTIONS SHOULD BE ADDED HERE:

struct Process {
    bool is_running;
    bool is_sleeping;
    bool is_ready;
    int id;
    char name[3]; //using 3 because max process count is 20 (i.e. p20)
};

int timetaken = 0;

int compute_remaining = 0;

int time_quantum_usecs = 0;

int pipesize_bytes;

struct Process ProcessList[0];


//  ---------------------------------------------------------------------

void state_change();
void is_running(); void is_ready(); void is_sleeping();

int get_next_available_index();

//  FUNCTIONS TO VALIDATE FIELDS IN EACH eventfile - NO NEED TO MODIFY
int check_PID(char word[], int lc) {
    int PID = atoi(word);

    if (PID <= 0 || PID > MAX_PROCESSES) {
        printf("invalid PID '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return PID;
}

int check_microseconds(char word[], int lc) {
    int usecs = atoi(word);

    if (usecs <= 0) {
        printf("invalid microseconds '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return usecs;
}

int check_descriptor(char word[], int lc) {
    int pd = atoi(word);

    if (pd < 0 || pd >= MAX_PIPE_DESCRIPTORS_PER_PROCESS) {
        printf("invalid pipe descriptor '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return pd;
}

int check_bytes(char word[], int lc) {
    int nbytes = atoi(word);

    if (nbytes <= 0) {
        printf("invalid number of bytes '%s', line %i\n", word, lc);
        exit(EXIT_FAILURE);
    }
    return nbytes;
}

// ------------------------------------------------------------------------

/**
 * Sleep the process
 * @param usecs the amount of seconds to sleep for
 * @param program the program to sleep
 */
void sleep_process(int usecs, struct Process p){
    if(p.is_ready){
        state_change("READY", "RUNNING");
        is_running(p);
    }
    printf("sleep() for %iusecs, ", usecs);

    state_change("RUNNING", "SLEEPING");
    is_sleeping(p);
    timetaken += usecs;

    printf("finished sleeping, ");

    state_change("SLEEPING", "READY");
    is_ready(p);

    state_change("READY", "RUNNING");
    is_running(p);
}

/**
 * Compute for usecs.
 * TODO: implement per process (parents & child from fork) compute
 * @param usecs
 * @param program
 */
void compute_process(int usecs, struct Process p){
    if(p.is_ready){
        state_change("READY", "RUNNING");
        is_running(p);
    }
    if(usecs <= time_quantum_usecs){
        printf("@%i p%i:compute() for %iusec (now completed) \n", timetaken,  p.id, usecs);
        timetaken += usecs;
        if(p.is_running){
            state_change("RUNNING", "READY");
            is_ready(p);
        }
    } else {
        timetaken += time_quantum_usecs;
        compute_remaining = usecs - time_quantum_usecs;
        printf("@%i p%i:compute() for %iusec (%i remaining) \n", timetaken, p.id, time_quantum_usecs, compute_remaining);
        if(p.is_running){
            state_change("RUNNING", "READY");
            is_ready(p);
        }
        compute_process(compute_remaining, p);
    }
}

void fork_process(struct Process parent, int new_id){
    printf("FORKING p%i -> ", parent.id);
    struct Process child = {
            parent.is_running,
            parent.is_sleeping,
            parent.is_ready,
            new_id
    };
    printf("NEW FORK p%i \n", child.id);
    is_ready(child); is_ready(parent);
    ProcessList[sizeof(ProcessList) + 1] = child;
}

void exit_process(){
    printf("exit(), ");
    state_change("RUNNING", "EXITED");
}


void state_change(char s1[], char s2[]){
    printf("%s->%s\n", s1, s2);
    timetaken += 5;
}

// can we minimise these down? Probably..
void is_running(struct Process p){
    printf("setting p.%i to running \n", p.id);
    p.is_running = true;
    p.is_ready = false;
    p.is_sleeping = false;
}

void is_sleeping(struct Process p){
    printf("setting p.%i to sleeping \n", p.id);
    p.is_running = false;
    p.is_ready = false;
    p.is_sleeping = true;
}

void is_ready(struct Process p){
    printf("setting p.%i to ready \n", p.id);
    p.is_running = false;
    p.is_ready = true;
    p.is_sleeping = false;
}

//  parse_eventfile() READS AND VALIDATES THE FILE'S CONTENTS
//  YOU NEED TO STORE ITS VALUES INTO YOUR OWN DATA-STRUCTURES AND VARIABLES
void parse_eventfile(char program[], char eventfile[]) {
#define LINELEN                 100
#define WORDLEN                 20
#define CHAR_COMMENT            '#'

    //  ATTEMPT TO OPEN OUR EVENTFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp = fopen(eventfile, "r");

    if (fp == NULL) {
        printf("%s: unable to open '%s'\n", program, eventfile);
        exit(EXIT_FAILURE);
    }

    char line[LINELEN], words[4][WORDLEN];
    int lc = 0;

    //  READ EACH LINE FROM THE EVENTFILE, UNTIL WE REACH THE END-OF-FILE
    while (fgets(line, sizeof line, fp) != NULL) {
        ++lc;

        //  COMMENT LINES ARE SIMPLY SKIPPED
        if (line[0] == CHAR_COMMENT) {
            continue;
        }

        //  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        int nwords = sscanf(line, "%19s %19s %19s %19s",
                            words[0], words[1], words[2], words[3]);

        //  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if (nwords <= 0) {
            continue;
        }

        //  ENSURE THAT THIS LINE'S PID IS VALID
        int thisPID = check_PID(words[0], lc);

        //  OTHER VALUES ON (SOME) LINES
        int otherPID, nbytes, usecs, pipedesc;

        // setup the process
        struct Process p = ProcessList[atoi(words[0])-1];

        //if the process hasn't been forked out, abort.
//        if( sizeof ProcessList < atoi(words[0])-1) return;


        //  IDENTIFY LINES RECORDING SYSTEM-CALLS AND THEIR OTHER VALUES
        //  THIS FUNCTION ONLY CHECKS INPUT;  YOU WILL NEED TO STORE THE VALUES
        if (nwords == 3 && strcmp(words[1], "compute") == 0) {
            usecs = check_microseconds(words[2], lc);
            compute_process(usecs, p);

        } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {
            usecs = check_microseconds(words[2], lc);
            sleep_process(usecs, p);
        }
        else if (nwords == 2 && strcmp(words[1], "exit") == 0) {
            exit_process();
        }
        else if (nwords == 3 && strcmp(words[1], "fork") == 0) {
            otherPID = check_PID(words[2], lc);
            fork_process(p, otherPID);
        }
        else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
            otherPID = check_PID(words[2], lc);
        }
        else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
        }
        else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
            nbytes = check_bytes(words[3], lc);
        } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {
            pipedesc = check_descriptor(words[2], lc);
            nbytes = check_bytes(words[3], lc);
        }
//  UNRECOGNISED LINE
        else {
            printf("%s: line %i of '%s' is unrecognized\n", program, lc, eventfile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);

#undef  LINELEN
#undef  WORDLEN
#undef  CHAR_COMMENT
}

void BOOT(){
    struct Process p1 = {
        false,
        false,
        false,
        1
    };

    ProcessList[0] = p1;
    printf("@%i BOOT, %s.RUNNING\n", timetaken, ProcessList[0].name);
}

//  ---------------------------------------------------------------------
//  CHECK THE COMMAND-LINE ARGUMENTS, CALL parse_eventfile(), RUN SIMULATION
int main(int argc, char *argv[]) {

    if(argv[3] != NULL) time_quantum_usecs = atoi(argv[2]);
    if(argv[4] != NULL) pipesize_bytes = atoi(argv[3]);

    printf("Time Quantum: %i nano seconds\n", time_quantum_usecs);
    printf("Pipe Size: %i bytes\n\n", pipesize_bytes);
    BOOT();
    parse_eventfile("p1", argv[1]);


    printf("\n\ntimetaken %i\n", timetaken);
    return 0;
}
