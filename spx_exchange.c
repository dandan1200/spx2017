/**
 * comp2017 - assignment 3
 * Daniel Chorev
 * Dcho3009
 */

#include "spx_exchange.h"


//Global variables, to be used in signal handler.
int num_traders = 0;
int num_products = 0;
struct trader_info * traders = NULL;
struct product * product_list = NULL;
struct fd_pair * fifo_fds = NULL;
int market_initialised = 0;
long trader_fees = 0;
int * closed_traders = NULL;

struct market_book_node * buy_book_head = NULL;
struct market_book_node * sell_book_head = NULL;
//Implementation of strsep
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

//Create or check for existing named pipes for each trader.
struct fd_pair * create_fifos(char ** argv) {
	char str1[MAX_FIFO_NAME] = "";
	char str2[MAX_FIFO_NAME] = "";

	struct fd_pair * fds = malloc(sizeof(struct fd_pair) * num_traders);

	//Repeat for each trader. Index starting at 2 due to command line arguments.
	for (int i = 2; i < num_traders + 2; i++){
		//Create string for file name
		sprintf(str1,FIFO_EXCHANGE,i - 2);
		//Create named pipe
		if (mkfifo(str1, FD_PERM) == -1) {
			if (errno != EEXIST) {
				perror("Could not create fifo file\n");
				return fds;
			}
		}
		//Exchange stdout messaging
		printf(LOG_PREFIX);
		printf(" Created FIFO ");
		printf(FIFO_EXCHANGE,i - 2);
		printf("\n");

		sprintf(str2,FIFO_TRADER,i - 2);
		if (mkfifo(str2, FD_PERM) == -1) {
			if (errno != EEXIST) {
				perror("Could not create fifo file\n");
				return fds;
			}
		}
		printf(LOG_PREFIX);
		printf(" Created FIFO ");
		printf(FIFO_TRADER,i - 2);
		printf("\n");

		//Start trader
		pid_t pid = fork();
		if (pid == 0) {
			free(product_list);
			free_traders();
			char trader_id[MAX_FIFO_NAME] = "";
			sprintf(trader_id, "%d", i - 2);
			execl(argv[i], argv[i], (char *) trader_id, (char *) NULL);
		} else if (pid != -1) {
			//Save trader pid in parent.
			(traders + i - 2)->pid = pid;

		}

		printf(LOG_PREFIX);
		printf(" Starting trader %d (%s)\n", i - 2, argv[i]);

		//Open fifos
		char str3[MAX_FIFO_NAME] = "";
		sprintf(str3,FIFO_EXCHANGE,i - 2);
		(fds + i - 2)->fd_exchange = open(str3, O_WRONLY);

		printf(LOG_PREFIX);
		printf(" Connected to %s\n",str3);

		char str4[MAX_FIFO_NAME] = "";
		sprintf(str4,FIFO_TRADER,i - 2);
		(fds + i - 2)->fd_trader = open(str4, O_RDONLY);

		printf(LOG_PREFIX);
		printf(" Connected to %s\n",str4);

	}
	return fds;
}

//Send market open message
int market_open_msg(){
	const char * msg = "MARKET OPEN;";

	//Send message and signal to each trader
	for (int i = 0; i < num_traders; i++){
		write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
		kill((traders + i)->pid, SIGUSR1);
	}
	market_initialised = 1;

	return 0;
}

//Convert command to order struct
struct order * command_to_order(char * command, int traderID){
	//Initialise order
	struct order * converted_order = calloc(1, sizeof(struct order));
	char * search_command = command;
	char * key = strsep(&search_command, " ");
	converted_order->traderID = traderID;

	//Assign type
	if (strcmp("BUY", key) == 0){
		converted_order->orderType = BUY;
		
	} else if (strcmp("SELL", key) == 0) {
		converted_order->orderType = SELL;
		
	} else if (strcmp("AMEND", key) == 0) {
		converted_order->orderType = AMEND;
		
	} else if (strcmp("CANCEL", key) == 0) {
		converted_order->orderType = CANCEL;
		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->orderID = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
		return converted_order;
	}

	//Assign other details based on type.
	if (converted_order->orderType == BUY || converted_order->orderType == SELL){
		//Separates string by spaces and adds converted values to order struct
		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->orderID = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
		

		key = strsep(&search_command, " ");
		if (key != NULL) {
			strncpy(converted_order->product, key, MAX_PRODUCT_SIZE);
		} else {
			free(converted_order);
			return NULL;
		}
		

		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->quantity = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
		

		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->price = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
		
	} else if (converted_order->orderType == AMEND){
		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->orderID = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}

		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->quantity = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
		

		key = strsep(&search_command, " ");
		if (key != NULL) {
			converted_order->price = atoi(key);
		} else {
			free(converted_order);
			return NULL;
		}
	}
	
	return converted_order;
}

//Send market messages to all traders
void send_mkt_msg(struct order * o) {
	char msg[MAX_COMMAND] = "";
	if (o->orderType == BUY) {
		sprintf(msg, "MARKET BUY %s %d %d;", o->product, o->quantity, o->price);
	} else if (o->orderType == SELL) {
		sprintf(msg, "MARKET SELL %s %d %d;", o->product, o->quantity, o->price);
	} else if (o->orderType == AMEND) {
		sprintf(msg, "MARKET AMEND %s %d %d;", o->product, o->quantity, o->price);
	} else if (o->orderType == CANCEL) {
		sprintf(msg, "MARKET CANCEL %s %d %d;", o->product, o->quantity, o->price);
	}

	for (int i = 0; i < num_traders; i++){
		if (i != o->traderID) {
			if (*(closed_traders + i) == 0){
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill((traders + i)->pid, SIGUSR1);
			}
		}
	}

	return;
}

