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

static Number parse_number(const char *str, const char *errmsg);
static void callback(void *arg, const Expr *expr);
static int compare_number(const void *lptr, const void *rptr);

#ifdef _SC_NPROCESSORS_ONLN
static size_t get_cpu_count();
#endif

Number parse_number(const char *str, const char *errmsg) {
	char *endptr = NULL;
	Number size = strtoul(str, &endptr, 10);
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

int compare_number(const void *lptr, const void *rptr) {
	Number l = *(Number*)lptr;
	Number r = *(Number*)rptr;
	return l < r ? -1 : r < l ? 1 : 0;
}

#ifdef _SC_NPROCESSORS_ONLN
#define HAS_GET_CPU_COUNT
size_t get_cpu_count() {
	const long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs < 0) {
		panice("getting number of CPUs/cores");
	}
	return (size_t)nprocs;
}
#endif

int main(int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(stderr, "not enough arguments\n");
		return 1;
	}

#ifdef HAS_GET_CPU_COUNT
	const size_t tasks = strcmp(argv[1], "-") == 0 ? get_cpu_count() :
#else
	const size_t tasks =
#endif
		parse_number(argv[1], "number of tasks is not a number or out of range");

	const Number target = parse_number(argv[2], "target is not a number or out of range");
	const size_t count = (size_t)argc - 3;

	if (tasks == 0) {
		panicf("number of tasks has to be >= 1");
	}

	if (count > sizeof(NumberSet) * 8) {
		panicf("only up to %zu numbers supported", sizeof(NumberSet) * 8);
	}

	Number *numbers = calloc(count, sizeof(Number));

	if (!numbers) {
		panice("allocating numbers array");
	}

	for (size_t index = 0; index < count; ++ index) {
		numbers[index] = parse_number(argv[index + 3], "not a number or out of range");
	}

	qsort(numbers, count, sizeof(Number), compare_number);

	printf("tasks = %zu\n", tasks);
	printf("target = " PRIN "\n", target);
	if (count == 0) {
		printf("numbers = [");
	}
	else {
		printf("numbers = [" PRIN, numbers[0]);
		for (size_t index = 1; index < count; ++ index) {
			printf(", " PRIN, numbers[index]);
		}
	}
	printf("]\n\nsolutions:\n");

	Context ctx = { .count = 1 };
	numbers_solutions(tasks, target, numbers, count, callback, &ctx);
	if (ctx.count == 1) {
		puts("no solutions found");
	}

	free(numbers);

	return 0;
}
