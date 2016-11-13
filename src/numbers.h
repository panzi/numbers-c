#ifndef NUMBERS_H
#define NUMBERS_H
#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "expr.h"

#ifdef __cplusplus
extern "C" {
#endif

void numbers_solutions(
	const size_t tasks, const Number target, const Number numbers[],
	const size_t count, void (*callback)(void*, const Expr*), void *arg);

#ifdef __cplusplus
}
#endif

#endif // NUMBERS_H
