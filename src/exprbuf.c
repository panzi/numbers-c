#include "exprbuf.h"
#include "panic.h"

#include <stdlib.h>
#include <stdint.h>

void exprbuf_init(ExprBuf *buf) {
	buf->capacity = EXPRBUF_INIT_CAPACITY;
	buf->size = 0;
	buf->buf = calloc(buf->capacity, sizeof(Expr));

	if (!buf->buf) {
		panice("allocating expression buffer");
	}
}

void exprbuf_add(ExprBuf *buf, Expr *expr) {
	if (buf->size == buf->capacity) {
		if (SIZE_MAX / 2 < buf->capacity) {
			panicf("integer overflow");
		}
		const size_t capacity = buf->capacity == 0 ? EXPRBUF_INIT_CAPACITY : buf->capacity * 2;
		buf->buf = realloc(buf->buf, capacity * sizeof(Expr));
		buf->capacity = capacity;
		if (!buf->buf) {
			panice("resizing expression buffer");
		}
	}

	buf->buf[buf->size] = expr;
	buf->size ++;
}

bool exprbuf_contains(const ExprBuf *buf, const Expr *expr) {
	for (size_t index = 0; index < buf->size; ++ index) {
		const Expr *other = buf->buf[index];
		if (expr_equals(expr, other)) {
			return true;
		}
	}
	return false;
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
