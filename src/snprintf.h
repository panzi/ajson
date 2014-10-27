#ifndef SNPRINTF_H__
#define SNPRINTF_H__
#pragma once

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// MSVC does not support these C99 functions:
int snprintf(char* str, size_t size, const char* format, ...);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // SNPRINTF_H__
