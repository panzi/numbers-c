#include "numbers.h"
#include "exprbuf.h"
#include "panic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

typedef struct ManagerS {
	sem_t semaphore;
	ExprBuf exprs;
	ExprBuf *segments;
	NumberSet segment_count;
} Manager;

typedef struct WorkerS {
	pthread_t thread;
	volatile ExprBuf new_exprs;
	volatile size_t lower;
	volatile size_t upper;
	sem_t semaphore;
	Manager *manager;
} Worker;

static void make_exprs(ExprBuf *exprs, const Expr *a, const Expr *b);
static void *worker_proc(void *arg);

void make_exprs(ExprBuf *exprs, const Expr *a, const Expr *b) {
	const Number avalue = a->value;
	const Number bvalue = b->value;

	if (is_normalized_add(a, b)) {
		exprbuf_add(exprs, new_expr(OpAdd, a, b));
	}
	else if (is_normalized_add(b, a)) {
		exprbuf_add(exprs, new_expr(OpAdd, b, a));
	}

	if (avalue != 1 && bvalue != 1) {
		if (is_normalized_mul(a, b)) {
			exprbuf_add(exprs, new_expr(OpMul, a, b));
		}
		else if (is_normalized_mul(b, a)) {
			exprbuf_add(exprs, new_expr(OpMul, b, a));
		}
	}

	if (avalue > bvalue) {
		if (is_normalized_sub(a, b)) {
			exprbuf_add(exprs, new_expr(OpSub, a, b));
		}

		if (bvalue != 1 && (avalue % bvalue) == 0 && is_normalized_div(a, b)) {
			exprbuf_add(exprs, new_expr(OpDiv, a, b));
		}
	}
	else if (bvalue > avalue) {
		if (is_normalized_sub(b, a)) {
			exprbuf_add(exprs, new_expr(OpSub, b, a));
		}

		if (avalue != 1 && (bvalue % avalue) == 0 && is_normalized_div(b, a)) {
			exprbuf_add(exprs, new_expr(OpDiv, b, a));
		}
	}
	else if (avalue == bvalue && bvalue != 1) {
		if (is_normalized_div(a, b)) {
			exprbuf_add(exprs, new_expr(OpDiv, a, b));
		}
		else if (is_normalized_div(b, a)) {
			exprbuf_add(exprs, new_expr(OpDiv, b, a));
		}
	}
}

void numbers_solutions(
	const size_t tasks, const Number target, const Number numbers[],
	const size_t count, void (*callback)(void*, const Expr*), void *arg) {

	if (tasks == 0) {
		panicf("number of tasks has to be >= 1");
	}

	if (count > sizeof(NumberSet) * 8) {
		panicf("only up to %zu numbers supported", sizeof(NumberSet) * 8);
	}

	const NumberSet full_usage = ~(~0ul << count);
	ExprBuf uniq_solutions = EXPRBUF_INIT;
	Manager manager = {
		.exprs = EXPRBUF_INIT,
		.segments = calloc(full_usage, sizeof(ExprBuf)),
		.segment_count = full_usage
	};

	if (!manager.segments) {
		panice("allocating segments array");
	}

	if (sem_init(&manager.semaphore, 0, 0) != 0) {
		panice("initializing manager semaphore");
	}

	Worker *workers = calloc(tasks, sizeof(Worker));
	if (!workers) {
		panice("allocating workers array");
	}

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		worker->manager = &manager;

		if (sem_init(&worker->semaphore, 0, 0) != 0) {
			panice("initializing worker semaphore");
		}
	}

	for (size_t index = 0; index < count; ++ index) {
		const Number number = numbers[index];
		if (number == 0) {
			panicf("given numbers may not be 0");
		}
		Expr *expr = new_val(number, index);
		exprbuf_add(&manager.exprs, expr);
		exprbuf_add(&manager.segments[expr->used - 1], expr);
	}

	for (size_t index = 0; index < count; ++ index) {
		const Expr *expr = manager.exprs.buf[index];
		if (expr->value == target) {
			callback(arg, expr);
			break;
		}
	}

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		const int errnum = pthread_create(&worker->thread, NULL, &worker_proc, worker);
		if (errnum != 0) {
			panicf("starting worker therad: %s", strerror(errnum));
		}
	}

	size_t lower = 0;
	size_t upper = count;
