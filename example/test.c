#include "external.h"

void security_hook_a();
void buzz();
void print_number(int n);
void security_hook_b();
void security_hook_e();

void test(int n) {
	int i;
	for (i = 0; i < n; i++) {
		int div_3 = i % 3 == 0;
		int div_5 = i % 5 == 0;

		if (div_3 && div_5) {
			fizzbuzz(n);
			security_hook_e();
		} else if (div_3) {
			security_hook_a();
		} else if (div_5) {
			buzz();
		} else {
			print_number(i);
		}
	}

	security_hook_b();
}
