#include <stdio.h>

enum rank { 
STUDENT = 0, TEACHER 
}; 
typedef struct {
    char *name;
    Bank bank;
    char *id;
    char *pw;
    enum rank isadmin;
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


int main() {
    printf("Hello, WdddWWWWorld!");
}
