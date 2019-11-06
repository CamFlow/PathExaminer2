void security_hook_c();
void security_hook_d();
void foobar();

void fizzbuzz(int n) {
	int threshold = 10;
	if (n > threshold)
		security_hook_c();
	else if (n == threshold)
		foobar();
	else
		security_hook_d();	
	return;
}
