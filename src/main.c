#include <stdio.h>
#include <stdlib.h>

#include "numbers.h"

typedef struct Context {
	size_t count;
} Context;

static unsigned long parse_unsigned_long(const char *str, const char *errmsg);
static void callback(void *arg, const Expr *expr);
static int compare_unsigned_long(const void *lptr, const void *rptr);

unsigned long parse_unsigned_long(const char *str, const char *errmsg) {
	char *endptr = NULL;
	unsigned long size = strtoul(str, &endptr, 10);
	if (!*str || *endptr) {
		fprintf(stderr, "%s: %s\n", errmsg, str);
		abort();
	}
	return size;
}

void callback(void *arg, const Expr *expr) {
	Context *ctx = (Context*)arg;
	printf("%3zu: ", ctx->count);
	expr_fprint(stdout, expr);
	putchar('\n');

	++ ctx->count;
}

int compare_unsigned_long(const void *lptr, const void *rptr) {
	unsigned long l = *(unsigned long*)lptr;
	unsigned long r = *(unsigned long*)rptr;
	return l < r ? -1 : r < l ? 1 : 0;
}

int main(int argc, char* argv[]) {

	if (argc < 4) {
		fprintf(stderr, "not enough arbuments\n");
		return 1;
	}

	const size_t tasks = parse_unsigned_long(argv[1], "number of tasks is not a number or out of range");
	const unsigned long target = parse_unsigned_long(argv[2], "target is not a number or out of range");
	const size_t count = (size_t)argc - 3;
	unsigned long *numbers = calloc(count, sizeof(unsigned long));

	if (!numbers) {
		perror("allocating numbers array");
		abort();
	}

	for (size_t index = 0; index < count; ++ index) {
		numbers[index] = parse_unsigned_long(argv[index + 3], "not a number or out of range");
	}

	qsort(numbers, count, sizeof(unsigned long), compare_unsigned_long);

	printf("target = %lu\n", target);
	printf("numbers = [%lu", numbers[0]);
	for (size_t index = 1; index < count; ++ index) {
		printf(", %lu", numbers[index]);
	}
	printf("]\n\nsolutions:\n");
	Context ctx;
	ctx.count = 1;
	numbers_solutions(tasks, target, numbers, count, callback, &ctx);

	return 0;
}
