#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <SAPI.h>
#include <Zend/zend.h>
#include <Zend/zend_exceptions.h>
#include <php.h>
#include <php_ini.h>
#include <php_main.h>
#include <ext/spl/spl_exceptions.h>
#include <ext/standard/info.h>

#include "compatibility.h"
#include "ddtrace.h"
#include "debug.h"
#include "env_config.h"
#include "memory_limit.h"
#include "serializer.h"

ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

zend_long get_memory_limit(TSRMLS_D) {
    char *raw_memory_limit = ddtrace_get_c_string_config("DD_TRACE_MEMORY_LIMIT");
    size_t len = 0;
    zend_long limit = -1;

    if (raw_memory_limit) {
        len = strlen(raw_memory_limit);
    }
    if (len == 0) {
        if (PG(memory_limit) > 0) {
            limit = PG(memory_limit) * ALLOWED_MAX_MEMORY_USE_IN_PERCENT_OF_MEMORY_LIMIT;
        } else {
            limit = -1;
        }
    } else {
        limit = zend_atol(raw_memory_limit, len);
        if (raw_memory_limit[len - 1] == '%') {
            if (PG(memory_limit) > 0) {
                limit = PG(memory_limit) * ((double)limit / 100.0);
            } else {
                limit = -1;
            }
        }
    }

    if (raw_memory_limit) {
        efree(raw_memory_limit);
    }

    return limit;
}
