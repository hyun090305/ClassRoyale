#include <stdio.h>

typedef struct {
    char *name;
    int balance;
    FILE *fp;
} Bank;

typedef struct {
    char *name;
    int stock;
    int cost;
} Item;

typedef struct {
    char *name;
    int type;
    int reward;
    int completed;
} Mission;

typedef struct {
    char *name;
    Item items[50];
    int income;
    int sales[50]; 
} Shop;

typedef struct {
    char *name;
    int id;
    int base_price;
    int current_price;
    int previous_price;
    char news[200];
} Stock;


int main() {
    printf("Hello, Class Royale!\n");
}
