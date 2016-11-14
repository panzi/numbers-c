#ifndef EXPR_H
#define EXPR_H
#pragma once

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OpE {
	// these numeric values equal to the corresponding operator precedence
	OpAdd = 0,
	OpSub = 1,
	OpMul = 3,
	OpDiv = 2,
	OpVal = 4
} Op;

typedef unsigned long Number;
typedef size_t NumberSet;

#define PRIN "%lu"

typedef struct ExprS {
	Op op;
	union {
		struct {
			const struct ExprS *left;
			const struct ExprS *right;
		} e;
		size_t index;
	} u;
	Number value;
	NumberSet used;
} Expr;

Expr *new_val(Number value, size_t index);
Expr *new_expr(Op op, const Expr *left, const Expr *right);

bool expr_equals(const Expr *left, const Expr *right);
void expr_fprint(FILE *stream, const Expr *expr);

bool is_normalized_add(const Expr *left, const Expr *right);
bool is_normalized_sub(const Expr *left, const Expr *right);
bool is_normalized_mul(const Expr *left, const Expr *right);
bool is_normalized_div(const Expr *left, const Expr *right);

#ifdef __cplusplus
}
#endif

#endif
