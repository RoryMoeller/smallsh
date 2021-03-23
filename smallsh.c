#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h> // dont forget to add -lm to compile args
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>

//formatting of data used to track application statuses
struct process_holder {
    int process_count;
    int capacity;
    int process_list[256];
    int last_process_exit_status;
    int died_with_signal;
    int r_i_b;
};

//global address of data struct so it can be accessed in signal handling
struct process_holder* proc_holder;

void handle_sigint(int sig){
    //write(2, "Terminated by signal\n", (sizeof(char) * 20));
    fprintf(stderr, "Terminated by signal %d\n", sig);
    fflush(stderr);
    fflush(stdout);
    proc_holder->died_with_signal = sig;
    //exit(1);
}

//check if any processes have exited & handle them
void check_processes(struct process_holder* ph){
    int i;
    int wstatus;
    int pid;
    for (i = 0; i < ph->process_count; i++){
        waitpid(ph->process_list[i], &wstatus, WNOHANG);
        if (WIFEXITED(wstatus)) {
            int statusCode = WEXITSTATUS(wstatus);
            if (statusCode == 0){
                printf("Background task finished\n[%d]\t%d\n", i, ph->process_list[i]);
            }else{
                printf("Background task finished with error status %d\n[%d]\t%d\n", statusCode, i, ph->process_list[i]);
                //printf("Exited with non-zero status... Errno: %d\n", statusCode);
            }
            fflush(stdout);
            ph->process_list[i] = ph->process_list[ph->process_count];
            ph->process_count--;
            i--;
        }
    }
}

//Toggle background allowed mode (r_i_b in proc holder)
void handle_tstp(int sig){
    if(proc_holder->r_i_b){
        printf("Running in background is now disabled.\n: ");
        proc_holder->r_i_b = 0;
    }else{
        printf("Running in background is now renabled.\n: ");
        proc_holder->r_i_b = 1;
    }
    fflush(stdout);
}

//Kill process that recieves a sigint (only use for foreground)
void die_on_sig_int(int sig){
    // fprintf(stderr, "REEEEEEEEEEEETerminated by signal %d\n", sig);
    check_processes(proc_holder);
    exit(0);
}

//Remove a specific pid from the proc holder
void remove_process(struct process_holder* proc_holder, int process_id){
    int i = 0;
    int removed = 0;
    if(proc_holder != NULL && proc_holder->process_list != NULL){
        for (i = 0; i <= proc_holder->process_count; i++){
            if( proc_holder->process_list[i] == process_id ){
                proc_holder->process_list[i] = proc_holder->process_list[proc_holder->process_count];
                proc_holder->process_list[proc_holder->process_count] = 0;
                proc_holder->process_count--;
                int removed = 1;
            }
        }
    }
    if (removed == 0){
        //printf("Background process with ID %d not found\n:", process_id);
    }
}

//Deprecated, would be useful if messages had to be sent immediately on the death of a child
void handle_sigchild(int sig){
    fprintf(stderr, "Terminated by signal %d\n", sig);

    if(proc_holder != NULL){
        if(sig == SIGCHLD){
            pid_t pid = wait(NULL);
            if(pid > 0){
                printf("\nBackground process %d exited.\n: ", pid);
                remove_process(proc_holder, pid);
            }else{
                //printf("\n:");
                
            }
        }else{
            printf("Not a sigchld");
            
        }
    fflush(stdout);
    fflush(stdin);
    }
    return;
}

//Empty function for ignoring signals... should probably just use SIG_IGN
void dont_handle_sigchild(int sig){
    // fprintf(stderr, "Skipped termination by signal %d\n", sig);
    ;
}

//Initialize variables & allocate memory for the passed proc holder
void init_proc_holder(struct process_holder* proc_holder ){
    proc_holder->capacity = 256;
    int i = 0;
    for(i = 0;i < 256; i++){
        proc_holder->process_list[i] = 0;
    }
    proc_holder->process_count = 0;  
    proc_holder->last_process_exit_status = 0;
    proc_holder->died_with_signal = 0;
    proc_holder->r_i_b = 1;
}

//Add a specific PID to the process holder
void add_process(struct process_holder* proc_holder, int process_id){
    if (proc_holder->process_count + 1 >= proc_holder->capacity){
        printf("Number of running processes cannot exceed 256!\n");
    }else{
        proc_holder->process_list[proc_holder->process_count] = process_id;
    }

    printf("[%d]\t%d\n", proc_holder->process_count, process_id);
    proc_holder->process_count+=1;
}

