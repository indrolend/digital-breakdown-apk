#pragma once

#include <cstdarg>
#include <cstdio>

#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6

inline int __android_log_print(int priority, const char* tag, const char* format, ...) {
    (void)priority;
    std::fprintf(stderr, "[%s] ", tag ? tag : "DBNATIVE");
    va_list args;
    va_start(args, format);
    const int result = std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    return result;
}
