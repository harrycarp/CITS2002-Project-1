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
    int time_processed;
};

int timetaken = 0;

int compute_remaining = 0;

int time_quantum_usecs = 0;

int pipesize_bytes;

struct Process ProcessList[MAX_PROCESSES];


//  ---------------------------------------------------------------------

void state_change();
void is_running();
void is_ready();
void is_sleeping();
int next_available_spot();
int index_of_process();
struct Process find_process();
bool p_is_ready(); bool p_is_running(); bool p_is_sleeping();
void set_time_processed(); int get_time_processed();
int get_time_taken();
// -----------------------------------------------------------------------

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
    if(p_is_ready(p)){
        state_change(p,"READY", "RUNNING");
        is_running(p);
    }
    printf("@%i   p%i.sleep() for %iusecs, ", timetaken, p.id, usecs);

    state_change(p,"RUNNING", "SLEEPING");
    is_sleeping(p);
    timetaken += usecs;

    printf("@%i   p%i finished sleeping, ", timetaken, p.id);

    state_change(p,"SLEEPING", "READY");
    is_ready(p);

    state_change(p,"READY", "RUNNING");
    is_running(p);
}

/**
 * Compute for usecs.
 * TODO: implement per process (parents & child from fork) compute
 * @param usecs
 * @param program
 */
void compute_process(int usecs, struct Process p){
    if(p_is_ready(p) == true){
        state_change(p,"READY", "RUNNING");
        is_running(p);
    }
    if(usecs <= time_quantum_usecs){
        printf("@%i   p%i:compute() for %iusec (now completed) \n", timetaken,  p.id, usecs);
        set_time_processed(p, usecs);
        if(p_is_running(p)){
            state_change(p,"RUNNING", "READY");
            is_ready(p);
        }
    } else {
        compute_remaining = usecs - time_quantum_usecs;
        printf("@%i   p%i:compute() for %iusec (%i remaining) \n", timetaken, p.id, time_quantum_usecs, compute_remaining);
        set_time_processed(p, time_quantum_usecs);
        if(p_is_running(p)){
            state_change(p,"RUNNING", "READY");
            is_ready(p);
        }
        compute_process(compute_remaining, p);
    }
}

void fork_process(struct Process parent, int new_id){
    if(p_is_ready(parent) == true){
        state_change(parent,"READY", "RUNNING");
        is_running(parent);
    }
    printf("@%i   p%i:fork(), ",  timetaken, parent.id);
    struct Process child = {
            parent.is_running,
            parent.is_sleeping,
            parent.is_ready,
            new_id,
            0
    };
    printf(" new childPID=%i\n", child.id);
    state_change(child, "NEW", "READY");
    state_change(parent, "RUNNING", "READY");
    is_ready(child); is_ready(parent);
    int int_last = next_available_spot();
    ProcessList[int_last] = child;
}

int next_available_spot(){
    for(int i = 0; i < MAX_PROCESSES; i++){
        if(ProcessList[i].id == 0){
            return i;
        }
    }
}

void exit_process(struct Process p){
    if(p_is_ready(p) == true){
        state_change(p,"READY", "RUNNING");
        is_running(p);
    }
    printf("@%i   p%i:exit(), ", timetaken, p.id);
    state_change(p,"RUNNING", "EXITED");
}

void state_change(struct Process p, char s1[], char s2[]){
    printf("@%i   p%i: %s->%s\n",  timetaken, p.id, s1, s2);
    set_time_processed(p, 5);
}

// can we minimise these down? Probably..
void is_running(struct Process p){
    //printf("setting p.%i to running \n", p.id);
    p.is_running = true;
    p.is_ready = false;
    p.is_sleeping = false;

    ProcessList[index_of_process(p)] = p;
}

void is_sleeping(struct Process p){
    //printf("setting p.%i to sleeping \n", p.id);

    p.is_running = false;
    p.is_ready = false;
    p.is_sleeping = true;

    ProcessList[index_of_process(p)] = p;
}

void is_ready(struct Process p){
    //printf("setting p.%i to ready \n", p.id);
    p.is_running = false;
    p.is_ready = true;
    p.is_sleeping = false;

    ProcessList[index_of_process(p)] = p;
}

int get_time_processed(struct Process p){
    return ProcessList[index_of_process(p)].time_processed;
}

void set_time_processed(struct Process p, int exec_time){
    p.time_processed = p.time_processed + exec_time;
    printf(">> ix: %i\n", index_of_process(p));
    ProcessList[index_of_process(p)] = p;
    printf(">> %i\n", ProcessList[index_of_process(p)].time_processed);
    timetaken = get_time_taken();
}

bool p_is_ready(struct Process p){
//    printf("p%i: IS READY is %s \n",
//           ProcessList[index_of_process(p)].id,
//           ProcessList[index_of_process(p)].is_ready
//           ? "TRUE" : "FALSE"
//           );
    return ProcessList[index_of_process(p)].is_ready;
}

bool p_is_running(struct Process p){
//    printf("p%i: IS RUNNING is %s \n",
//           ProcessList[index_of_process(p)].id,
//           ProcessList[index_of_process(p)].is_running
//           ? "TRUE" : "FALSE"
//    );
    return ProcessList[index_of_process(p)].is_running;
}

bool p_is_sleeping(struct Process p){
//    printf("p%i: IS SLEEPING is %s \n",
//           ProcessList[index_of_process(p)].id,
//           ProcessList[index_of_process(p)].is_sleeping
//           ? "TRUE" : "FALSE"
//    );
    return ProcessList[index_of_process(p)].is_sleeping;
}

int index_of_process(struct Process p){
    for (int i = 0; i < MAX_PROCESSES; i++){
        if(ProcessList[i].id == p.id){
            //printf("--ix: %i ", i);
            return i;
        }
    }
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
        struct Process p = find_process(atoi(words[0]));

        //if the process hasn't been forked out, abort.
        //if( sizeof ProcessList < atoi(words[0])-1) return;

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
            exit_process(p);
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

struct Process find_process(int pid){
    for(int i = 0; i < MAX_PROCESSES; i++){
        if(ProcessList[i].id == pid) { return ProcessList[i]; }
    }
}

void BOOT(){
    struct Process p1 = {
        true,
        false,
        false,
        1,
        0
    };
    ProcessList[0] = p1;

    printf("@%i BOOT, p%i.RUNNING\n", timetaken, p1.id);
//    is_running(ProcessList[0]);
}

//  ---------------------------------------------------------------------
//  CHECK THE COMMAND-LINE ARGUMENTS, CALL parse_eventfile(), RUN SIMULATION
int main(int argc, char *argv[]) {

    if(argv[3] != NULL) time_quantum_usecs = atoi(argv[2]);
    if(argv[4] != NULL) pipesize_bytes = atoi(argv[3]);

    printf("Time Quantum: %i nano seconds\n", time_quantum_usecs);
    printf("Pipe Size: %i bytes\n\n", pipesize_bytes);
    BOOT();
    printf("\n@--> %i\n", ProcessList[0].id);
    parse_eventfile("p1", argv[1]);

    timetaken = get_time_taken();

    printf("\n\ntimetaken %i\n", timetaken);
    return 0;
}

int get_time_taken(){
    int sum = 0;
    for(int i = 0; i < MAX_PROCESSES; i++){
        sum += ProcessList[i].time_processed;
    }
    return sum;
}