//Kill all processes & report errors in the proc holder
void kill_all_processes(struct process_holder* ph){
    int i = 0;
    for (i = 0;i < ph->process_count; i++){
        if (kill(ph->process_list[i], SIGKILL) >= 0){
            printf("Killed [%d]... ID: %d\n", i, ph->process_list[i]);
        }else if (errno == 3){
            printf("Failed to kill background process \n[%d]\t%d\nErrorNo: %d... Process has already been killed.\n", i, ph->process_list[i], errno);
        }else{
            printf("Failed to kill background process \n[%d]\t%d\nErrorNo: %d\n", i, ph->process_list[i], errno);
        }
    }
}

/*
* Cleanly exit the shell
* -Kill all processes. Free & NULL the process handler
* -Free argv & commandline
* -Exit with status 1
 */
void exit_smallsh(int smsh_argc, char** smsh_argv, char* commandline, struct process_holder* ph){
    //handle other pre-exit stuff here
    if (ph != NULL){
        if(ph->process_count > 0){
            printf("Warning: killing %d background processes\n-------------------\n", ph->process_count);
            kill_all_processes(ph);
        }
        free(ph);
        ph = NULL;
    }

    int i = 0;
    if(smsh_argc >= 0){ //dont free what doesnt exist!
        for (i = 1; i <= smsh_argc; i++){ //free everything from argv except the first arg (used in while condition)
            //printf("Freeing %s at pos %d\n", smsh_argv[i], i);
            free(smsh_argv[i]);
            smsh_argv[i] = NULL;
        }
    }

    if(smsh_argv[0] != NULL){
        free(smsh_argv[0]);
        smsh_argv[0] = NULL;
    }

    free(commandline);
    //printf("\n");

    //waitpid(0, &i, 0);

    //printf("waitpid status pointer: %d\n", i);
    exit(0);
}

//Change directory to argv[1] if argued path, if not change to home dir
void change_dir(int argc, char* argv[]){
    if(argc > 0){
        chdir(argv[1]);
    }else{
        int id = getuid();
        struct passwd *info = getpwuid(id);
        chdir(info->pw_dir);
    }

    return;
}

//Prints exit status or signal # of last exited foreground process
void status(struct process_holder* ph){
    if (ph->died_with_signal != 0){
        printf("Last process died by signal %d\n", ph->died_with_signal);
    }else{
        printf("Last process exited with status %d\n", ph->last_process_exit_status);
    }
    return;
}

/* 
Handles commands that are NOT locally handled by the shell
* 1. Fork
* CHILD
* 1.  Input redirection
* 2.  Set up signal handling
* 3.  Exec argv
* 4.  Exit status 1 & error message if not executed
* PARENT
* 1.  Set up SIGINT for foreground processes
* 2.  Retrieve exit status & update proc holder with status for foreground processes
* 3.  Background processes get added to the proc holder
 */
