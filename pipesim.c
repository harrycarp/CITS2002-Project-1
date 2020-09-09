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

// MAX LENGTH OF A WORD IN A LINE
#define WORDLEN                 20
//  ---------------------------------------------------------------------

//  YOUR DATA STRUCTURES, VARIABLES, AND FUNCTIONS SHOULD BE ADDED HERE:


// struct for pipe
// * @int id = the ID (desc) of the pipe
// * @int written_bytes = the number of bytes currently written to the pipe
// * @bool is_readable = can you read the pipe?
// * @bool is_writable = can you write to the pipe?
struct Pipe {
    int id;
    int bytes;
    bool is_readable;
    bool is_writable;
    int read_process;
};

//
// struct for Process
// * @bool is_running = if the process is running
// * @bool is_sleeping = if the process is asleep
// * @bool is_ready = is the process is ready
// * @int id = the PID
// * @int time_processed = the accumulative time the process has processed
//
struct Process {
    bool is_running;
    bool is_sleeping;
    bool is_ready;
    bool is_active;
    bool is_waiting;
    int id;
    int waiting_for;
    int time_processed;
    int ix; //mainly for testing purposes
    int sleep_for;
    struct Pipe pipes [MAX_PIPE_DESCRIPTORS_PER_PROCESS];
    int pipe_count;
    int command_count;
    char command_queue [MAX_SYSCALLS_PER_PROCESS][WORDLEN];
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
bool p_is_ready();
bool p_is_running();
bool p_is_sleeping();
void set_time_processed();
int get_time_processed();
int get_time_taken();
void check_sleep(bool);
void wake_process();
void set_process_inactive();
int get_pipe_index();
void
run_command_line(int nwords, char pString[4][20], int usecs, int pid, struct Process process, int pipedesc, int nbytes,
                 int lc);

// -----------------------------------------------------------------------

void set_pipe_writeable(struct Process process, struct Pipe pipe, bool b);

void set_pipe_readable(struct Process proc, struct Pipe pipe, bool b);

bool check_if_readable(struct Process process, int id);

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

struct Pipe get_pipe(struct Process proc, int pipe_id){
    for(int i = 0; i < proc.pipe_count; i++){
        if(proc.pipes[i].id == pipe_id){
            return proc.pipes[i];
        }
    }
    printf("pipe %i not found in process p%i\n", pipe_id, proc.id);
    exit(EXIT_FAILURE);
}


// ------------------------------------------------------------------------

//
// * Sleep the process
// * @param usecs the amount of seconds to sleep for
// * @param program the program to sleep
//
void sleep_process(int usecs, struct Process p) {
    if (p_is_ready(p)) {
        state_change(p, "READY", "RUNNING");
        is_running(p);
    }
    printf("@%i   p%i.sleep() for %iusecs, ", timetaken, p.id, usecs);
    state_change(p, "RUNNING", "SLEEPING");
    p.sleep_for =  usecs;
    is_sleeping(p);
}

bool is_process_asleep(struct Process p) {
    return p.is_sleeping;
}

void check_sleep(bool is_exit) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if(!ProcessList[i].is_active || !ProcessList[i].is_sleeping) return;
       // printf("@%i wake time at %iusecs, is passed: %s\n", timetaken, timetaken + ProcessList[i].sleep_for, timetaken >= timetaken + ProcessList[i].sleep_for ? "TRUE" : "FALSE");
        if (timetaken >= timetaken + ProcessList[i].sleep_for) {
            wake_process(ProcessList[i]);
        } else if (is_exit) {
            wake_process(ProcessList[i]);
        }
    }
}

void wake_process(struct Process p) {
    timetaken += p.sleep_for;
    printf("@%i   p%i finished sleeping, ", timetaken, p.id);
    state_change(p, "SLEEPING", "READY");
    is_ready(p);
    state_change(p, "READY", "RUNNING");
    is_running(p);
}

//
// * Compute for usecs.
// * @param usecs
// * @param program
//
void compute_process(int usecs, struct Process p) {
    if (p_is_running(p) == false) {
        state_change(p, "READY", "RUNNING");
        is_running(p);
    }
    if (usecs <= time_quantum_usecs) {
        printf("@%i   p%i:compute() for %iusec (now completed) \n", timetaken, p.id, usecs);
        set_time_processed(p, usecs);
        state_change(p, "RUNNING", "READY");
        is_ready(p);
    } else {
        compute_remaining = usecs - time_quantum_usecs;
        printf("@%i   p%i:compute() for %iusec (%i remaining) \n", timetaken, p.id, time_quantum_usecs,
               compute_remaining);
        set_time_processed(p, time_quantum_usecs);
        state_change(p, "RUNNING", "READY");
        is_ready(p);
        compute_process(compute_remaining, p);
    }
}