//Print the orderbook to stdout
void print_order_book(){
	printf(LOG_PREFIX);
	printf("	--ORDERBOOK--\n");

	//For each product
	for (int i = 0; i < num_products; i++) {

		//Initialise values
		int buy_levels = 0;
		int sell_levels = 0;
		int price = 0;
		struct potential_trade_order * possible_sell_orders = NULL;
		struct potential_trade_order * possible_buy_orders = NULL;
		
		//Get num sell orders for product
		int num_sell_matches = get_num_potential_orders((product_list + i)->name, __INT_MAX__, SELL);

		if (num_sell_matches > 0) {
			//Get all sell orders for product
			possible_sell_orders = get_time_priority_orders((product_list + i)->name, __INT_MAX__, SELL, num_sell_matches);
			sell_levels = 1;
			price = possible_sell_orders->order_node->order->price;

			//Count sell levels by iterating through all sell orders.
			for (int j = 1; j < num_sell_matches; j++){
				if ((possible_sell_orders + j)->order_node->order->price != price){
					sell_levels += 1;
					price = (possible_sell_orders + j)->order_node->order->price;
				}
			}
		}
		
		//Get num buy orders for products
		int num_buy_matches = get_num_potential_orders((product_list + i)->name, 0, BUY);
		if (num_buy_matches > 0){
			//Get all buy orders for product
			possible_buy_orders = get_time_priority_orders((product_list + i)->name, 0, BUY, num_buy_matches);
			buy_levels = 1;
			price = possible_buy_orders->order_node->order->price;

			//Count buy levels by iterating through all sell orders
			for (int j = 1; j < num_buy_matches; j++){
				if ((possible_buy_orders + j)->order_node->order->price != price){
					buy_levels += 1;
					price = (possible_buy_orders + j)->order_node->order->price;
				}
			}
		}

		//Print order levels
		printf(LOG_PREFIX);
		printf("	Product: %s; Buy levels: %d; Sell levels: %d\n", (product_list + i)->name, buy_levels, sell_levels);

		if (sell_levels > 0) {
			int quantity = possible_sell_orders->order_node->order->quantity;
			price = possible_sell_orders->order_node->order->price;
			int num_orders = 1;
			int has_printed = 0;

			//Iterate through all sell orders, first one intialised for comparison of price.
			for (int j = 1; j < num_sell_matches; j++) {

				//If a new price is found - new level
				if ((possible_sell_orders + j)->order_node->order->price != price){
					//Print current level
					printf(LOG_PREFIX);
					if (num_orders > 1) {
						printf("		SELL %d @ $%d (%d orders)\n", quantity, price, num_orders);
					} else {
						printf("		SELL %d @ $%d (%d order)\n", quantity, price, num_orders);
					}
					
					//Reset level counters and details for next comparison
					num_orders = 1;
					quantity = (possible_sell_orders + j)->order_node->order->quantity;
					price = (possible_sell_orders + j)->order_node->order->price;
					//Print in here if the end has been reached.
					if (j == num_sell_matches - 1){
						has_printed = 1;
						printf(LOG_PREFIX);
						if (num_orders > 1) {
							printf("		SELL %d @ $%d (%d orders)\n", quantity, price, num_orders);
						} else {
							printf("		SELL %d @ $%d (%d order)\n", quantity, price, num_orders);
						}
					}
				} else {
					//Otherwise, increase number of orders per level counter.
					num_orders++;
					quantity += (possible_sell_orders + j)->order_node->order->quantity;
				}
			}
			// If last order was a price repetiton, print outside loop
			if (has_printed == 0) {
				printf(LOG_PREFIX);
				if (num_orders > 1) {
					printf("		SELL %d @ $%d (%d orders)\n", quantity, price, num_orders);
				} else {
					printf("		SELL %d @ $%d (%d order)\n", quantity, price, num_orders);
				}
			}
			free(possible_sell_orders);
		}

		//Same as above for buy orders.
		if (buy_levels > 0) {
			
			int quantity = possible_buy_orders->order_node->order->quantity;
			price = possible_buy_orders->order_node->order->price;
			int num_orders = 1;
			int has_printed = 0;
			for (int j = 1; j < num_buy_matches; j++) {
				if ((possible_buy_orders + j)->order_node->order->price != price){
					printf(LOG_PREFIX);
					if (num_orders > 1) {
						printf("		BUY %d @ $%d (%d orders)\n", quantity, price, num_orders);
					} else {
						printf("		BUY %d @ $%d (%d order)\n", quantity, price, num_orders);
					}
					
					num_orders = 1;
					quantity = (possible_buy_orders + j)->order_node->order->quantity;
					price = (possible_buy_orders + j)->order_node->order->price;
					
					if (j == num_buy_matches - 1){
						has_printed = 1;
						printf(LOG_PREFIX);
						if (num_orders > 1) {
							printf("		BUY %d @ $%d (%d orders)\n", quantity, price, num_orders);
						} else {
							printf("		BUY %d @ $%d (%d order)\n", quantity, price, num_orders);
						}
					}
				} else {
					num_orders++;
					quantity += (possible_buy_orders + j)->order_node->order->quantity;
				}
			}
			if (has_printed == 0) {
				printf(LOG_PREFIX);
				if (num_orders > 1) {
					printf("		BUY %d @ $%d (%d orders)\n", quantity, price, num_orders);
				} else {
					printf("		BUY %d @ $%d (%d order)\n", quantity, price, num_orders);
				}
			}
			free(possible_buy_orders);
		}
	}
	return;
}

