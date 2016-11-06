#include "numbers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

typedef struct Manager {
	sem_t semaphore;
	ExprBuf exprs;
} Manager;

typedef struct Worker {
	pthread_t thread;
	volatile ExprBuf new_exprs;
	volatile size_t lower;
	volatile size_t upper;
	sem_t semaphore;
	Manager *manager;
} Worker;

static Expr *new_val(unsigned long value, size_t index);
static Expr *new_expr(Op op, const Expr *left, const Expr *right);

static void make_exprs(ExprBuf *exprs, const Expr *a, const Expr *b);

static bool is_normalized_add(const Expr *left, const Expr *right);
static bool is_normalized_sub(const Expr *left, const Expr *right);
static bool is_normalized_mul(const Expr *left, const Expr *right);
static bool is_normalized_div(const Expr *left, const Expr *right);

static void expr_fprint_op(FILE *stream, char op, const Expr *expr);

static void *worker_proc(void *arg);

Expr *new_val(unsigned long value, size_t index) {
	Expr *expr = malloc(sizeof(Expr));

	if (!expr) {
		perror("allocating new value");
		abort();
	}

	expr->op = OpVal;
	expr->u.index = index;
	expr->value = value;
	expr->used = 1 << index;

	return expr;
}

Expr *new_expr(Op op, const Expr *left, const Expr *right) {
	Expr *expr = malloc(sizeof(Expr));

	if (!expr) {
		perror("allocating new expression");
		abort();
	}

	expr->op = op;
	expr->u.e.left  = left;
	expr->u.e.right = right;

	switch (op) {
	case OpAdd:
		expr->value = left->value + right->value;
		break;

	case OpSub:
		expr->value = left->value - right->value;
		break;

	case OpMul:
		expr->value = left->value * right->value;
		break;

	case OpDiv:
		expr->value = left->value / right->value;
		break;

	default:
		fprintf(stderr, "illegal operation\n");
		abort();
	}

	expr->used = left->used | right->used;

	return expr;
}

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

void exprbuf_init(ExprBuf *buf) {
	buf->capacity = 32;
	buf->size = 0;
	buf->buf = calloc(buf->capacity, sizeof(Expr));

	if (!buf->buf) {
		perror("allocating expression buffer");
		abort();
	}
}

void exprbuf_swap(ExprBuf *left, ExprBuf *right) {
	Expr **buf = left->buf;
	const size_t capacity = left->capacity;
	const size_t size = left->size;

	left->buf      = right->buf;
	left->capacity = right->capacity;
	left->size     = right->size;

	right->buf      = buf;
	right->capacity = capacity;
	right->size     = size;
}

void exprbuf_add(ExprBuf *buf, Expr *expr) {
	if (buf->capacity == 0) {
		exprbuf_init(buf);
	}
	else if (buf->size == buf->capacity) {
		if (SIZE_MAX / 2 < buf->capacity) {
			fprintf(stderr, "integer overflow\n");
			abort();
		}
		const size_t capacity = buf->capacity * 2;
		buf->buf = realloc(buf->buf, capacity * sizeof(Expr));
		buf->capacity = capacity;
		if (!buf->buf) {
			perror("resizing expression buffer");
			abort();
		}
	}

	buf->buf[buf->size] = expr;
	buf->size ++;
}

void exprbuf_free_buf(ExprBuf *buf) {
	free(buf->buf);
	buf->buf      = NULL;
	buf->size     = 0;
	buf->capacity = 0;
}

void exprbuf_free_items(ExprBuf *buf) {
	for (size_t index = 0; index < buf->size; ++ index) {
		free(buf->buf[index]);
	}
	exprbuf_free_buf(buf);
}

bool is_normalized_add(const Expr *left, const Expr *right) {
	switch (right->op) {
		case OpAdd:
		case OpSub:
			return false;

		default:
			switch (left->op) {
				case OpAdd: return left->u.e.right->value <= right->value;
				case OpSub: return false;
				default:    return left->value <= right->value;
			}
	}
}

bool is_normalized_sub(const Expr *left, const Expr *right) {
	switch (right->op) {
		case OpAdd:
		case OpSub:
			return false;

		default:
			switch (left->op) {
				case OpSub: return left->u.e.right->value <= right->value;
				default:    return true;
			}
	}
}

bool is_normalized_mul(const Expr *left, const Expr *right) {
	switch (right->op) {
		case OpMul:
		case OpDiv:
			return false;

		default:
			switch (left->op) {
				case OpMul: return left->u.e.right->value <= right->value;
				case OpDiv: return false;
				default:    return left->value <= right->value;
			}
	}
}

bool is_normalized_div(const Expr *left, const Expr *right) {
	switch (right->op) {
		case OpMul:
		case OpDiv:
			return false;

		default:
			switch (left->op) {
				case OpDiv: return left->u.e.right->value <= right->value;
				default:    return true;
			}
	}
}

