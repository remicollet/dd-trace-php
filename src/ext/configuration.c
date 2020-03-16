#include "configuration.h"

#include <stdlib.h>

#include "env_config.h"

struct ddtrace_memoized_configuration_t ddtrace_memoized_configuration = {
#define CHAR(...) NULL, FALSE,
#define BOOL(...) FALSE, FALSE,
#define INT(...) 0, FALSE,
#define DOUBLE(...) 0.0, FALSE,
    DD_CONFIGURATION
#undef CHAR
#undef BOOL
#undef INT
#undef DOUBLE
        PTHREAD_MUTEX_INITIALIZER};

void ddtrace_reload_config(TSRMLS_D) {
#define CHAR(getter_name, ...)                            \
    if (ddtrace_memoized_configuration.getter_name) {     \
        free(ddtrace_memoized_configuration.getter_name); \
    }                                                     \
    ddtrace_memoized_configuration.__is_set_##getter_name = FALSE;
#define BOOL(getter_name, ...) ddtrace_memoized_configuration.__is_set_##getter_name = FALSE;
#define INT(getter_name, ...) ddtrace_memoized_configuration.__is_set_##getter_name = FALSE;
#define DOUBLE(getter_name, ...) ddtrace_memoized_configuration.__is_set_##getter_name = FALSE;

    DD_CONFIGURATION

#undef CHAR
#undef BOOL
#undef INT
#undef DOUBLE
    // repopulate config
    ddtrace_initialize_config(TSRMLS_C);
}

void ddtrace_initialize_config(TSRMLS_D) {
    // read all values to memoize them

    // CHAR returns a copy of a value that we need to free
#define CHAR(getter_name, env_name, default, ...)                                  \
    do {                                                                           \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);                 \
        ddtrace_memoized_configuration.getter_name =                               \
            ddtrace_get_c_string_config_with_default(env_name, default TSRMLS_CC); \
        ddtrace_memoized_configuration.__is_set_##getter_name = TRUE;              \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);               \
    } while (0);
#define BOOL(getter_name, env_name, default, ...)                                                          \
    do {                                                                                                   \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);                                         \
        ddtrace_memoized_configuration.getter_name = ddtrace_get_bool_config(env_name, default TSRMLS_CC); \
        ddtrace_memoized_configuration.__is_set_##getter_name = TRUE;                                      \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);                                       \
    } while (0);
#define INT(getter_name, env_name, default, ...)                                                          \
    do {                                                                                                  \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);                                        \
        ddtrace_memoized_configuration.getter_name = ddtrace_get_int_config(env_name, default TSRMLS_CC); \
        ddtrace_memoized_configuration.__is_set_##getter_name = TRUE;                                     \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);                                      \
    } while (0);
#define DOUBLE(getter_name, env_name, default, ...)                                                          \
    do {                                                                                                     \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);                                           \
        ddtrace_memoized_configuration.getter_name = ddtrace_get_double_config(env_name, default TSRMLS_CC); \
        ddtrace_memoized_configuration.__is_set_##getter_name = TRUE;                                        \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);                                         \
    } while (0);

    DD_CONFIGURATION

#undef CHAR
#undef BOOL
#undef INT
#undef DOUBLE
}

void ddtrace_config_shutdown(void) {
    // read all values to memoize them

    // CHAR returns a copy of a value that we need to free
#define CHAR(getter_name, env_name, default, ...)                      \
    do {                                                               \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);     \
        if (ddtrace_memoized_configuration.getter_name) {              \
            free(ddtrace_memoized_configuration.getter_name);          \
            ddtrace_memoized_configuration.getter_name = NULL;         \
        }                                                              \
        ddtrace_memoized_configuration.__is_set_##getter_name = FALSE; \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);   \
    } while (0);
#define BOOL(getter_name, env_name, default, ...)                      \
    do {                                                               \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);     \
        ddtrace_memoized_configuration.__is_set_##getter_name = FALSE; \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);   \
    } while (0);
#define INT(getter_name, env_name, default, ...)                       \
    do {                                                               \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);     \
        ddtrace_memoized_configuration.__is_set_##getter_name = FALSE; \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);   \
    } while (0);
#define DOUBLE(getter_name, env_name, default, ...)                    \
    do {                                                               \
        pthread_mutex_lock(&ddtrace_memoized_configuration.mutex);     \
        ddtrace_memoized_configuration.__is_set_##getter_name = FALSE; \
        pthread_mutex_unlock(&ddtrace_memoized_configuration.mutex);   \
    } while (0);

    DD_CONFIGURATION

#undef CHAR
#undef BOOL
#undef INT
#undef DOUBLE
}

// define configuration getters macros
#define CHAR(getter_name, ...) extern inline char* getter_name(void);
#define BOOL(getter_name, ...) extern inline bool getter_name(void);
#define INT(getter_name, ...) extern inline int64_t getter_name(void);
#define DOUBLE(getter_name, ...) extern inline double getter_name(void);

DD_CONFIGURATION

#undef CHAR
#undef BOOL
#undef INT
#undef DOUBLE
