#include "panic.h"
#include "expr.h"

#include <stdlib.h>
#include <stdint.h>

static void expr_fprint_op(FILE *stream, char op, const Expr *expr);

Expr *new_val(Number value, size_t index, size_t generation) {
	Expr *expr = malloc(sizeof(Expr));

	if (!expr) {
		panice("allocating new value");
	}

	expr->op = OpVal;
	expr->u.index = index;
	expr->value = value;
	expr->used = 1 << index;
	expr->generation = generation;

	return expr;
}

Expr *new_expr(Op op, const Expr *left, const Expr *right, size_t generation) {
	Expr *expr = malloc(sizeof(Expr));

	if (!expr) {
		panice("allocating new expression");
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
		panicf("illegal operation");
	}

	expr->used = left->used | right->used;
	expr->generation = generation;

	return expr;
}

bool expr_equals(const Expr *left, const Expr *right) {
	if (left == right) {
		return true;
	}

	if (left->op != right->op || left->value != right->value) {
		return false;
	}

	if (left->op != OpVal) {
		if (!expr_equals(left->u.e.left, right->u.e.left) || !expr_equals(left->u.e.right, right->u.e.right)) {
			return false;
		}
	}

	return true;
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

#ifdef DEBUG
void expr_fprint(FILE *stream, const Expr *expr) {
	fprintf(stream, "(0x%zx)(", (uintptr_t)expr);
	switch (expr->op) {
		case OpAdd: expr_fprint_op(stream, '+', expr); break;
		case OpSub: expr_fprint_op(stream, '-', expr); break;
		case OpMul: expr_fprint_op(stream, '*', expr); break;
		case OpDiv: expr_fprint_op(stream, '/', expr); break;
		case OpVal: fprintf(stream, PRIN " #%zu", expr->value, expr->u.index); break;
	}
	fprintf(stream, ")");
}
#else
void expr_fprint(FILE *stream, const Expr *expr) {
	switch (expr->op) {
		case OpAdd: expr_fprint_op(stream, '+', expr); break;
		case OpSub: expr_fprint_op(stream, '-', expr); break;
		case OpMul: expr_fprint_op(stream, '*', expr); break;
		case OpDiv: expr_fprint_op(stream, '/', expr); break;
		case OpVal: fprintf(stream, PRIN, expr->value); break;
	}
}
#endif
