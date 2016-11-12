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
	size_t   segment_count;
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
	if (is_normalized_add(a, b)) {
		exprbuf_add(exprs, new_expr(OpAdd, a, b));
	}
	else if (is_normalized_add(b, a)) {
		exprbuf_add(exprs, new_expr(OpAdd, b, a));
	}

	if (a->value != 1 && b->value != 1) {
		if (is_normalized_mul(a, b)) {
			exprbuf_add(exprs, new_expr(OpMul, a, b));
		}
		else if (is_normalized_mul(b, a)) {
			exprbuf_add(exprs, new_expr(OpMul, b, a));
		}
	}

	if (a->value > b->value) {
		if (is_normalized_sub(a, b)) {
			exprbuf_add(exprs, new_expr(OpSub, a, b));
		}

		if (b->value != 1 && (a->value % b->value) == 0 && is_normalized_div(a, b)) {
			exprbuf_add(exprs, new_expr(OpDiv, a, b));
		}
	}
	else if (b->value > a->value) {
		if (is_normalized_sub(b, a)) {
			exprbuf_add(exprs, new_expr(OpSub, b, a));
		}

		if (a->value != 1 && (b->value % a->value) == 0 && is_normalized_div(b, a)) {
			exprbuf_add(exprs, new_expr(OpDiv, b, a));
		}
	}
	else if (b->value != 1) {
		if (is_normalized_div(a, b)) {
			exprbuf_add(exprs, new_expr(OpDiv, a, b));
		}
		else if (is_normalized_div(b, a)) {
			exprbuf_add(exprs, new_expr(OpDiv, b, a));
		}
	}
}

void numbers_solutions(
	const size_t tasks, const unsigned long target, const unsigned long numbers[],
	const size_t count, void (*callback)(void*, const Expr*), void *arg) {

	if (tasks == 0) {
		panicf("number of tasks has to be >= 1");
	}

	if (count > sizeof(size_t) * 8) {
		panicf("only up to %lu numbers supported", sizeof(size_t) * 8);
	}

	Manager manager;
	ExprBuf uniq_solutions;

	if (sem_init(&manager.semaphore, 0, 0) != 0) {
		panice("initializing manager semaphore");
	}

	const size_t full_usage = ~(~0ul << count);

	exprbuf_init(&manager.exprs);
	exprbuf_init(&uniq_solutions);

	manager.segment_count = full_usage;
	manager.segments = calloc(manager.segment_count, sizeof(ExprBuf));

	if (!manager.segments) {
		panice("allocating segments array");
	}

	Worker *workers = calloc(tasks, sizeof(Worker));
	if (!workers) {
		panice("allocating workers array");
	}

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		exprbuf_init((ExprBuf*)&worker->new_exprs);
		worker->manager = &manager;

		if (sem_init(&worker->semaphore, 0, 0) != 0) {
			panice("initializing worker semaphore");
		}
	}

	for (size_t index = 0; index < count; ++ index) {
		Expr *expr = new_val(numbers[index], index);
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

	while (lower < upper) {
		size_t worker_count = 0;
		const size_t x0 = lower;
		const size_t xn = upper;
		size_t x_last = x0;
		size_t i = 1;
		const size_t x0_sq = x0 * x0;
		const double area = (double)(xn * xn - x0_sq) / (double)tasks;

		while (x_last < xn || worker_count == 0) {
			const size_t xi_ = (size_t)round(sqrt((double)i * area + (double)x0_sq));
			const size_t xi = xi_ > xn ? xn : xi_;

			if (xi > x_last) {
				if (worker_count >= tasks) {
					panicf("calculating worker count: more workers than requested tasks");
				}
				const size_t xim1 = x_last;

				Worker *worker = &workers[worker_count];
				worker->lower = xim1;
				worker->upper = xi;

				if (sem_post(&worker->semaphore) != 0) {
					panice("sending work to worker thread");
				}

				x_last = xi;
				++ worker_count;
			}

			++ i;
		}

#ifdef DEBUG
		if (worker_count == 0) {
			panicf("BUG: no worker created");
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
		exprbuf_free_buf((ExprBuf*)&worker->new_exprs);
		if (sem_destroy(&worker->semaphore) != 0) {
			perror("destroying worker semaphore");
		}
	}

	free(workers);

	for (size_t index = 0; index < manager.segment_count; ++ index) {
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

	for (;;) {
		if (sem_wait(&worker->semaphore) != 0) {
			panice("worker waiting for work");
		}

		const size_t lower = worker->lower;
		const size_t upper = worker->upper;

		if (lower == upper) {
			break;
		}

		Expr *const *const exprs = worker->manager->exprs.buf;
		const ExprBuf *segments = worker->manager->segments;
		const size_t segment_count = worker->manager->segment_count;
		ExprBuf *new_exprs = (ExprBuf*)&worker->new_exprs;

		for (size_t b = lower; b < upper; ++ b) {
			const Expr *bexpr = exprs[b];
			const size_t bused = bexpr->used;

			for (size_t aused = 1; aused <= segment_count; ++ aused) {
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

		if (sem_post(&worker->manager->semaphore) != 0) {
			panice("returning result to manager thread");
		}
	}

	pthread_exit(NULL);
	return NULL;
}
