#ifndef PANIC_H
#define PANIC_H
#pragma once

#include <stdlib.h>
#include <stdio.h>

#define panicf(FMT, ...) \
	fprintf(stderr, "%s:%u:%s: ", __FILE__, __LINE__, __func__); \
	fprintf(stderr, FMT, ## __VA_ARGS__); \
	fputc('\n', stderr); \
	exit(1);

#define panice(MSG) \
	fprintf(stderr, "%s:%u:%s: ", __FILE__, __LINE__, __func__); \
	perror(MSG); \
	exit(1);

#endif
