/*
 * 파일 목적: 공용 타입 및 선언 제공
 * 작성자: 박성우
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdbool.h>
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
typedef enum {
    STUDENT = 0,
    TEACHER
} RankEnum;

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
typedef struct AssetPoint AssetPoint;
typedef struct Seat Seat;

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
    long last_interest_ts; /* epoch seconds when interest was last applied */
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
    int log[200];
    int log_len;
    char news[200];
};

struct User {
    char name[50];
    Bank bank;
    char pw[100];
    char id[50];
    RankEnum isadmin;
    Item items[10];
    int completed_missions;
    int total_missions;
    Mission missions[99];
    int mission_count;
    StockHolding holdings[MAX_HOLDINGS];
    int holding_count;
};

struct AssetPoint{
    long timestamp;
    long total_asset;
}; 

struct {
    char name[64];
} Seat;

#endif /* TYPES_H */
