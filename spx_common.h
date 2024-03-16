#ifndef SPX_COMMON_H
#define SPX_COMMON_H
#define _POSIX_C_SOURCE 199309L
#define _POSIX_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#define FIFO_EXCHANGE "/tmp/spx_exchange_%d"
#define FIFO_TRADER "/tmp/spx_trader_%d"
#define FEE_PERCENTAGE 1
#define MAX_PRODUCT_SIZE 18
#define MAX_FIFO_NAME 22
#define MAX_COMMAND 45
#define FD_PERM 0777

enum orderType {
    SELL,
    BUY,
    AMEND,
    CANCEL,
};

struct order {
    // 0 for Sell
    // 1 for Buy
    // 2 for Ammend
    // 3 for Cancel
    enum orderType orderType;
    int orderID;
    char product[MAX_PRODUCT_SIZE];
    int quantity;
    int price;
    int traderID;
};

struct market_book_node {
    struct order * order;
    struct market_book_node * prev;
    struct market_book_node * next;
};


struct fd_pair {
    int fd_exchange;
    int fd_trader;
};

struct product {
    char name[MAX_PRODUCT_SIZE];
};

struct potential_trade_order {
    struct market_book_node * order_node;
    int time;
};


#endif