#ifdef DEBUG
	size_t collisions = 0;
#endif

	while (lower < upper) {
		size_t worker_count = 0;
		// ceiling integer division
		const size_t task_size = 1 + ((upper - lower - 1) / tasks);
		size_t prev_upper = lower;

		while (prev_upper < upper) {
#ifdef DEBUG
			if (worker_count >= tasks) {
				panicf("BUG: calculating worker count: more workers than requested tasks");
			}
#endif
			const size_t task_upper = upper - task_size < prev_upper ?
				upper : prev_upper + task_size;
			Worker *worker = &workers[worker_count];
			worker->lower = prev_upper;
			worker->upper = task_upper;

			if (sem_post(&worker->semaphore) != 0) {
				panice("sending work to worker thread");
			}

			++ worker_count;
			prev_upper = task_upper;
		}

#ifdef DEBUG
		if (worker_count == 0) {
			panicf("BUG: no workers created");
		}
#endif

		for (size_t finished = 0; finished < worker_count; ++ finished) {
			if (sem_wait(&manager.semaphore) != 0) {
				panice("waiting for worker thread");
			}
		}

		for (size_t index = 0; index < worker_count; ++ index) {
			Worker *worker = &workers[index];
			for (size_t i = 0; i < worker->new_exprs.size; ++ i) {
				Expr *expr = worker->new_exprs.buf[i];
				if (expr->value == target) {
					if (!exprbuf_contains(&uniq_solutions, expr)) {
						exprbuf_add(&uniq_solutions, expr);
						callback(arg, expr);
					}
					else {
#ifdef DEBUG
						++ collisions;
#endif
						free(expr);
					}
				} else if (expr->used != full_usage) {
					exprbuf_add(&manager.exprs, expr);
					exprbuf_add(&manager.segments[expr->used - 1], expr);
				} else {
					free(expr);
				}
			}
			worker->new_exprs.size = 0;
		}

		lower = upper;
		upper = manager.exprs.size;
	}

#ifdef DEBUG
	printf("collisions: %zu\n", collisions);
#endif

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		worker->lower = 0;
		worker->upper = 0;
		if (sem_post(&worker->semaphore) != 0) {
			perror("signaling end to worker thread");
		}
	}

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		const int errnum = pthread_join(worker->thread, NULL);
		if (errnum != 0) {
			fprintf(stderr, "wating for worker thread to end: %s\n", strerror(errnum));
		}
		if (sem_destroy(&worker->semaphore) != 0) {
			perror("destroying worker semaphore");
		}
	}

	free(workers);

	for (NumberSet index = 0; index < manager.segment_count; ++ index) {
		exprbuf_free_buf(&manager.segments[index]);
	}

	free(manager.segments);

	exprbuf_free_items(&uniq_solutions);
	exprbuf_free_items(&manager.exprs);

	if (sem_destroy(&manager.semaphore) != 0) {
		perror("destroying manager semaphore");
	}
}

void *worker_proc(void *arg) {
	Worker *worker = (Worker*)arg;
	Manager *manager = worker->manager;
	const ExprBuf *segments = manager->segments;
	const NumberSet segment_count = manager->segment_count;

	for (;;) {
		if (sem_wait(&worker->semaphore) != 0) {
			panice("worker waiting for work");
		}

		const size_t lower = worker->lower;
		const size_t upper = worker->upper;

		ExprBuf *new_exprs = (ExprBuf*)&worker->new_exprs;

		if (lower == upper) {
			exprbuf_free_buf(new_exprs);
			break;
		}

		Expr *const *const exprs = manager->exprs.buf;

		for (size_t b = lower; b < upper; ++ b) {
			const Expr *bexpr = exprs[b];
			const NumberSet bused = bexpr->used;

			for (NumberSet aused = 1; aused <= segment_count; ++ aused) {
				if ((aused & bused) == 0) {
					const ExprBuf *segment = &segments[aused - 1];
					Expr *const *const buf = segment->buf;
					const size_t segment_size = segment->size;

					for (size_t index = 0; index < segment_size; ++ index) {
						const Expr *aexpr = buf[index];
						make_exprs(new_exprs, aexpr, bexpr);
					}
				}
			}
		}

		if (sem_post(&manager->semaphore) != 0) {
			panice("returning result to manager thread");
		}
	}

	pthread_exit(NULL);
	return NULL;
}
