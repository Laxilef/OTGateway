#pragma once

// Preserve C6 size optimizations through SDK metadata and shell parsing.
#define _GLIBCXX20_CONSTEXPR __attribute__((cold)) constexpr
#define _GLIBCXX23_CONSTEXPR __attribute__((cold)) constexpr