void fork_process(struct Process parent, int new_id) {
    if (p_is_ready(parent) == true) {
        state_change(parent, "READY", "RUNNING");
        is_running(parent);
    }
    printf("@%i   p%i:fork(), ", timetaken, parent.id);
    struct Process child = {
            parent.is_running,
            parent.is_sleeping,
            parent.is_ready,
            parent.is_active,
            parent.is_waiting,
            new_id,
            0,
            0,
            next_available_spot()
    };

    // on fork set all pipes that don't have a child to read from new forked process
    for(int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
        if(parent.pipes[i].read_process == 0){
            parent.pipes[i].read_process = new_id;
        }
    }

    printf(" new childPID=%i\n", child.id);
    state_change(child, "NEW", "READY");
    state_change(parent, "RUNNING", "READY");
    is_ready(child);
    is_ready(parent);
    int int_last = next_available_spot();
    ProcessList[int_last] = child;
}

void wait_for_process_termination(struct Process to_wait, int wait_for_pid){
    to_wait.is_waiting = true;
    to_wait.is_running = false;
    to_wait.is_sleeping = false;
    to_wait.is_ready = false;
    to_wait.waiting_for = wait_for_pid;

    state_change(to_wait, "RUNNING", "WAITING");

    ProcessList[to_wait.ix] = to_wait;
}

void run_queued_commands(struct Process process);

void unwait_on_termination(int terminated_pid){
    for(int i = 0; i < MAX_PROCESSES; i++){
        if(ProcessList[i].waiting_for == terminated_pid){
            printf("@%i   p%i:terminated(), remove p%i from wait queue\n", timetaken, terminated_pid, ProcessList[i].id);
            ProcessList[i].is_waiting = 0;
            run_queued_commands(ProcessList[i]);
        }
    }
}

void run_queued_commands(struct Process process) {
    // now to run the commands... oh boy...
    for(int i=0; i <= process.command_count; i++){

//        int count = 0;
//        for (int j = 0;process.command_queue[i][j] != '\0';j++)
//        {
//            printf("!%c\n", process.command_queue[i][j]);
//        }

        printf("@%i   p%i. running queued command: '%s'\n", timetaken, process.id, process.command_queue[i]);

        //run_command_line(count, process.command_queue[i], 0, 0, process, 0, 0, 0);
    }
}

void construct_pipe(struct Process write_node, int pipe_id){
    if(p_is_ready(write_node)){
        state_change(write_node, "READY", "RUNNING");
        is_running(write_node);
    }

    printf("@%i   p%i: pipe() pipedesc=%i, ", timetaken, write_node.id, pipe_id);

    struct Pipe new_pipe = {
            pipe_id,
            0,
            false,
            true,
    };

    write_node.pipes[write_node.pipe_count] = new_pipe;
    write_node.pipe_count += 1;
    ProcessList[write_node.ix] = write_node;

    state_change(write_node, "RUNNING", "READY");
    is_ready(write_node);
}

struct Process find_process_for_pipe(int pipe_id, int proc_id, struct Process process);

