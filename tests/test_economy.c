#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/domain/economy.h"

int main(void) {
	User u;
	memset(&u, 0, sizeof(u));
	strcpy(u.name, "testuser");

	u.bank.balance = 100000; /* starting balance */
	u.bank.loan = 100000;    /* starting loan */

	if (!econ_apply_hourly_interest(&u, 1)) {
		fprintf(stderr, "econ_apply_hourly_interest failed\n");
		return 2;
	}

	/* expected: balance * 1.001 => 100100 (floor), loan * 1.0015 => 100150 (floor) */
	if (u.bank.balance != 100100) {
		fprintf(stderr, "Balance mismatch: got %d expected %d\n", u.bank.balance, 100100);
		return 3;
	}
	if (u.bank.loan != 100150) {
		fprintf(stderr, "Loan mismatch: got %d expected %d\n", u.bank.loan, 100150);
		return 4;
	}

	printf("economy interest test passed\n");
	return 0;
}
