#ifndef EXPRBUF_H
#define EXPRBUF_H

#include "expr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXPRBUF_INIT_CAPACITY 64

typedef struct ExprBufS {
	Expr **buf;
	size_t size;
	size_t capacity;
} ExprBuf;

void exprbuf_init(ExprBuf *buf);
void exprbuf_add(ExprBuf *buf, Expr *expr);
bool exprbuf_contains(const ExprBuf *buf, const Expr *expr);
void exprbuf_free_buf(ExprBuf *buf);
void exprbuf_free_items(ExprBuf *buf);

#ifdef __cplusplus
}
#endif

#endif
