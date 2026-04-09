#pragma once

#include <print>

#define LOG_ERROR(fmt, ...) std::println(stderr, "\033[31m" fmt "\033[0m" __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(fmt, ...)  std::println(stdout, fmt __VA_OPT__(, ) __VA_ARGS__)
