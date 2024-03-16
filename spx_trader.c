#include "spx_trader.h"
int exchange = -1;
int trader = -1;
int orderid = 0;
int sig_flag = 0;
int accepted = 0;
struct order * curr_order = NULL;

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
    char * data = calloc(MAX_COMMAND,1);
    read(exchange, data, MAX_COMMAND);
    char * data_sep = data;
    char * cmd = strsep(&data_sep, " ");
    if (strcmp(cmd, "MARKET") == 0) {
        cmd = strsep(&data_sep, " ");

        if (strcmp(cmd, "SELL") == 0) {
            strcpy(curr_order->product, strsep(&data_sep, " "));
            curr_order->quantity = atoi(strsep(&data_sep, " "));
            curr_order->price = atoi(strsep(&data_sep, " "));
            
            sig_flag = 1;
        }
    }
    char accept[MAX_COMMAND] = "ACCEPTED";
    if (strcmp(cmd, accept) == 0) {
        accepted = 1;
        printf("Accepted\n");
    }
    free(data);

    return;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
    struct sigaction sig;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_SIGINFO;
	sig.sa_sigaction = sig_handler;
	sigaction(SIGUSR1, &sig, NULL);

    curr_order = malloc(sizeof(struct order));
    char str1[MAX_FIFO_NAME] = "";
	sprintf(str1,FIFO_EXCHANGE,atoi(argv[1]));
	exchange = open(str1, O_RDONLY);
	char str2[MAX_FIFO_NAME] = "";
	sprintf(str2,FIFO_TRADER,atoi(argv[1]));
	trader = open(str2, O_WRONLY);
    
    int shutdown = 0;
    
    
    printf("AT: Starting loop\n");

    while (shutdown == 0) {
        pause();
        if (sig_flag == 1){
            sig_flag = 0;
            if (curr_order->quantity >= 1000) {
                shutdown = 1;
                break;
            } else {
                char msg[MAX_COMMAND] = "";
                sprintf(msg,"BUY %d %s %d %d;",orderid, curr_order->product, curr_order->quantity, curr_order->price);
                
                write(trader,msg,strlen(msg));
                char accept[MAX_COMMAND] = "";
                sprintf(accept, "ACCEPTED %d",orderid);
                
                
                orderid+=1;
                
                while (accepted == 0) {
                    kill(getppid(),SIGUSR1);
                    sleep(1);
                    
                }
                accepted = 0;
            }
        }
    }
    

    
    sleep(1);
    

    free(curr_order);
    printf("[TRADER] Closing\n");
    close(trader);
    close(exchange);
    return 0;
    
}
