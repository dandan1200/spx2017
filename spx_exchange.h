#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"

#define LOG_PREFIX "[SPX]"

struct position {
    char name[MAX_PRODUCT_SIZE];
    int quantity;
    long value;
};

struct trader_info {
    pid_t pid;
    int order_id;
    struct position * positions;
};

int get_num_potential_orders(char product[MAX_PRODUCT_SIZE], int price, enum orderType ot);
struct potential_trade_order * get_time_priority_orders(char product[MAX_PRODUCT_SIZE], int price, enum orderType ot, int num_orders);
void place_buy_order(struct order * o);
void place_sell_order(struct order * o);
void free_traders();
#endif
