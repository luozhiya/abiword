

// a bunch of macro for compiler control.
// TODO FIXME, currently tied to gcc and clang.

#pragma once

#if defined(__clang__)
# define ABI_W_PUSH \
_Pragma("clang diagnostic push")
#elif defined(__GNUC__)
# define ABI_W_PUSH \
_Pragma("GCC diagnostic push")
#else
# define ABI_W_PUSH
#endif

#if defined(_MSC_VER)
#define ABI_W_NO_CONST_QUAL
#define ABI_W_NO_DEPRECATED
#define ABI_W_NO_SUGGEST_OVERRIDE
#else
#define ABI_W_NO_CONST_QUAL \
ABI_W_PUSH \
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")

#define ABI_W_NO_DEPRECATED \
ABI_W_PUSH \
_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define ABI_W_NO_SUGGEST_OVERRIDE \
ABI_W_PUSH \
_Pragma("GCC diagnostic ignored \"-Wsuggest-override\"")
#endif

#if defined(__clang__)
# define ABI_W_POP \
_Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
# define ABI_W_POP \
_Pragma("GCC diagnostic pop")
#else
# define ABI_W_POP
#endif
