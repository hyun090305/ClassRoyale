#include <stdio.h>

typedef enum{ 
STUDENT = 0, TEACHER 
}rank; 

typedef struct {
    char *name;
    Bank bank;
    char *id;
    char *pw;
    rank isadmin;
    Item items[10];
    int completed_missions;
    int total_missions;
    Mission mission[99];
} User;

typedef struct {
    char *name;
    int balance;
    FILE *fp;
    char rating;
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
<<<<<<< HEAD
    printf("Class Royale");
=======
    printf("Hello, 생정파!");
>>>>>>> dd66d473fc1f3fad500da80fa8c136223859f01e
}
