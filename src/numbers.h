#ifndef NUMBERS_H
#define NUMBERS_H

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OpE {
	OpAdd = 0,
	OpSub = 1,
	OpMul = 3,
	OpDiv = 2,
	OpVal = 4
} Op;

typedef struct ExprS {
	Op op;
	union {
		struct {
			const struct ExprS *left;
			const struct ExprS *right;
		} e;
		size_t index;
	} u;
	unsigned long value;
	size_t used;
} Expr;

typedef struct ExprBufS {
	Expr **buf;
	size_t size;
	size_t capacity;
} ExprBuf;

void expr_fprint(FILE *stream, const Expr *expr);

void exprbuf_init(ExprBuf *buf);
void exprbuf_add(ExprBuf *buf, Expr *expr);
void exprbuf_free_buf(ExprBuf *buf);
void exprbuf_free_elems(ExprBuf *buf);
bool exprbuf_contains(const ExprBuf *buf, const Expr *expr);

void numbers_solutions(
	const size_t tasks, const unsigned long target, const unsigned long numbers[],
	const size_t count, void (*callback)(void*, const Expr*), void *arg);

#ifdef __cplusplus
}
#endif

#endif // NUMBERS_H