void write_pipe(struct Process write_node, int pipe_id, int bytes){

    if(p_is_ready(write_node)){
        state_change(write_node, "READY", "RUNNING");
        is_running(write_node);
    }

//    printf("@%i   p%i: writing %i bytes to pipe %i with buffer of %i bytes\n",
//           timetaken, write_node.id, bytes, pipe_id, pipesize_bytes);



    struct Pipe pipe_to_write = get_pipe(write_node, pipe_id);

    set_pipe_readable(write_node, pipe_to_write, false);

    if(pipe_to_write.is_writable){
        if((bytes + pipe_to_write.bytes) <= pipesize_bytes){
            printf("@%i   p%i: writepipe() %i bytes to pipedesc=%i\n",
                   timetaken, write_node.id, bytes, pipe_to_write.id);
            set_time_processed(write_node, (bytes*USECS_PER_BYTE_TRANSFERED));
            state_change(write_node, "RUNNING", "READY");
            set_pipe_readable(write_node, pipe_to_write, true);

        } else {
            printf("@%i   p%i: writepipe() %i bytes (%i total, %i remaining) to pipedesc=%i\n",
                   timetaken, write_node.id, pipesize_bytes, bytes, bytes-pipesize_bytes, pipe_to_write.id);

            pipe_to_write.bytes += pipesize_bytes;
            write_node.pipes[get_pipe_index(pipe_id)] = pipe_to_write;
            ProcessList[write_node.ix] = write_node;

            set_time_processed(write_node, (pipesize_bytes*USECS_PER_BYTE_TRANSFERED));
            state_change(write_node, "RUNNING", "WRITEBLOCKED");
            // since the pipe isn't empty, make sure we can't try and empty it again on the same process node
            set_pipe_writeable(write_node, pipe_to_write, false);

            //write_pipe(write_node, pipe_id, (bytes - pipe_to_write.written_bytes));
        }

    } else {
        printf("@%i   p%i: writepipe() WRITE BLOCKED for pipedesc=%i\n",
               timetaken, write_node.id, pipe_to_write.id);
    }
}

// this function needs to reduce the bytes stored in the pipe by what's been read
void read_pipe(struct Process write_node, int pipe_id, int bytes){
    if(!check_if_readable(write_node, pipe_id)){
        return;
    }
    if(p_is_ready(write_node)){
        state_change(write_node, "READY", "RUNNING");
        is_running(write_node);
    }

    struct Pipe pipe_to_read = get_pipe(find_process_for_pipe(pipe_id, write_node.id, write_node), pipe_id);
//    if(write_node.id - 1 == 0){
//        printf("ERROR: Can't read pipe from invalid process, setting to parent\n");
//        struct Process PARENT_PROC = find_process(write_node.id - 1);
//    }

    set_pipe_writeable(write_node, pipe_to_read, false);

    if(pipe_to_read.is_readable){
        if(bytes > pipe_to_read.bytes){
            // if it's trying to read more bytes than there are
            printf("@%i   p%i: readpipe() %i bytes  to pipedesc=%i (completed)\n",
                   timetaken, write_node.id, bytes-pipe_to_read.bytes, pipe_to_read.id);
            pipe_to_read.bytes = 0;
        } else {
            printf("@%i   p%i: readpipe() %i bytes to pipedesc=%i (completed)\n",
                   timetaken, write_node.id, bytes, pipe_to_read.id);
            pipe_to_read.bytes -= bytes;
        }
        state_change(write_node, "RUNNING", "READY");
        set_pipe_readable(write_node, pipe_to_read, true);
        write_node.pipes[get_pipe_index(pipe_id)] = pipe_to_read;
        ProcessList[write_node.ix] = write_node;

    } else {
        printf("@%i   p%i: readpipe() READ BLOCKED for pipedesc=%i\n",
               timetaken, write_node.id, pipe_to_read.id);
    }

}

struct Process find_process_for_pipe(int pipe_id, int proc_id, struct Process process) {
    printf(">>> finding Process with pipe %i with readable of p%i\n", pipe_id, proc_id);
    for(int i = 0; i < MAX_PROCESSES; i++){
        for(int j = 0; j < ProcessList[i].pipe_count; j++){
            if(ProcessList[i].pipes[j].id == pipe_id && ProcessList[i].pipes[j].read_process == proc_id){
                return ProcessList[i];
            }
        }
    }
    return process;
}

bool check_if_readable(struct Process process, int id) {
    for(int i = 0; i < MAX_PROCESSES; i++){
        for(int j = 0; j < MAX_PIPE_DESCRIPTORS_PER_PROCESS; j++){
            printf("pipe: %i == %i && process: %i == %i\n",
                   ProcessList[i].pipes[i].id, id, ProcessList[i].pipes[i].read_process, process.id);
            if(ProcessList[i].pipes[i].id == id && ProcessList[i].pipes[i].read_process == process.id){
                return true;
            }
        }
    }
    return false;
}

void set_pipe_readable(struct Process process, struct Pipe pipe, bool b) {
    pipe.is_readable = b;
    int pipe_index = get_pipe_index(process, pipe.id);
    if(pipe_index < 0) return;
    process.pipes[pipe_index] = pipe;
    ProcessList[process.ix] = process;
}

