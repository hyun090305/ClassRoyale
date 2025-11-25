#include "../../include/domain/economy.h"

#include <math.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>

#include "../../include/domain/account.h"
#include "../../include/core/csv.h"

int econ_deposit(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    return account_adjust(acc, amount);
}

int econ_borrow(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    /* Note: legacy behavior increased balance; leave as-is for compatibility */
    return account_adjust(acc, amount);
}

int econ_repay(Bank *acc, int amount) {
    if (!acc || amount <= 0) {
        return 0;
    }
    return account_adjust(acc, -amount);
}

int econ_apply_hourly_interest(User *user, int hours) {
    if (!user || hours <= 0) return 0;

    /* rates */
    const double r_deposit = 0.001;  /* 0.1% per hour */
    const double r_loan = 0.0015;    /* 0.15% per hour */

    double bal = (double)user->bank.balance;
    double loan = (double)user->bank.loan;

    /* compound */
    double newbal_d = floor(bal * pow(1.0 + r_deposit, hours) + 0.0000001);
    double newloan_d = floor(loan * pow(1.0 + r_loan, hours) + 0.0000001);

    if (newbal_d < 0) newbal_d = 0;
    if (newloan_d < 0) newloan_d = 0;

    int newbal = (int)newbal_d;
    int newloan = (int)newloan_d;

    int bal_diff = newbal - user->bank.balance;
    int loan_diff = newloan - user->bank.loan;

    if (bal_diff != 0) {
        /* use account_adjust so balance log is updated */
        if (!account_adjust(&user->bank, bal_diff)) return 0;
        /* also write a transaction row for visibility */
        csv_ensure_dir("data");
        csv_ensure_dir("data/txs");
        char path[512];
        snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
        csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), "INTEREST_DEPOSIT", bal_diff, user->bank.balance);
    }

    if (loan_diff != 0) {
        long newloan_long = (long)user->bank.loan + loan_diff;
        if (newloan_long > INT_MAX) return 0;
        user->bank.loan = (int)newloan_long;
        /* persist a loan-interest row in txs for traceability */
        csv_ensure_dir("data");
        csv_ensure_dir("data/txs");
        char path[512];
        snprintf(path, sizeof(path), "data/txs/%s.csv", user->name);
        char reason[64];
        snprintf(reason, sizeof(reason), "INTEREST_LOAN_%dh", hours);
        csv_append_row(path, "%ld,%s,%+d,%d", (long)time(NULL), reason, loan_diff, user->bank.balance);
    }

    return 1;
}