void handle_other(char** smsh_argv, int smsh_argc, struct process_holder* proc_holder, int run_in_background){
    int mainID = getpid();
    int input_redirection = 0;
    int output_redirection = 0;
    char* output_stream = NULL;
    char* input_stream = NULL;
    int pid = 0;
    int filedescriptor = 0;
    int i = 0;

    pid = fork();
    if (pid==0){
        //child process
        signal(SIGTSTP, SIG_IGN); //ignore sigtstp in all child processes

        for (i = 0; i < smsh_argc; i++){ //Input redirection
            if (smsh_argv[i] != NULL && strcmp(smsh_argv[i], ">") == 0){
                output_redirection = 1;
                output_stream = malloc(sizeof(char) * (1+strlen(smsh_argv[i+1])));
                strcpy(output_stream, smsh_argv[i+1]);
                free(smsh_argv[i]);
                free(smsh_argv[i + 1]);
                //smsh_argv[i] = malloc(sizeof(char) * 2);
                //smsh_argv[i+1] = malloc(sizeof(char) * 2);
                //strcpy(smsh_argv[i], " ");
                //strcpy(smsh_argv[i+1], " ");
                smsh_argv[i] = NULL;
                smsh_argv[i + 1] = NULL;
                filedescriptor = open(output_stream, O_WRONLY | O_CREAT | O_TRUNC, 0750);
                if (filedescriptor > 0){
                    //printf("Redirecting output to %s\n", output_stream);
                    dup2(filedescriptor, STDOUT_FILENO);
                }else{
                    printf("Error opening file for output redirection\n");
                    exit(1);
                }
                free(output_stream);
            }else if(smsh_argv[i] != NULL && strcmp(smsh_argv[i], "<") == 0){
                input_redirection = 1;
                input_stream = malloc(sizeof(char) * (1+strlen(smsh_argv[i+1])));
                strcpy(input_stream, smsh_argv[i+1]);
                free(smsh_argv[i]); //gotta remove redirects before executing the program
                free(smsh_argv[i + 1]);
                //smsh_argv[i] = malloc(sizeof(char) * 2);
                //smsh_argv[i+1] = malloc(sizeof(char) * 2);
                //strcpy(smsh_argv[i], " ");
                //strcpy(smsh_argv[i+1], " ");
                smsh_argv[i] = NULL;
                smsh_argv[i + 1] = NULL;
                filedescriptor = open(input_stream, O_RDONLY, 0750);
                if(filedescriptor > 0){
                    //printf("Redirecting input to %s", input_stream);
                    dup2(filedescriptor, STDIN_FILENO);
                }else{
                    printf("Cannot open file for input redirection\n");
                    exit(1);
                }
                free(input_stream);
            }
        }

        if(run_in_background){
            // signal(SIGINT, dont_handle_sigchild);
            signal(SIGINT, SIG_IGN);
            if (!input_redirection){ // if nothing specified for output
                char path[] = "/dev/null";
                filedescriptor = open(path, O_RDONLY, 0750);
                if (filedescriptor > 0){
                    //printf("Redirecting input to %s\n", path);
                    dup2(filedescriptor, STDIN_FILENO);
                }else{
                    printf("Error opening file for input redirection\n");
                    exit(1);
                }
            }
            if (!output_redirection){ // if nothing specified for input
                output_stream = malloc(sizeof(char) * 20);
                strcpy(output_stream, "/dev/null");
                filedescriptor = open(output_stream, O_WRONLY , 0750);
                if (filedescriptor > 0){
                    //printf("Redirecting output to /dev/null\n");
                    dup2(filedescriptor, 1);
                    fflush(stdout);
                }else{
                    printf("Error opening file for output redirection\n");
                    exit(1);
                }
                free(output_stream);
            } 
        }else{
            //printf("in foreground\n");
            signal(SIGINT, die_on_sig_int);
            //foreground processes
            
        }

        /*
        for(i = 0;i<=smsh_argc;i++){
            printf("argv[%d] = %s\n", i, smsh_argv[i]);
        }*/

        execvp(smsh_argv[0], smsh_argv);

        fprintf(stderr, "Program not found: %s\n", smsh_argv[0]);
        exit(1);

    }else{
        //parent process
        int wstatus = 0;
        if(!run_in_background){
            //foreground process.
            //signal(SIGCHLD, dont_handle_sigchild);
            signal(SIGINT, handle_sigint);
            waitpid(pid, &wstatus, 0);
            if (WIFEXITED(wstatus)) {
                int statusCode = WEXITSTATUS(wstatus);
                if (statusCode == 0){
                    //printf("very nice.\n");
                }else{
                    //printf("Exited with non-zero status... Errno: %d\n", statusCode);
                }
                proc_holder->last_process_exit_status = statusCode;
                proc_holder->died_with_signal = 0;
            }
        }else{
            //background process
            add_process(proc_holder, pid);
            // printf("Running process with pid = %d\n", pid);
            //signal(SIGCHLD, handle_sigchild);
            fflush(stdout);
            waitpid(pid, &wstatus, WNOHANG);
        }
    }
    return;
}

//Expand instances of "$$" inside arguments of smsh_argv into the pid
void variable_expansion(int argc, char*** smsh_argv){
    char** argv = *(smsh_argv);
    int i = 0;
    char finding[] = "$$";
    char* pos;
    int procID = getpid();
    char* newString = NULL;
    int length_of_id = floor(log10(procID));
    int length = 0;
    while (argv[i] != NULL){
        pos =  strstr(argv[i], finding);
        while(pos != NULL){
            length = strlen(argv[i]); //old length of string
            length = (length - 2 + length_of_id + 2); //length of new string with wiggle because my brain is small
            newString = malloc(sizeof(char) * length);
            *(pos) = '\0';
            sprintf(newString, "%s%d%s", argv[i], procID, pos+2);
            free(argv[i]);
            argv[i] = newString;
            newString = NULL;
            pos =  strstr(argv[i], finding);
        }
        i++;
    }
}