//Print trader positions to stdout
void print_positions(){
	printf(LOG_PREFIX);
	printf("	--POSITIONS--\n");

	//For each trader print positions in each product
	for (int i = 0; i < num_traders; i++){
		printf(LOG_PREFIX);
		printf("	Trader %d: ",i);
		int j = 0;
		for (j = 0; j < num_products - 1; j++){
			printf("%s %d ($%ld), ", ((traders + i)->positions + j)->name , ((traders + i)->positions + j)->quantity, ((traders + i)->positions + j)->value);
		}
		printf("%s %d ($%ld)\n", ((traders + i)->positions + j)->name, ((traders + i)->positions + j)->quantity, ((traders + i)->positions + j)->value);
	}
}

//Amend order command
int amend_order(struct order * new_o) {

	struct order * find_order = NULL;

	struct market_book_node * search_node = buy_book_head;
	int order_found = 0;

	//Look for order number in buy orders
	while (search_node != NULL) {
		if (search_node->order->orderID == new_o->orderID && search_node->order->traderID == new_o->traderID) {
			//When found, keep track of order, remove old order from list - new order to be added when buy/sell match attempt is executed.

			find_order = search_node->order;
			
			//Remove current order from list maintaining linked list.
			if (search_node->prev == NULL && search_node->next == NULL){
				buy_book_head = NULL;
			} else if (search_node->prev == NULL && search_node->next != NULL) {
				buy_book_head = search_node->next;
				buy_book_head->prev = NULL;
			} else if (search_node->prev != NULL && search_node->next == NULL) {
				search_node->prev->next = search_node->next;
			} else {
				search_node->prev->next = search_node->next;
				search_node->next->prev = search_node->prev;
			}
			order_found = 1;
			free(search_node);
			break;
		}
		search_node = search_node->next;
	}

	//Same as above for sell book.
	if (search_node == NULL) {
		search_node = sell_book_head;
		while (search_node != NULL) {
			if (search_node->order->orderID == new_o->orderID && search_node->order->traderID == new_o->traderID) {
				find_order = search_node->order;

				if (search_node->prev == NULL && search_node->next == NULL){
					sell_book_head = NULL;
				} else if (search_node->prev == NULL && search_node->next != NULL) {
					sell_book_head = search_node->next;
					sell_book_head->prev = NULL;
				} else if (search_node->prev != NULL && search_node->next == NULL) {
					search_node->prev->next = search_node->next;
				} else {
					search_node->prev->next = search_node->next;
					search_node->next->prev = search_node->prev;
				}
				free(search_node);
				order_found = 1;
				break;
			}
			search_node = search_node->next;
		}
	}

	//Change details to reuse new order for buy/sell
	if (find_order != NULL) {
		new_o->orderType = find_order->orderType;
		strcpy(new_o->product,find_order->product);
		free(find_order);
	}

	//Return whether order was found.
	return order_found;
}

//Cancel order command
int cancel_order(struct order * o, int order_id, int trader_id) {
	struct market_book_node * search_node = buy_book_head;
	int order_found = 0;

	//Look for order number in buy book
	while (search_node != NULL) {
		if (search_node->order->orderID == order_id && search_node->order->traderID == trader_id) {

			//When found, keep track of order type and details.
			o->orderType = search_node->order->orderType;
			strcpy(o->product,search_node->order->product);


			//Remove from linked list
			if (search_node->prev == NULL && search_node->next == NULL){
				buy_book_head = NULL;
			} else if (search_node->prev == NULL && search_node->next != NULL) {
				buy_book_head = search_node->next;
				buy_book_head->prev = NULL;
			} else if (search_node->prev != NULL && search_node->next == NULL) {
				search_node->prev->next = search_node->next;
			} else {
				search_node->prev->next = search_node->next;
				search_node->next->prev = search_node->prev;
			}
			free(search_node->order);
			free(search_node);
			
			order_found = 1;
			break;
		}
		search_node = search_node->next;
	}
	//Same as above for sell book.
	if (search_node == NULL) {
		search_node = sell_book_head;
		while (search_node != NULL) {
			if (search_node->order->orderID == order_id && search_node->order->traderID == trader_id) {
				o->orderType = search_node->order->orderType;
				strcpy(o->product,search_node->order->product);

				if (search_node->prev == NULL && search_node->next == NULL){
					sell_book_head = NULL;
				} else if (search_node->prev == NULL && search_node->next != NULL) {
					sell_book_head = search_node->next;
					sell_book_head->prev = NULL;
				} else if (search_node->prev != NULL && search_node->next == NULL) {
					search_node->prev->next = search_node->next;
				} else {
					search_node->prev->next = search_node->next;
					search_node->next->prev = search_node->prev;
				}
				free(search_node->order);
				free(search_node);
				order_found = 1;
				break;
			}
			search_node = search_node->next;
		}
	}
	return order_found;
}

