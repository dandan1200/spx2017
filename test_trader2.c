#include "spx_trader.h"

#define MAX_FILENAME 50

int exchange = -1;
int trader = -1;
int orderid = 0;
int sig_flag = 0;
int market_open = 0;
char curr_out_line[MAX_COMMAND];
int accepted = 0;
FILE * temp_out = NULL;


char *strsep(char **stringp, const char *delim) {
    char *rv = *stringp;
    if (rv) {
        *stringp += strcspn(*stringp, delim);
        if (**stringp)
            *(*stringp)++ = '\0';
        else
            *stringp = 0; }
    return rv;
}

void sig_handler(int sig, siginfo_t * info, void * vp){
    printf("Signal recieved\n");
    char * data = calloc(MAX_COMMAND,1);
    read(exchange, data, MAX_COMMAND);
    char * data_sep = data;
    if (strcmp(data_sep, "MARKET OPEN;") == 0) {
        market_open = 1;
        printf("MARKET OPEN\n");
    }

    fprintf(temp_out, "%s\n", data_sep);

    free(data);

    return;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
    //Open test files
    char in_file_name[MAX_FILENAME] = "";
    sprintf(in_file_name, "tests/EndToEndTests/current_test/test_%d.in", atoi(argv[1]));
    FILE * in_file = fopen(in_file_name, "r");

    char out_file_name[MAX_FILENAME] = "";
    sprintf(out_file_name, "tests/EndToEndTests/current_test/temp_%d.out", atoi(argv[1]));
    temp_out = fopen(out_file_name, "w");

    //Setup signal handler
    struct sigaction sig;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_SIGINFO;
	sig.sa_sigaction = sig_handler;
	sigaction(SIGUSR1, &sig, NULL);

    //Connect to pipes
    char str1[MAX_FIFO_NAME] = "";
	sprintf(str1,FIFO_EXCHANGE,atoi(argv[1]));
	exchange = open(str1, O_RDONLY);
	char str2[MAX_FIFO_NAME] = "";
	sprintf(str2,FIFO_TRADER,atoi(argv[1]));
	trader = open(str2, O_WRONLY);
    
    int shutdown = 0;
    
    char curr_in_line[MAX_COMMAND];
    printf("Ready to start\n");
    
    sleep(2);
    printf("Reading\n");
    if (market_open == 1) {
        while (fgets(curr_in_line, MAX_COMMAND, in_file)) {
            
            curr_in_line[strlen(curr_in_line) - 1] = '\0';
            curr_out_line[strlen(curr_out_line) - 1] = '\0';
            printf("Sending: %s\n", curr_in_line);
            write(trader,curr_in_line,strlen(curr_in_line));
            kill(getppid(), SIGUSR1);
            sleep(1);
        }
    }
    
    sleep(1);
    


    printf("[TRADER] Closing\n");
    close(trader);
    close(exchange);
    return 0;
    
}