void set_pipe_writeable(struct Process process, struct Pipe pipe, bool b) {
    pipe.is_writable = b;
    int pipe_index = get_pipe_index(process, pipe.id);
    if(pipe_index < 0) return;
    process.pipes[pipe_index] = pipe;
    ProcessList[process.ix] = process;
}

int get_pipe_index(struct Process p, int pipe_id){
    for(int i = 0; i < MAX_PIPE_DESCRIPTORS_PER_PROCESS; i++){
        if(p.pipes[i].id == pipe_id) {
            return i;
        }
    }
    printf("@%i   p%i:NO PIPE FOUND\n", timetaken, p.id);
    return -1;
}

int next_available_spot() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (ProcessList[i].id == 0) {
            return i;
        }
    }
    return -1;
}

void exit_process(struct Process p) {
    if (p_is_ready(p)) {
        state_change(p, "READY", "RUNNING");
        is_running(p);
    }
    check_sleep(true);
    set_process_inactive(p);
    printf("@%i   p%i:exit(), ", timetaken, p.id);
    state_change(p, "RUNNING", "EXITED");
    unwait_on_termination(p.id);
}

void state_change(struct Process p, char s1[], char s2[]) {
    printf("@%i   p%i: %s->%s\n", timetaken, p.id, s1, s2);
    set_time_processed(p, USECS_TO_CHANGE_PROCESS_STATE);
}

//------------------------------------------------------
// can we minimise these down? Probably..
void is_running(struct Process p) {
    //printf("setting p.%i to running \n", p.id);
    p.is_running = true;
    p.is_ready = false;
    p.is_sleeping = false;

    ProcessList[p.ix] = p;
}

void is_sleeping(struct Process p) {
    //printf("setting p.%i to sleeping \n", p.id);

    p.is_running = false;
    p.is_ready = false;
    p.is_sleeping = true;

    ProcessList[p.ix] = p;
}

void is_ready(struct Process p) {
    //printf("setting p.%i to ready \n", p.id);
    p.is_running = false;
    p.is_ready = true;
    p.is_sleeping = false;

    ProcessList[p.ix] = p;
}

void set_process_inactive(struct Process p){
    //printf("setting p.%i to ready \n", p.id);
    p.is_active = false;
    ProcessList[p.ix] = p;
}

void set_time_processed(struct Process p, int exec_time) {
    struct Process p1 = p;
    p1.time_processed += exec_time;
    ProcessList[p.ix] = p1;
    timetaken += exec_time;
}

bool p_is_ready(struct Process p) {
    return ProcessList[p.ix].is_ready;
}

bool p_is_running(struct Process p) {
    return ProcessList[p.ix].is_running;
}

bool p_is_sleeping(struct Process p) {
    return ProcessList[p.ix].is_sleeping;
}

bool p_is_waiting(struct Process p){
    return ProcessList[p.ix].is_waiting;
}
//-------------------------------------------------------