//SIGUSR1 handler
void sig_handler(int sig, siginfo_t * info, void * vp){

	//Search for pid in pid list
	if (market_initialised == 0){
		return;
	}

	//Get pid of signal sender, find in trader list.
	pid_t sender_pid = info->si_pid;
	int found = 0;
	int i = 0;
	for (i = 0; i < num_traders; i++){
		if ((traders + i)->pid == sender_pid){
			found = 1;
			break;
		}
	}

	if (found == 1) {

		//Initialise values
		char * command = calloc(MAX_COMMAND,1);
		read((fifo_fds + i)->fd_trader, command, MAX_COMMAND);
		command[strlen(command) - 1] = 0;

		//Update exchange stdout
		printf(LOG_PREFIX);
		printf(" [T%d] Parsing command: <%s>\n", i, command);
		
		//Convert command to order struct
		struct order * o = command_to_order(command, i);
		char msg[MAX_COMMAND] = "";

		//Check for valid command
		if (o == NULL) {
			sprintf(msg,"INVALID;");
			write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
			kill(sender_pid,SIGUSR1);
			free(command);
			return;
		}

		int found_product = 0;
		for (int i = 0; i < num_products; i++) {
			if (strcmp(o->product,(product_list + i)->name) == 0) {
				found_product = 1;
			}
		}
		
		//Check for valid command
		if (o->orderType == BUY || o->orderType == SELL) {
			if ((traders + o->traderID)->order_id != o->orderID){
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}
			if (found_product == 0 || o->price <= 0 || o->quantity <= 0 || o->price > 999999 || o->quantity > 999999){
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}

		} else if (o->orderType == AMEND) {
			if ((traders + o->traderID)->order_id <= o->orderID){
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}
			if (o->price <= 0 || o->quantity <= 0 || o->price > 999999 || o->quantity > 999999){
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}

		} else {
			if ((traders + o->traderID)->order_id <= o->orderID){
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}
		}
		
		//Initial checks on command are valid.

		if (o->orderType == BUY){
			//Increment expected order id
			(traders + o->traderID)->order_id++;

			//Send accept message to trader
			sprintf(msg,"ACCEPTED %d;", o->orderID);
			write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
			kill(sender_pid,SIGUSR1);

			//Alert all traders of new order.
			send_mkt_msg(o);

			place_buy_order(o);
			print_order_book();
			print_positions();

		} else if (o->orderType == SELL) {
			//Increment expected order id
			(traders + o->traderID)->order_id++;

			//Send accept message to trader
			sprintf(msg,"ACCEPTED %d;", o->orderID);
			write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
			kill(sender_pid,SIGUSR1);

			//Alert all traders of new order
			send_mkt_msg(o);

			place_sell_order(o);
			print_order_book();
			print_positions();
		} else if (o->orderType == AMEND) {
			//Check if order is found.
			if (amend_order(o) == 1){
				//Send confirmation to trader
				sprintf(msg,"AMENDED %d;", o->orderID);
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				
				//Alert all traders of new order
				send_mkt_msg(o);
				
				//Place order
				if (o->orderType == BUY) {
					place_buy_order(o);
				} else if (o->orderType == SELL) {
					place_sell_order(o);
				}
				print_order_book();
				print_positions();
			} else {
				//If not found, send invalid
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}

			
		} else if (o->orderType == CANCEL) {
			//Same as above for cancel
			if (cancel_order(o, o->orderID, o->traderID) == 1) {

				sprintf(msg,"CANCELLED %d;", o->orderID);
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);

				send_mkt_msg(o);
				
				print_order_book();
				print_positions();
			} else {
				free(o);
				sprintf(msg,"INVALID;");
				write((fifo_fds + i)->fd_exchange, msg, strlen(msg));
				kill(sender_pid,SIGUSR1);
				free(command);
				return;
			}
			
			free(o);
		} else {
			//INVALID COMMAND
			free(o);
		}
		free(command);
	}
	return;
}

//Counts number of potential matchable orders
int get_num_potential_orders(char product[MAX_PRODUCT_SIZE], int price, enum orderType ot){
	int count = 0;
	
	//If function call for BUY order type
	if (ot == BUY){
		struct market_book_node * search_orders = buy_book_head;
		//For all orders in buy book linked list
		while (search_orders != NULL){
			// If correct product and price, increment count./
			if (strcmp(product, search_orders->order->product) == 0 && search_orders->order->price >= price) {
				count++;
			}
			search_orders = search_orders->next;
		}
	//Same for sell order function call.
	} else if (ot == SELL){
		struct market_book_node * search_orders = sell_book_head;
		while (search_orders != NULL){
			if (strcmp(product, search_orders->order->product) == 0 && search_orders->order->price <= price) {
				count++;
			}
			search_orders = search_orders->next;
		}
	}

	return count;
}

//Sort orders comparator
int sortOrders(const void * a, const void * b){
	const struct potential_trade_order * ptoa = (const struct potential_trade_order *) a;
	const struct potential_trade_order * ptob = (const struct potential_trade_order *) b;

	//Compare by price first
	if (ptoa->order_node->order->price > ptob->order_node->order->price) {
		return -1;
	} else if (ptoa->order_node->order->price < ptob->order_node->order->price) {
		return 1;
	//If equal compare by time.
	} else if (ptoa->time > ptob->time){
		return 1;
	} else {
		return -1;
	}

}

//Get all sorted orders matching price and product criteria
struct potential_trade_order * get_time_priority_orders(char product[MAX_PRODUCT_SIZE], int price, enum orderType ot, int num_orders){
	
	struct potential_trade_order * return_priority_orders = malloc(sizeof(struct potential_trade_order)*num_orders);

	//If function call for buy orders
	if (ot == BUY){
		struct market_book_node * search_orders = buy_book_head;
		int i = 0;
		int t = 0;
		//For all buy orders
		while (search_orders != NULL){
			//If buy order product and price correct, append to list of orders
			if (strcmp(product, search_orders->order->product) == 0 && search_orders->order->price >= price) {
				(return_priority_orders + i)->order_node = search_orders;
				(return_priority_orders + i)->time = t;
				i++;
			}
			t++;
			search_orders = search_orders->next;
		}

	//Same for sell orders
	} else if (ot == SELL){
		struct market_book_node * search_orders = sell_book_head;
		int i = 0;
		int t = 0;
		while (search_orders != NULL){
			if (strcmp(product, search_orders->order->product) == 0 && search_orders->order->price <= price) {
				(return_priority_orders + i)->order_node = search_orders;
				(return_priority_orders + i)->time = t;
				i++;
			}
			t++;
			search_orders = search_orders->next;
		}
	}

