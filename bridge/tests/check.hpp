// BrassCue - GPL-3.0. Always-active test checks.
#pragma once
#include <cstdlib>
#include <iostream>
#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__)) {                                                                      \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ << "\n";              \
            std::exit(EXIT_FAILURE);                                                               \
        }                                                                                          \
    } while (false)