//General switch-like controller to manage program flow based on argv[0]
int handle_command(char** smsh_argv, int smsh_argc, char* commandline, struct process_holder* proc_holder, int background_allowed){
    variable_expansion(smsh_argc, &smsh_argv);
    
    int run_in_background = 0;

    if (strcmp( smsh_argv[smsh_argc], "&") == 0 && proc_holder->r_i_b){
        //printf("Running command in background...\n");
        run_in_background = 1;
        // strcpy(smsh_argv[smsh_argc], " ");
        free(smsh_argv[smsh_argc]);
        smsh_argv[smsh_argc] = NULL;
    }else if (strcmp( smsh_argv[smsh_argc], "&") == 0) {
        printf("Ignoring request to run in background\nRe-enable running background processes with ctrl+z\n:");
        free(smsh_argv[smsh_argc]);
        smsh_argv[smsh_argc] = NULL;
        // strcpy(smsh_argv[smsh_argc], " ");
    }
    
    if (strcmp(smsh_argv[0], "exit") == 0){
        exit_smallsh(smsh_argc, smsh_argv, commandline, proc_holder);
    }
    else if (strcmp(smsh_argv[0], "cd") == 0){
        change_dir(smsh_argc, smsh_argv);
    }
    else if (strcmp(smsh_argv[0], "status") == 0){
        status(proc_holder);
    }
    else{
        handle_other(smsh_argv, smsh_argc, proc_holder, run_in_background);
    }
    return 1;
}

//Sets up variables & manages the looping structure of the shell
int main() {
    int background_allowed = 1;
    int commandcount = 0;
    char* commandline;
    size_t length = 2048;
    int cmdlength = 0;
    int smsh_argc = 0;
    char* smsh_argv[512];
    commandline = (char*)calloc(255, sizeof(char));
    char** cmd_ptr = &commandline;
    char* cmdstring = commandline;
    char cwd[PATH_MAX];
    int i = 0;
    char* saveptr = NULL;
    char* token;
    int run_in_background;
    getcwd(cwd, sizeof(cwd));

    signal(SIGINT, dont_handle_sigchild);
    signal(SIGTSTP, handle_tstp);
    
    proc_holder = malloc(sizeof(struct process_holder));
    init_proc_holder(proc_holder);

    

    // struct sigaction sa;
    // sa.sa_handler = &sig_switch;

    for (i = 0; i < 512; i++){
        smsh_argv[i] = NULL;
    }

    do{
        run_in_background = proc_holder->r_i_b;
        signal(SIGINT, SIG_IGN);

        if(smsh_argc >= 0 && smsh_argv[0] != NULL){ //free argv[0] for commandcount >0 (used as the while condition, need to wait to free!)
            free(smsh_argv[0]);
            smsh_argv[0] = NULL;
        }
        memset(commandline, 0, sizeof(char)); //clear input
        //printf("smallsh > %s > %d$ ", cwd, commandcount++); //expanded version
        fflush(stdout);
        fflush(stdin);
        check_processes(proc_holder);
        printf(": ");
        fflush(stdout);
        fflush(stdin);
        getline(cmd_ptr, &length, stdin); //getting the actual command from stdin
        cmdlength = strlen(commandline); //GET RID OF before turning in

        i=0;
        saveptr = NULL; //reset the tracking pointer for strtok_r before reparsing
        for(token = strtok_r(cmdstring, " \n", &saveptr);
            token;
            token = strtok_r(NULL, " \n", &saveptr)){
                smsh_argv[i] = calloc(strlen(token) + 1, sizeof(char));
                strcpy(smsh_argv[i], token);
                //printf("%s\n", smsh_argv[i++]);
                i++;
        }
        smsh_argc = i -1; //setting argc based on number of iterations of strtok_r

        if(smsh_argc < 0){
            //printf("Constructing a placeholder comment in argv[0]\n");
            smsh_argv[0] = calloc(2, sizeof(char));
            strcpy(smsh_argv[0], "#");
            smsh_argc++;
        }


        if (smsh_argc >= 0 && smsh_argv[0][0] != '#'){ //not a comment
            handle_command(smsh_argv, smsh_argc, commandline, proc_holder, background_allowed);
        }else{
            //printf("This is a comment\n");
        }
        if(smsh_argc >= 0){ //dont free what doesnt exist!
            for (i = 1; i <= smsh_argc; i++){ //free everything from argv except the first arg (used in while condition)
                free(smsh_argv[i]);
                smsh_argv[i] = NULL;
            }
        }

    }while(strcmp(smsh_argv[0], "exit") != 0);

    return 300; //if execution somehow ended up here, return big number because that is bad
}