	//Sort by price-time priority
	qsort(return_priority_orders,num_orders,sizeof(struct potential_trade_order), sortOrders);

	return return_priority_orders;
}

//Place buy order function
void place_buy_order(struct order * o){
	//Case 1: No sell orders yet.
	if (sell_book_head == NULL) {
		//If no buy orders, simply add order to head of buy book
		if (buy_book_head == NULL) {
			buy_book_head = calloc(1, sizeof(struct market_book_node));
			buy_book_head->order = o;
			buy_book_head->prev = NULL;
			buy_book_head->next = NULL;
		} else {
			//Find end of buy list and add order to end.
			struct market_book_node * buy_book_search = buy_book_head;
			while (buy_book_search->next != NULL) {
				buy_book_search = buy_book_search->next;
			}

			buy_book_search->next = calloc(1, sizeof(struct market_book_node));
			buy_book_search->next->order = o;
			buy_book_search->next->prev = buy_book_search;
			buy_book_search->next->next = NULL;
		}
	} else {
		//Returns all orders of same product in time-price priority order.
		//First int is the size of the array
		int num_matches = get_num_potential_orders(o->product, o->price, SELL);
		struct potential_trade_order * possible_sell_orders = get_time_priority_orders(o->product, o->price, SELL, num_matches);

		//Iterate through potential orders for matches.
		for (int i = 0; i < num_matches; i++){
			//If sell order is bigger than buy order, enough to fill with just this order
			if (o->quantity < (possible_sell_orders + i)->order_node->order->quantity) {
				//Decrement quantity
				(possible_sell_orders + i)->order_node->order->quantity -= o->quantity;
				
				//Print match details.
				printf(LOG_PREFIX);
				printf(" Match: Order %d [T%d],", (possible_sell_orders + i)->order_node->order->orderID, (possible_sell_orders + i)->order_node->order->traderID);
				printf(" New Order %d [T%d],", o->orderID, o->traderID);
				
				//Calculate value and fee and print
				long value = (long) ((long) o->quantity * (long) (possible_sell_orders + i)->order_node->order->price);
				long fee = (long) ((long) value / 100);
				if (value % 100 >= 50) {
					fee++;
				}
				trader_fees += fee;
				printf(" value: $%ld, fee: $%ld.\n", value, fee);

				//Find product position in product array
				int prod_pos = 0;
				for (prod_pos = 0; prod_pos < num_products; prod_pos++) {
					if (strcmp(o->product,(product_list + prod_pos)->name) == 0) {
						break;
					}
				}

				//Update trader positions.
				((traders + o->traderID)->positions + prod_pos)->quantity += o->quantity;
				((traders + o->traderID)->positions + prod_pos)->value -= (value + fee);

				((traders + (possible_sell_orders + i)->order_node->order->traderID)->positions + prod_pos)->quantity -= o->quantity;
				((traders + (possible_sell_orders + i)->order_node->order->traderID)->positions + prod_pos)->value += value;

				if (o->traderID == (possible_sell_orders + i)->order_node->order->traderID && o->orderID < (traders + (possible_sell_orders + i)->order_node->order->traderID)->order_id) {
					//Send fill message
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, o->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);
					
					if (*(closed_traders + (possible_sell_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_sell_orders + i)->order_node->order->orderID, o->quantity);
						write((fifo_fds + (possible_sell_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_sell_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}
					

					
					
				} else {
					//Send fill message
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, o->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);
					//Check if matching order trader is disconnected before sending fill message.
					if (*(closed_traders + (possible_sell_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_sell_orders + i)->order_node->order->orderID, o->quantity);
						write((fifo_fds + (possible_sell_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_sell_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}

					
				}

				
				
				free(o);
				free(possible_sell_orders);
				return;
			} else {
				//Not enough quantity to totally fill order.
				//Partial match fill.

				printf(LOG_PREFIX);
				printf(" Match: Order %d [T%d],", (possible_sell_orders + i)->order_node->order->orderID, (possible_sell_orders + i)->order_node->order->traderID);
				printf(" New Order %d [T%d],", o->orderID, o->traderID);

				long value = (long) ((long) (possible_sell_orders + i)->order_node->order->quantity * (long) (possible_sell_orders + i)->order_node->order->price);
				long fee = (long) ((long) value / 100);
				if (value % 100 >= 50) {
					fee++;
				}
				trader_fees += fee;
				
				printf(" value: $%ld, fee: $%ld.\n", value, fee);

				int prod_pos = 0;
				for (prod_pos = 0; prod_pos < num_products; prod_pos++) {
					if (strcmp(o->product,(product_list + prod_pos)->name) == 0) {
						break;
					}
				}

				((traders + o->traderID)->positions + prod_pos)->quantity += (possible_sell_orders + i)->order_node->order->quantity;
				((traders + o->traderID)->positions + prod_pos)->value -= (value + fee);

				((traders + (possible_sell_orders + i)->order_node->order->traderID)->positions + prod_pos)->quantity -= (possible_sell_orders + i)->order_node->order->quantity;
				((traders + (possible_sell_orders + i)->order_node->order->traderID)->positions + prod_pos)->value += value;
				
				if (o->traderID == (possible_sell_orders + i)->order_node->order->traderID && o->orderID < (traders + (possible_sell_orders + i)->order_node->order->traderID)->order_id) {
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, (possible_sell_orders + i)->order_node->order->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);

					if (*(closed_traders + (possible_sell_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_sell_orders + i)->order_node->order->orderID, (possible_sell_orders + i)->order_node->order->quantity);
						write((fifo_fds + (possible_sell_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_sell_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}
					
					
					
				} else {
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, (possible_sell_orders + i)->order_node->order->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);

					if (*(closed_traders + (possible_sell_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_sell_orders + i)->order_node->order->orderID, (possible_sell_orders + i)->order_node->order->quantity);
						write((fifo_fds + (possible_sell_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_sell_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}

					
				}

				

			
				//Decrement needed quantity of order being processed.
				o->quantity -= (possible_sell_orders + i)->order_node->order->quantity;
				

				//Remove order that has been filled completely in linked list.
				if ((possible_sell_orders + i)->order_node->prev == NULL && (possible_sell_orders + i)->order_node->next == NULL){
					free((possible_sell_orders + i)->order_node->order);
					free((possible_sell_orders + i)->order_node);
					sell_book_head = NULL;
				} else if ((possible_sell_orders + i)->order_node->prev == NULL && (possible_sell_orders + i)->order_node->next != NULL) {
					sell_book_head = (possible_sell_orders + i)->order_node->next;
					sell_book_head->prev = NULL;
					free((possible_sell_orders + i)->order_node->order);
					free((possible_sell_orders + i)->order_node);
				} else if ((possible_sell_orders + i)->order_node->prev != NULL && (possible_sell_orders + i)->order_node->next == NULL) {
					(possible_sell_orders + i)->order_node->prev->next = (possible_sell_orders + i)->order_node->next;
					free((possible_sell_orders + i)->order_node->order);
					free((possible_sell_orders + i)->order_node);
				} else {
					(possible_sell_orders + i)->order_node->prev->next = (possible_sell_orders + i)->order_node->next;
					(possible_sell_orders + i)->order_node->next->prev = (possible_sell_orders + i)->order_node->prev;
					free((possible_sell_orders + i)->order_node->order);
					free((possible_sell_orders + i)->order_node);
				}
			}
		}

		if (o->quantity == 0) {
			free(o);
			free(possible_sell_orders);
			return;
		}

		//Add rest of order to book
		struct market_book_node * buy_book_search = buy_book_head;
		if (buy_book_head == NULL) {
			buy_book_head = calloc(1, sizeof(struct market_book_node));
			buy_book_head->order = o;
			buy_book_head->prev = NULL;
			buy_book_head->next = NULL;
		} else {
			while (buy_book_search->next != NULL) {
				buy_book_search = buy_book_search->next;
			}

			buy_book_search->next = calloc(1, sizeof(struct market_book_node));
			buy_book_search->next->order = o;
			buy_book_search->next->prev = buy_book_search;
			buy_book_search->next->next = NULL;
			
		}
		free(possible_sell_orders);

	}
	return;
}

//Place sell order function, logic same as buy order but different order book.
void place_sell_order(struct order * o){
	//Case 1: No sell orders yet.
	if (buy_book_head == NULL) {
		if (sell_book_head == NULL) {
			sell_book_head = calloc(1, sizeof(struct market_book_node));
			sell_book_head->order = o;
			sell_book_head->prev = NULL;
			sell_book_head->next = NULL;
		} else {
			struct market_book_node * sell_book_search = sell_book_head;
			while (sell_book_search->next != NULL) {
				sell_book_search = sell_book_search->next;
			}

			sell_book_search->next = calloc(1, sizeof(struct market_book_node));
			sell_book_search->next->order = o;
			sell_book_search->next->prev = sell_book_search;
			sell_book_search->next->next = NULL;
		}
	} else {
		//Returns all orders of same product in time-price priority order.
		//First int is the size of the array
		int num_matches = get_num_potential_orders(o->product, o->price, BUY);
		struct potential_trade_order * possible_buy_orders = get_time_priority_orders(o->product, o->price, BUY, num_matches);

		for (int i = 0; i < num_matches; i++){
			if (o->quantity < (possible_buy_orders + i)->order_node->order->quantity) {
				(possible_buy_orders + i)->order_node->order->quantity -= o->quantity;

				
				printf(LOG_PREFIX);
				printf(" Match: Order %d [T%d],", (possible_buy_orders + i)->order_node->order->orderID, (possible_buy_orders + i)->order_node->order->traderID);
				printf(" New Order %d [T%d],", o->orderID, o->traderID);
				
				long value = (long) ((long) o->quantity * (long)(possible_buy_orders + i)->order_node->order->price);
				long fee = (long)((long) value / 100);
				if (value % 100 >= 50) {
					fee++;
				}
				trader_fees += fee;

				printf(" value: $%ld, fee: $%ld.\n", value, fee);

				int prod_pos = 0;
				for (prod_pos = 0; prod_pos < num_products; prod_pos++) {
					if (strcmp(o->product,(product_list + prod_pos)->name) == 0) {
						break;
					}
				}

				((traders + o->traderID)->positions + prod_pos)->quantity -= o->quantity;
				((traders + o->traderID)->positions + prod_pos)->value += (value - fee);

				((traders + (possible_buy_orders + i)->order_node->order->traderID)->positions + prod_pos)->quantity += o->quantity;
				((traders + (possible_buy_orders + i)->order_node->order->traderID)->positions + prod_pos)->value -= value;

				// if (o->traderID == (possible_buy_orders + i)->order_node->order->traderID && o->orderID < (traders + (possible_buy_orders + i)->order_node->order->traderID)->order_id) {
				// 	if (*(closed_traders + (possible_buy_orders + i)->order_node->order->traderID) == 0){
				// 		char msg2[MAX_COMMAND] = "";
				// 		sprintf(msg2, "FILL %d %d;", (possible_buy_orders + i)->order_node->order->orderID, o->quantity);
				// 		write((fifo_fds + (possible_buy_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
				// 		kill((traders + (possible_buy_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
				// 	}
				// 	char msg[MAX_COMMAND] = "";
				// 	sprintf(msg, "FILL %d %d;", o->orderID, o->quantity);
				// 	write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
				// 	kill((traders + o->traderID)->pid, SIGUSR1);
					

					
				// } else {
					if (*(closed_traders + (possible_buy_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_buy_orders + i)->order_node->order->orderID, o->quantity);
						write((fifo_fds + (possible_buy_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_buy_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, o->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);
					
				//}
				
				

				free(o);
				free(possible_buy_orders);

				return;
			} else {
				
				printf(LOG_PREFIX);
				printf(" Match: Order %d [T%d],", (possible_buy_orders + i)->order_node->order->orderID, (possible_buy_orders + i)->order_node->order->traderID);
				printf(" New Order %d [T%d],", o->orderID, o->traderID);

				long value = (long) ((long)(possible_buy_orders + i)->order_node->order->quantity * (long) (possible_buy_orders + i)->order_node->order->price);
				long fee = (long)((long) value / 100);
				if (value % 100 >= 50) {
					fee++;
				}
				trader_fees += fee;

				printf(" value: $%ld, fee: $%ld.\n", value, fee);

				int prod_pos = 0;
				for (prod_pos = 0; prod_pos < num_products; prod_pos++) {
					if (strcmp(o->product,(product_list + prod_pos)->name) == 0) {
						break;
					}
				}

				((traders + o->traderID)->positions + prod_pos)->quantity -= (possible_buy_orders + i)->order_node->order->quantity;
				((traders + o->traderID)->positions + prod_pos)->value += (value - fee);

				((traders + (possible_buy_orders + i)->order_node->order->traderID)->positions + prod_pos)->quantity += (possible_buy_orders + i)->order_node->order->quantity;
				((traders + (possible_buy_orders + i)->order_node->order->traderID)->positions + prod_pos)->value -= value;
				
				// if (o->traderID == (possible_buy_orders + i)->order_node->order->traderID && o->orderID < (traders + (possible_buy_orders + i)->order_node->order->traderID)->order_id) {
				// 	if (*(closed_traders + (possible_buy_orders + i)->order_node->order->traderID) == 0){
				// 		char msg2[MAX_COMMAND] = "";
				// 		sprintf(msg2, "FILL %d %d;", (possible_buy_orders + i)->order_node->order->orderID, (possible_buy_orders + i)->order_node->order->quantity);
				// 		write((fifo_fds + (possible_buy_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
				// 		kill((traders + (possible_buy_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
				// 	}
				// 	char msg[MAX_COMMAND] = "";
				// 	sprintf(msg, "FILL %d %d;", o->orderID, (possible_buy_orders + i)->order_node->order->quantity);
				// 	write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
				// 	kill((traders + o->traderID)->pid, SIGUSR1);
					
					
					
					
				// } else {
					if (*(closed_traders + (possible_buy_orders + i)->order_node->order->traderID) == 0){
						char msg2[MAX_COMMAND] = "";
						sprintf(msg2, "FILL %d %d;", (possible_buy_orders + i)->order_node->order->orderID, (possible_buy_orders + i)->order_node->order->quantity);
						write((fifo_fds + (possible_buy_orders + i)->order_node->order->traderID)->fd_exchange, msg2, strlen(msg2));
						kill((traders + (possible_buy_orders + i)->order_node->order->traderID)->pid, SIGUSR1);
					}
					char msg[MAX_COMMAND] = "";
					sprintf(msg, "FILL %d %d;", o->orderID, (possible_buy_orders + i)->order_node->order->quantity);
					write((fifo_fds + o->traderID)->fd_exchange, msg, strlen(msg));
					kill((traders + o->traderID)->pid, SIGUSR1);
					
				//}

				
				o->quantity -= (possible_buy_orders + i)->order_node->order->quantity;
				
				if ((possible_buy_orders + i)->order_node->prev == NULL && (possible_buy_orders + i)->order_node->next == NULL){
					free((possible_buy_orders + i)->order_node->order);
					free((possible_buy_orders + i)->order_node);
					buy_book_head = NULL;
				} else if ((possible_buy_orders + i)->order_node->prev == NULL && (possible_buy_orders + i)->order_node->next != NULL) {
					buy_book_head = (possible_buy_orders + i)->order_node->next;
					buy_book_head->prev = NULL;
					free((possible_buy_orders + i)->order_node->order);
					free((possible_buy_orders + i)->order_node);
				} else if ((possible_buy_orders + i)->order_node->prev != NULL && (possible_buy_orders + i)->order_node->next == NULL) {
					(possible_buy_orders + i)->order_node->prev->next = (possible_buy_orders + i)->order_node->next;
					free((possible_buy_orders + i)->order_node->order);
					free((possible_buy_orders + i)->order_node);
				} else {
					(possible_buy_orders + i)->order_node->prev->next = (possible_buy_orders + i)->order_node->next;
					(possible_buy_orders + i)->order_node->next->prev = (possible_buy_orders + i)->order_node->prev;
					free((possible_buy_orders + i)->order_node->order);
					free((possible_buy_orders + i)->order_node);
				}
				
			}
		}

		if (o->quantity == 0) {
			free(o);
			free(possible_buy_orders);
			return;
		}

		//Add rest of order to book
		struct market_book_node * sell_book_search = sell_book_head;
		if (sell_book_search == NULL) {
			sell_book_head = calloc(1, sizeof(struct market_book_node));
			sell_book_head->order = o;
			sell_book_head->prev = NULL;
			sell_book_head->next = NULL;
		} else {
			while (sell_book_search->next != NULL) {
				sell_book_search = sell_book_search->next;
			}

			sell_book_search->next = calloc(1, sizeof(struct market_book_node));
			sell_book_search->next->order = o;
			sell_book_search->next->prev = sell_book_search;
			sell_book_search->next->next = NULL;
		}
		
		free(possible_buy_orders);
	}
	return;
}

//Free all memory for market books.
void free_market_books(){
	struct market_book_node * node = buy_book_head;
	if (node != NULL) {
		while (node->next != NULL){
			free(node->order);
			node = node->next;
			free(node->prev);
		}
		free(node->order);
	}
	free(node);

	node = sell_book_head;
	if (node != NULL) {
		while (node->next != NULL){
			free(node->order);
			node = node->next;
			free(node->prev);
		}
		free(node->order);
		
	}
	free(node);
	
	return;
}

//Delete fifos
void shutdown(){
	
	char str1[MAX_FIFO_NAME] = "";
	char str2[MAX_FIFO_NAME] = "";

	for (int i = 2; i < num_traders + 2; i++){
		sprintf(str1,FIFO_EXCHANGE,i - 2);
		sprintf(str2,FIFO_TRADER,i - 2);

		remove(str1);
		remove(str2);
	}
	return;
}

//Free trader info memory
void free_traders() {
	for (int i = 0; i < num_traders; i++){
		free((traders + i)->positions);
	}
	free(traders);
}

//Create trader struct with positions for each product.
void initialise_traders(){
	for (int i = 0; i < num_traders; i++){
		(traders + i)->positions = calloc(num_products, sizeof(struct position));
		(traders + i)->order_id = 0;

		for (int j = 0; j < num_products; j++) {
			strcpy(((traders + i)->positions + j)->name, (product_list + j)->name);
		}
	}
}

int main(int argc, char ** argv) {
	
	//Deal with command line args
	if (argc < 2) {
		perror("Not enough arguments\n");
		return 1;
	}

	//Read product file
	FILE * prod_file = fopen(argv[1], "r");

	//Read products
	char n_items[MAX_PRODUCT_SIZE] = "";
	fgets(n_items,MAX_PRODUCT_SIZE,prod_file);
	num_products = atoi(n_items);

	//Store number of traders
	num_traders = argc - 2;

	//Starting sequence
	printf(LOG_PREFIX);
	printf(" Starting\n");

	//Copy products into array and print products
	printf(LOG_PREFIX);
	printf(" Trading %d products: ",num_products);
	product_list = calloc(num_products,sizeof(struct product));
	for (int i = 0; i < num_products; i++){
		fgets((product_list + i)->name ,MAX_PRODUCT_SIZE,prod_file);
		(product_list + i)->name[strlen((product_list + i)->name)- 1] = '\0';
		if (i < num_products - 1) {
			printf("%s ",(product_list + i)->name);
		}
	}
	fclose(prod_file);
	printf("%s\n",(product_list + num_products - 1)->name);

	traders = calloc(num_traders, sizeof(struct trader_info));
	initialise_traders();

	//Create fifos
	fifo_fds = create_fifos(argv);

	
	//Signal handler definition
	struct sigaction sig;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_SIGINFO;
	sig.sa_sigaction = sig_handler;
	sigaction(SIGUSR1, &sig, NULL);
	
	//Initialise poll structures
	struct pollfd * pollfds_exchange = malloc(sizeof(struct pollfd) * num_traders);
	struct pollfd * pollfds_trader = malloc(sizeof(struct pollfd) * num_traders);
	
	//Add exchange file descriptors
	for (int i = 0; i < num_traders; i++) {
		(pollfds_exchange + i)->fd = (fifo_fds + i)->fd_exchange;
		(pollfds_exchange + i)->events = POLLHUP;
	}

	//Add trader file destriptors
	for (int i = 0; i < num_traders; i++) {
		(pollfds_trader + i)->fd = (fifo_fds + i)->fd_trader;
		(pollfds_trader + i)->events = POLLHUP;
	}

	int complete = 0;

	//Market open msg
	market_open_msg();

	//Initialise closed trader list
	closed_traders = calloc(num_traders, num_traders * sizeof(int));

	//While some traders are still connected.
	while (complete != num_traders) {
		nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
		//Poll trader and exchange fds.
		//int num_disconnect = poll(pollfds_exchange, num_traders, 250);
		int num_disconnect = poll(pollfds_trader, num_traders, 0);

		if (num_disconnect > 0) {
			//If there is a disconnection, find trader and add to list.
			for (int i = 0; i < num_traders; i++){
				if ((pollfds_trader + i)->revents == POLLHUP) {
					if (*(closed_traders + i) == 0){
						printf(LOG_PREFIX);
						printf(" Trader %d disconnected\n", i);
						*(closed_traders + i) = 1;
						complete += 1;
					}
					
				}
			}
		}
		
	}

	//Clean up all used memory and gracefully exit.
	free(closed_traders);

	printf(LOG_PREFIX);
	printf(" Trading completed\n");
	printf(LOG_PREFIX);
	printf(" Exchange fees collected: $%ld\n", trader_fees);
	free(pollfds_exchange);
	free(pollfds_trader);
	free_traders();
	free(fifo_fds);
	free(product_list);
	free_market_books();
	shutdown();
	return 0;
}