void expr_fprint_op(FILE *stream, char op, const Expr *expr) {
	// op equals to it's precedence
	const int p = expr->op;
	const int lp = expr->u.e.left->op;
	const int rp = expr->u.e.right->op;

	if (p > lp) {
		if (p > rp) {
			fputc('(', stream);
			expr_fprint(stream, expr->u.e.left);
			fputc(')', stream);

			printf(" %c ", op);

			fputc('(', stream);
			expr_fprint(stream, expr->u.e.right);
			fputc(')', stream);
		}
		else {
			fputc('(', stream);
			expr_fprint(stream, expr->u.e.left);
			fputc(')', stream);

			printf(" %c ", op);

			expr_fprint(stream, expr->u.e.right);
		}
	}
	else {
		if (p > rp) {
			expr_fprint(stream, expr->u.e.left);

			printf(" %c ", op);

			fputc('(', stream);
			expr_fprint(stream, expr->u.e.right);
			fputc(')', stream);
		}
		else {
			expr_fprint(stream, expr->u.e.left);

			printf(" %c ", op);

			expr_fprint(stream, expr->u.e.right);
		}
	}
}

void expr_fprint(FILE *stream, const Expr *expr) {
	switch (expr->op) {
		case OpAdd: expr_fprint_op(stream, '+', expr); break;
		case OpSub: expr_fprint_op(stream, '-', expr); break;
		case OpMul: expr_fprint_op(stream, '*', expr); break;
		case OpDiv: expr_fprint_op(stream, '/', expr); break;
		case OpVal: fprintf(stream, "%lu", expr->value); break;
	}
}

void numbers_solutions(
	const size_t tasks, const unsigned long target, const unsigned long numbers[],
	const size_t count, void (*callback)(void*, const Expr*), void *arg) {

	if (tasks == 0) {
		fprintf(stderr, "number of tasks has to be >= 1\n");
		abort();
	}

	if (count > sizeof(size_t) * 8) {
		fprintf(stderr, "only up to %lu numbers supported\n", sizeof(size_t) * 8);
		abort();
	}

	Manager manager;

	if (sem_init(&manager.semaphore, 0, 1) != 0) {
		perror("initializing manager semaphore");
		abort();
	}

	const unsigned long full_usage = ~(~0ul << count);

	exprbuf_init(&manager.exprs);

	Worker *workers = calloc(tasks, sizeof(Worker));
	if (!workers) {
		perror("allocating workers array");
		abort();
	}

	for (size_t index = 0; index < tasks; ++ index) {
		Worker *worker = &workers[index];
		exprbuf_init((ExprBuf*)&worker->new_exprs);
		worker->manager = &manager;

		if (sem_init(&worker->semaphore, 0, 1) != 0) {
			perror("initializing worker semaphore");
			abort();
		}
	}

	for (size_t index = 0; index < count; ++ index) {
		exprbuf_add(&manager.exprs, new_val(numbers[index], index));
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
			fprintf(stderr, "starting worker therad: %s\n", strerror(errnum));
			abort();
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
		const size_t area = (double)(xn * xn - x0_sq) / (double)tasks;

		while (x_last < xn || worker_count == 0) {
			const size_t xi_ = (size_t)round(sqrt((double)i * area + (double)x0_sq));
			const size_t xi = xi_ > xn ? xn : xi_;

			if (xi > x_last) {
				if (worker_count >= tasks) {
					fprintf(stderr, "calculating worker count: more workers than requested tasks\n");
					abort();
				}
				const size_t xim1 = x_last;

				Worker *worker = &workers[worker_count];
				worker->lower = xim1;
				worker->upper = xi;

				if (sem_post(&worker->semaphore) != 0) {
					perror("sending work to worker thread");
					abort();
				}

				x_last = xi;
				++ worker_count;
			}

			++ i;
		}

		for (size_t finished = 0; finished < worker_count; ++ finished) {
			if (sem_wait(&manager.semaphore) != 0) {
				perror("waiting for worker thread");
				abort();
			}
		}

		for (size_t index = 0; index < worker_count; ++ index) {
			Worker *worker = &workers[index];
			for (size_t i = 0; i < worker->new_exprs.size; ++ i) {
				Expr *expr = worker->new_exprs.buf[i];
				if (expr->value == target) {
					callback(arg, expr);
					free(expr);
				} else if (expr->used != full_usage) {
					exprbuf_add(&manager.exprs, expr);
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

	exprbuf_free_items(&manager.exprs);
	sem_destroy(&manager.semaphore);
}

void *worker_proc(void *arg) {
	Worker *worker = (Worker*)arg;

	for (;;) {
		if (sem_wait(&worker->semaphore) != 0) {
			perror("worker waiting for work");
			abort();
		}

		size_t lower = worker->lower;
		size_t upper = worker->upper;

		if (lower == upper) {
			break;
		}

		Expr **exprs = worker->manager->exprs.buf;
		ExprBuf *new_exprs = (ExprBuf*)&worker->new_exprs;

		for (size_t b = lower; b < upper; ++ b) {
			const Expr *bexpr = exprs[b];

			for (size_t a = 0; a < b; ++ a) {
				const Expr *aexpr = exprs[a];

				if ((aexpr->used & bexpr->used) == 0) {
					make_exprs(new_exprs, aexpr, bexpr);
				}
			}
		}

		if (sem_post(&worker->manager->semaphore) != 0) {
			perror("returning result to manager thread");
			abort();
		}
	}

	pthread_exit(NULL);
	return NULL;
}
