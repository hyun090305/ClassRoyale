#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Macro configuration                                                        */
/* -------------------------------------------------------------------------- */
#define MAX_ITEM_SIZE 100
#define MAX_NAME_LEN 30
#define MAX_STUDENTS 50
#define MAX_MISSIONS 128
#define MAX_NOTIFICATIONS 256
#define MAX_HOLDINGS 16

#define CP_DEFAULT 1
#define BOX_WIDTH 40
#define BOX_HEIGHT 20

/* -------------------------------------------------------------------------- */
/*  Enumerations                                                               */
/* -------------------------------------------------------------------------- */
typedef enum rank {
    STUDENT = 0,
    TEACHER
} rank_t;

/* -------------------------------------------------------------------------- */
/*  Forward declarations                                                       */
/* -------------------------------------------------------------------------- */
typedef struct Item Item;
typedef struct Mission Mission;
typedef struct Shop Shop;
typedef struct Stock Stock;
typedef struct Bank Bank;
typedef struct StockHolding StockHolding;
typedef struct User User;

typedef struct StockHolding {
    char symbol[32];
    int qty;
} StockHolding;

struct Bank {
    char name[50];
    int balance; /* deposit/bank balance (backward-compatible) */
    int cash;   /* physical/portable cash (new) */
    int loan;   /* outstanding loan amount (new) */
    char log[128];
    char rating;
};

struct Item {
    char name[64];
    int stock;
    int cost;
};

struct Mission {
    int id;
    char name[64];
    int type;
    int reward;
    int completed;
};

struct Shop {
    char name[64];
    Item items[50];
    int income;
    int sales[50];
    int item_count;
};

struct Stock {
    char name[64];
    int id;
    int base_price;
    int current_price;
    int previous_price;
    char news[200];
    int dividend_per_tick;
};

struct User {
    char name[50];
    Bank bank;
    char pw[100];
    char id[50];
    rank_t isadmin;
    Item items[10];
    int completed_missions;
    int total_missions;
    Mission missions[99];
    int mission_count;
    StockHolding holdings[MAX_HOLDINGS];
    int holding_count;
};

#endif /* TYPES_H */
