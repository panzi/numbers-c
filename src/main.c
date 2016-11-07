#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#define __WINDOWS__
#endif

#if !defined(__WINDOWS__)
#include <unistd.h>
#endif

#include "numbers.h"
#include "panic.h"

typedef struct Context {
	size_t count;
} Context;

static unsigned long parse_unsigned_long(const char *str, const char *errmsg);
static void callback(void *arg, const Expr *expr);
static int compare_unsigned_long(const void *lptr, const void *rptr);

#ifdef _SC_NPROCESSORS_ONLN
static size_t get_cpu_count();
#endif

unsigned long parse_unsigned_long(const char *str, const char *errmsg) {
	char *endptr = NULL;
	unsigned long size = strtoul(str, &endptr, 10);
	if (!*str || *endptr) {
		panicf("%s: %s", errmsg, str);
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

#ifdef _SC_NPROCESSORS_ONLN
size_t get_cpu_count() {
	const long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 0) {
		panice("getting number of CPUs/cores");
	}
	return (size_t)nprocs;
}
#endif

int main(int argc, char* argv[]) {
	if (argc < 4) {
		fprintf(stderr, "not enough arguments\n");
		return 1;
	}

#ifdef _SC_NPROCESSORS_ONLN
	const size_t tasks = strcmp(argv[1], "-") == 0 ? get_cpu_count() :
#else
	const size_t tasks =
#endif
		parse_unsigned_long(argv[1], "number of tasks is not a number or out of range");

	const unsigned long target = parse_unsigned_long(argv[2], "target is not a number or out of range");
	const size_t count = (size_t)argc - 3;

	if (tasks == 0) {
		panicf("number of tasks has to be >= 1");
	}

	if (count > sizeof(size_t) * 8) {
		panicf("only up to %lu numbers supported", sizeof(size_t) * 8);
	}

	unsigned long *numbers = calloc(count, sizeof(unsigned long));

	if (!numbers) {
		panice("allocating numbers array");
	}

	for (size_t index = 0; index < count; ++ index) {
		numbers[index] = parse_unsigned_long(argv[index + 3], "not a number or out of range");
	}

	qsort(numbers, count, sizeof(unsigned long), compare_unsigned_long);

	printf("tasks = %zu\n", tasks);
	printf("target = %lu\n", target);
	printf("numbers = [%lu", numbers[0]);
	for (size_t index = 1; index < count; ++ index) {
		printf(", %lu", numbers[index]);
	}
	printf("]\n\nsolutions:\n");
	Context ctx;
	ctx.count = 1;
	numbers_solutions(tasks, target, numbers, count, callback, &ctx);

	free(numbers);

	return 0;
}