//  parse_eventfile() READS AND VALIDATES THE FILE'S CONTENTS
//  YOU NEED TO STORE ITS VALUES INTO YOUR OWN DATA-STRUCTURES AND VARIABLES
void parse_eventfile(char program[], char eventfile[]) {
#define LINELEN                 100
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
        struct Process p = find_process(thisPID);

        if(p_is_waiting(p)){
            printf("@%i   p%i:is waiting for p%i termination\n", timetaken, p.id, p.waiting_for);
            // need to queue up commands to run AFTER this...
            // what's the best way to do this? store as plain text then run the operation again?

            // loop through each word in the command and store in arr.
            // assuming that a VALID eventfile is passed through, I SHOULD add error checking for this.
            for(int i = 0; i < 4; i++){
                if(sizeof words[i] > 0){
                    char *to_write = (char*)malloc(256 * sizeof(char));
                    sprintf(to_write, "%s ", words[i]);
                    strcat(p.command_queue[p.command_count], to_write);
                }
            }
            p.command_count += 1;
            ProcessList[p.ix] = p;
        }

        // check if the process being called upon is asleep, if so, wake it up
        if(is_process_asleep(p)){
            wake_process(p);
        }
        //check if an asleep process is can be woken, if so, wake it
        check_sleep(false);

        //MOVED THE IDENTIFIERS TO OWN FUNCTION
        run_command_line(nwords, words, usecs, otherPID, p, pipedesc, nbytes, lc);
//        //  IDENTIFY LINES RECORDING SYSTEM-CALLS AND THEIR OTHER VALUES
//        //  THIS FUNCTION ONLY CHECKS INPUT;  YOU WILL NEED TO STORE THE VALUES
//        if (nwords == 3 && strcmp(words[1], "compute") == 0) {
//            usecs = check_microseconds(words[2], lc);
//            compute_process(usecs, p);
//        } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {
//            usecs = check_microseconds(words[2], lc);
//            sleep_process(usecs, p);
//        } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {
//            exit_process(p);
//        } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {
//            otherPID = check_PID(words[2], lc);
//            fork_process(p, otherPID);
//        } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
//            otherPID = check_PID(words[2], lc);
//            wait_for_process_termination(p, otherPID);
//        } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
//            pipedesc = check_descriptor(words[2], lc);
//            construct_pipe(p, pipedesc);
//        } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
//            pipedesc = check_descriptor(words[2], lc);
//            nbytes = check_bytes(words[3], lc);
//            write_pipe(p, pipedesc, nbytes);
//        } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {
//            pipedesc = check_descriptor(words[2], lc);
//            nbytes = check_bytes(words[3], lc);
//            read_pipe(p, pipedesc, nbytes);
//        }
////  UNRECOGNISED LINE
//        else {
//            printf("%s: line %i of '%s' is unrecognized\n", program, lc, eventfile);
//            exit(EXIT_FAILURE);
//        }
    }
    fclose(fp);


}

void
run_command_line(int nwords, char words[4][WORDLEN], int usecs, int otherPID, struct Process p, int pipedesc, int nbytes,
                 int lc) {
    if (nwords == 3 && strcmp(words[1], "compute") == 0) {
        usecs = check_microseconds(words[2], lc);
        compute_process(usecs, p);
    } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {
        usecs = check_microseconds(words[2], lc);
        sleep_process(usecs, p);
    } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {
        exit_process(p);
    } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {
        otherPID = check_PID(words[2], lc);
        fork_process(p, otherPID);
    } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
        otherPID = check_PID(words[2], lc);
        wait_for_process_termination(p, otherPID);
    } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
        pipedesc = check_descriptor(words[2], lc);
        construct_pipe(p, pipedesc);
    } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
        pipedesc = check_descriptor(words[2], lc);
        nbytes = check_bytes(words[3], lc);
        write_pipe(p, pipedesc, nbytes);
    } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {
        pipedesc = check_descriptor(words[2], lc);
        nbytes = check_bytes(words[3], lc);
        read_pipe(p, pipedesc, nbytes);
    }
//  UNRECOGNISED LINE
    else {
        printf("%s: line %i is unrecognized\n", p, lc);
        exit(EXIT_FAILURE);
    }

}


#undef  LINELEN
#undef  WORDLEN
#undef  CHAR_COMMENT

struct Process find_process(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (ProcessList[i].id == pid) { return ProcessList[i]; }
    }
    exit(EXIT_FAILURE);
}

void BOOT() {
    struct Process p1 = {
            true,
            false,
            false,
            true,
            false,
            1,
            0,
            0,
            0,
            0
    };
    ProcessList[0] = p1;

    printf("@%i BOOT, p%i.RUNNING = %s\n", timetaken, p1.id, ProcessList[0].is_running ? "TRUE" : "FALSE");
//    is_running(ProcessList[0]);
}

//  ---------------------------------------------------------------------
//  CHECK THE COMMAND-LINE ARGUMENTS, CALL parse_eventfile(), RUN SIMULATION
int main(int argc, char *argv[]) {

    if (argv[2] != NULL) time_quantum_usecs = atoi(argv[2]);
    if (argv[3] != NULL) pipesize_bytes = atoi(argv[3]);

    printf("Time Quantum: %i nano seconds\n", time_quantum_usecs);
    printf("Pipe Size: %i bytes\n\n", pipesize_bytes);
    BOOT();
    printf("\n@--> %i\n", ProcessList[0].id);
    parse_eventfile("p1", argv[1]);

    printf("\n\ntimetaken %i\n", timetaken);
    return 0;
}

//
//int get_time_taken() {
//    int sum = 0;
//    for (int i = 0; i < MAX_PROCESSES; i++) {
//        sum += ProcessList[i].time_processed;
//    }
//    return sum;
//}