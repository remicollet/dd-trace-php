#include <php.h>
#include <ext/spl/spl_exceptions.h>

#include "ddtrace.h"
#include "dispatch.h"

#include <Zend/zend.h>
#include "compat_zend_string.h"
#include "dispatch_compat.h"

#include <Zend/zend_closures.h>
#include <Zend/zend_exceptions.h>
#include "debug.h"

// avoid Older GCC being overly cautious over {0} struct initializer
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define BUSY_FLAG 1

#if PHP_VERSION_ID >= 70100
#define RETURN_VALUE_USED(opline) ((opline)->result_type != IS_UNUSED)
#else
#define RETURN_VALUE_USED(opline) (!((opline)->result_type & EXT_TYPE_UNUSED))
#endif

ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

#if PHP_VERSION_ID < 70000
#undef EX
#define EX(x) ((execute_data)->x)

#else  // PHP7.0+
// imported from PHP 7.2 as 7.0 missed this method
zend_class_entry *get_executed_scope(void) {
    zend_execute_data *ex = EG(current_execute_data);

    while (1) {
        if (!ex) {
            return NULL;
        } else if (ex->func && (ZEND_USER_CODE(ex->func->type) || ex->func->common.scope)) {
            return ex->func->common.scope;
        }
        ex = ex->prev_execute_data;
    }
}
#endif

#if PHP_VERSION_ID < 70000
static ddtrace_dispatch_t *lookup_dispatch(const HashTable *lookup, ddtrace_lookup_data_t *lookup_data) {
    if (lookup_data->function_name_length == 0) {
        lookup_data->function_name_length = strlen(lookup_data->function_name);
    }

    char *key = zend_str_tolower_dup(lookup_data->function_name, lookup_data->function_name_length);
    ddtrace_dispatch_t *dispatch = NULL;
    dispatch = zend_hash_str_find_ptr(lookup, key, lookup_data->function_name_length);

    efree(key);
    return dispatch;
}
#else
static ddtrace_dispatch_t *lookup_dispatch(const HashTable *lookup, ddtrace_lookup_data_t *lookup_data) {
    zend_string *to_free = NULL, *key = lookup_data->function_name;

    if (!ddtrace_is_all_lower(key)) {
        key = zend_string_tolower(key);
        to_free = key;
    }

    ddtrace_dispatch_t *dispatch = zend_hash_find_ptr(lookup, key);

    if (to_free) {
        zend_string_free(key);
    }
    return dispatch;
}
#endif

static ddtrace_dispatch_t *find_dispatch(const zend_class_entry *class, ddtrace_lookup_data_t *lookup_data TSRMLS_DC) {
    if (!lookup_data->function_name) {
        return NULL;
    }
    HashTable *class_lookup = NULL;

#if PHP_VERSION_ID < 70000
    const char *class_name = NULL;
    size_t class_name_length = 0;
    class_name = class->name;
    class_name_length = class->name_length;
    class_lookup = zend_hash_str_find_ptr(&DDTRACE_G(class_lookup), class_name, class_name_length);
#else
    class_lookup = zend_hash_find_ptr(&DDTRACE_G(class_lookup), class->name);
#endif

    ddtrace_dispatch_t *dispatch = NULL;
    if (class_lookup) {
        dispatch = lookup_dispatch(class_lookup, lookup_data);
    }

    if (dispatch) {
        return dispatch;
    }

    if (class->parent) {
        return find_dispatch(class->parent, lookup_data TSRMLS_CC);
    } else {
        return NULL;
    }
}

#if PHP_VERSION_ID < 70000
zend_function *fcall_fbc(zend_execute_data *execute_data TSRMLS_DC) {
    zend_op *opline = EX(opline);
    zend_function *fbc = NULL;
    zval *fname = opline->op1.zv;

    if (CACHED_PTR(opline->op1.literal->cache_slot)) {
        return CACHED_PTR(opline->op1.literal->cache_slot);
    } else if (EXPECTED(zend_hash_quick_find(EG(function_table), Z_STRVAL_P(fname), Z_STRLEN_P(fname) + 1,
                                             Z_HASH_P(fname), (void **)&fbc) == SUCCESS)) {
        return fbc;
    } else {
        return NULL;
    }
}
#endif

static void execute_fcall(ddtrace_dispatch_t *dispatch, zval *this, zend_execute_data *execute_data,
                          zval **return_value_ptr TSRMLS_DC) {
    zend_fcall_info fci = {0};
    zend_fcall_info_cache fcc = {0};
    char *error = NULL;
    zval closure;
    INIT_ZVAL(closure);
    zend_function *current_fbc = DDTRACE_G(original_context).fbc;
    zend_class_entry *executed_method_class = NULL;
    if (this) {
        executed_method_class = Z_OBJCE_P(this);
    }

    zend_function *func;

#if PHP_VERSION_ID < 70000
    const char *func_name = DDTRACE_CALLBACK_NAME;
    func = datadog_current_function(execute_data);

    zend_function *callable = (zend_function *)zend_get_closure_method_def(&dispatch->callable TSRMLS_CC);

    // convert passed callable to not be static as we're going to bind it to *this
    if (this) {
        callable->common.fn_flags &= ~ZEND_ACC_STATIC;
    }

    zend_create_closure(&closure, callable, executed_method_class, this TSRMLS_CC);
#else
    zend_string *func_name = zend_string_init(ZEND_STRL(DDTRACE_CALLBACK_NAME), 0);
    func = EX(func);
    zend_create_closure(&closure, (zend_function *)zend_get_closure_method_def(&dispatch->callable),
                        executed_method_class, executed_method_class, this TSRMLS_CC);
#endif
    if (zend_fcall_info_init(&closure, 0, &fci, &fcc, NULL, &error TSRMLS_CC) != SUCCESS) {
        if (DDTRACE_G(strict_mode)) {
            const char *scope_name, *function_name;
#if PHP_VERSION_ID < 70000
            scope_name = (func->common.scope) ? func->common.scope->name : NULL;
            function_name = func->common.function_name;
#else
            scope_name = (func->common.scope) ? ZSTR_VAL(func->common.scope->name) : NULL;
            function_name = ZSTR_VAL(func->common.function_name);
#endif
            if (scope_name) {
                zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC,
                                        "cannot set override for %s::%s - %s", scope_name, function_name, error);
            } else {
                zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0 TSRMLS_CC, "cannot set override for %s - %s",
                                        function_name, error);
            }
        }

        if (error) {
            efree(error);
        }
        goto _exit_cleanup;
    }

    ddtrace_setup_fcall(execute_data, &fci, return_value_ptr TSRMLS_CC);

    // Move this to closure zval before zend_fcall_info_init()
    fcc.function_handler->common.function_name = func_name;

#if PHP_VERSION_ID >= 70000
    zend_class_entry *orig_scope = fcc.function_handler->common.scope;
    fcc.function_handler->common.scope = DDTRACE_G(original_context).calling_fbc->common.scope;
    fcc.calling_scope = DDTRACE_G(original_context).calling_fbc->common.scope;
#endif

    zend_execute_data *prev_original_execute_data = DDTRACE_G(original_context).execute_data;
    DDTRACE_G(original_context).execute_data = execute_data;
#if PHP_VERSION_ID < 70000
    zval *prev_original_function_name = DDTRACE_G(original_context).function_name;
    DDTRACE_G(original_context).function_name = (*EG(opline_ptr))->op1.zv;
#endif

    zend_call_function(&fci, &fcc TSRMLS_CC);

#if PHP_VERSION_ID < 70000
    DDTRACE_G(original_context).function_name = prev_original_function_name;
#endif
    DDTRACE_G(original_context).execute_data = prev_original_execute_data;

#if PHP_VERSION_ID >= 70000
    fcc.function_handler->common.scope = orig_scope;
#endif

#if PHP_VERSION_ID < 70000
    if (fci.params) {
        efree(fci.params);
    }
#else
    zend_string_release(func_name);
    if (fci.params) {
        zend_fcall_info_args_clear(&fci, 0);
    }
#endif

_exit_cleanup:
    if (this) {
#if PHP_VERSION_ID < 70000
        Z_DELREF_P(this);
#else
        if (EX_CALL_INFO() & ZEND_CALL_RELEASE_THIS) {
            OBJ_RELEASE(Z_OBJ(execute_data->This));
        }
#endif
    }
    DDTRACE_G(original_context).fbc = current_fbc;
    Z_DELREF(closure);
}

static int is_anonymous_closure(zend_function *fbc, ddtrace_lookup_data_t *lookup) {
    if (!(fbc->common.fn_flags & ZEND_ACC_CLOSURE) || !lookup->function_name) {
        return 0;
    }
#if PHP_VERSION_ID < 70000
    if (lookup->function_name_length == 0) {
        lookup->function_name_length = strlen(lookup->function_name);
    }
    if ((lookup->function_name_length == (sizeof("{closure}") - 1)) &&
        strcmp(lookup->function_name, "{closure}") == 0) {
        return 1;
    } else {
        return 0;
    }
#else
    if ((ZSTR_LEN(lookup->function_name) == (sizeof("{closure}") - 1)) &&
        strcmp((ZSTR_VAL(lookup->function_name)), "{closure}") == 0) {
        return 1;
    } else {
        return 0;
    }
#endif
}

static zend_always_inline zend_bool wrap_and_run(zend_execute_data *execute_data,
                                                 ddtrace_lookup_data_t *lookup_data TSRMLS_DC) {
#if PHP_VERSION_ID < 50500
    zval *original_object = EX(object);
#endif

    zval *this = ddtrace_this(execute_data);
    DD_PRINTF("Loaded $this object ptr: %p", (void *)this);

    ddtrace_dispatch_t *dispatch = NULL;

    zend_class_entry *class = NULL;

    if (this) {
        class = Z_OBJCE_P(this);
    }

    if (!this && (DDTRACE_G(original_context).fbc->common.fn_flags & ZEND_ACC_STATIC) != 0) {
        class = DDTRACE_G(original_context).fbc->common.scope;
    }

    if (class) {
        dispatch = find_dispatch(class, lookup_data TSRMLS_CC);
    } else {
        dispatch = lookup_dispatch(&DDTRACE_G(function_lookup), lookup_data);
    }

    if (dispatch && !dispatch->busy) {
        ddtrace_class_lookup_acquire(dispatch);  // protecting against dispatch being freed during php code execution
        dispatch->busy = 1;                      // guard against recursion, catching only topmost execution

#if PHP_VERSION_ID < 50500
        if (EX(opline)->opcode == ZEND_DO_FCALL) {
            zend_op *opline = EX(opline);
            zend_ptr_stack_3_push(&EG(arg_types_stack), FBC(), EX(object), EX(called_scope));

            if (CACHED_PTR(opline->op1.literal->cache_slot)) {
                EX(function_state).function = CACHED_PTR(opline->op1.literal->cache_slot);
            } else {
                EX(function_state).function = fcall_fbc(execute_data TSRMLS_CC);
                CACHE_PTR(opline->op1.literal->cache_slot, EX(function_state).function);
            }

            EX(object) = NULL;
        }
        if (this) {
            EX(object) = original_object;
        }
#endif
        const zend_op *opline = EX(opline);

#if PHP_VERSION_ID < 50500
#define EX_T(offset) (*(temp_variable *)((char *)EX(Ts) + offset))
        zval rv;
        INIT_ZVAL(rv);

        zval **return_value = NULL;
        zval *rv_ptr = &rv;

        if (RETURN_VALUE_USED(opline)) {
            EX_T(opline->result.var).var.ptr = &EG(uninitialized_zval);
            EX_T(opline->result.var).var.ptr_ptr = NULL;

            return_value = NULL;
        } else {
            return_value = &rv_ptr;
        }

        DD_PRINTF("Starting handler for %s#%s", common_scope, lookup_data->function_name);

        if (RETURN_VALUE_USED(opline)) {
            temp_variable *ret = &EX_T(opline->result.var);

            if (EG(return_value_ptr_ptr) && *EG(return_value_ptr_ptr)) {
                ret->var.ptr = *EG(return_value_ptr_ptr);
                ret->var.ptr_ptr = EG(return_value_ptr_ptr);
            } else {
                ret->var.ptr = NULL;
                ret->var.ptr_ptr = &ret->var.ptr;
            }

            ret->var.fcall_returned_reference =
                (DDTRACE_G(original_context).fbc->common.fn_flags & ZEND_ACC_RETURN_REFERENCE) != 0;
            return_value = ret->var.ptr_ptr;
        }

        execute_fcall(dispatch, this, execute_data, return_value TSRMLS_CC);
        EG(return_value_ptr_ptr) = EX(original_return_value);

        if (!RETURN_VALUE_USED(opline) && return_value && *return_value) {
            zval_delref_p(*return_value);
            if (Z_REFCOUNT_PP(return_value) == 0) {
                efree(*return_value);
                *return_value = NULL;
            }
        }

#elif PHP_VERSION_ID < 70000
        zval *return_value = NULL;
        execute_fcall(dispatch, this, execute_data, &return_value TSRMLS_CC);

        if (return_value != NULL) {
            if (RETURN_VALUE_USED(opline)) {
                EX_TMP_VAR(execute_data, opline->result.var)->var.ptr = return_value;
            } else {
                zval_ptr_dtor(&return_value);
            }
        }

#else
        zval rv;
        INIT_ZVAL(rv);

        zval *return_value = (RETURN_VALUE_USED(opline) ? EX_VAR(EX(opline)->result.var) : &rv);
        execute_fcall(dispatch, this, EX(call), &return_value TSRMLS_CC);

        if (!RETURN_VALUE_USED(opline)) {
            zval_dtor(&rv);
        }
#endif

        dispatch->busy = 0;
        ddtrace_class_lookup_release(dispatch);
        return 1;
    } else {
        return 0;
    }
}

static zend_always_inline zend_function *get_current_fbc(zend_execute_data *execute_data TSRMLS_DC) {
    zend_function *fbc = NULL;

#if PHP_VERSION_ID < 70000
    if (EX(opline)->opcode == ZEND_DO_FCALL_BY_NAME) {
        fbc = FBC();
    } else {
        fbc = fcall_fbc(execute_data TSRMLS_CC);
#ifdef ZTS
        (void)TSRMLS_C;
#endif  // ZTS
    }
#else
    fbc = EX(call)->func;
#endif
    return fbc;
}

static zend_always_inline zend_bool is_function_wrappable(zend_execute_data *execute_data, zend_function *fbc,
                                                          ddtrace_lookup_data_t *lookup_data) {
    if (!fbc) {
        DD_PRINTF("No function obj found, skipping lookup");
        return 0;
    }

#if PHP_VERSION_ID < 70000
    if (EX(opline)->opcode == ZEND_DO_FCALL_BY_NAME) {
        if (fbc) {
            lookup_data->function_name = fbc->common.function_name;
        }
    } else {
        zval *fname = EX(opline)->op1.zv;

        lookup_data->function_name = Z_STRVAL_P(fname);
        lookup_data->function_name_length = Z_STRLEN_P(fname);
    }
#else
    fbc = EX(call)->func;
    if (fbc->common.function_name) {
        lookup_data->function_name = fbc->common.function_name;
    }
#endif
    if (!lookup_data->function_name) {
        DD_PRINTF("No function name, skipping lookup");
        return 0;
    }

    if (is_anonymous_closure(fbc, lookup_data)) {
        DD_PRINTF("Anonymous closure, skipping lookup");
        return 0;
    }

    return 1;
}

#define CTOR_CALL_BIT 0x1
#define CTOR_USED_BIT 0x2
#define DECODE_CTOR(ce) ((zend_class_entry *)(((zend_uintptr_t)(ce)) & ~(CTOR_CALL_BIT | CTOR_USED_BIT)))

static int update_opcode_leave(zend_execute_data *execute_data TSRMLS_DC) {
    DD_PRINTF("Update opcode leave");
#if PHP_VERSION_ID < 50500
    EX(function_state).function = (zend_function *)EX(op_array);
    EX(function_state).arguments = NULL;
    EG(opline_ptr) = &EX(opline);
    EG(active_op_array) = EX(op_array);

    EG(return_value_ptr_ptr) = EX(original_return_value);
    EX(original_return_value) = NULL;

    EG(active_symbol_table) = EX(symbol_table);

    EX(object) = EX(current_object);
    EX(called_scope) = DECODE_CTOR(EX(called_scope));

    zend_arg_types_stack_3_pop(&EG(arg_types_stack), &EX(called_scope), &EX(current_object), &EX(fbc));
    zend_vm_stack_clear_multiple(TSRMLS_C);
#elif PHP_VERSION_ID < 70000
    zend_vm_stack_clear_multiple(0 TSRMLS_CC);
    EX(call)--;
#else
    EX(call) = EX(call)->prev_execute_data;
#endif
    EX(opline) = EX(opline) + 1;

    return ZEND_USER_OPCODE_LEAVE;
}

int default_dispatch(zend_execute_data *execute_data TSRMLS_DC) {
    DD_PRINTF("calling default dispatch");
    if (EX(opline)->opcode == ZEND_DO_FCALL_BY_NAME) {
        if (DDTRACE_G(ddtrace_old_fcall_by_name_handler)) {
            return DDTRACE_G(ddtrace_old_fcall_by_name_handler)(execute_data TSRMLS_CC);
        }
    } else {
        if (DDTRACE_G(ddtrace_old_fcall_handler)) {
            return DDTRACE_G(ddtrace_old_fcall_handler)(execute_data TSRMLS_CC);
        }
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

int ddtrace_wrap_fcall(zend_execute_data *execute_data TSRMLS_DC) {
    DD_PRINTF("OPCODE: %s", zend_get_opcode_name(EX(opline)->opcode));
    if (DDTRACE_G(disable) || DDTRACE_G(disable_in_current_request)) {
        return default_dispatch(execute_data TSRMLS_CC);
    }

    zend_function *current_fbc = get_current_fbc(execute_data TSRMLS_CC);
    ddtrace_lookup_data_t lookup_data = {0};

    if (!is_function_wrappable(execute_data, current_fbc, &lookup_data)) {
        return default_dispatch(execute_data TSRMLS_CC);
    }
    zend_function *previous_fbc = DDTRACE_G(original_context).fbc;
    DDTRACE_G(original_context).fbc = current_fbc;
    zend_function *previous_calling_fbc = DDTRACE_G(original_context).calling_fbc;
#if PHP_VERSION_ID < 70000
    DDTRACE_G(original_context).calling_fbc =
        execute_data->function_state.function && execute_data->function_state.function->common.scope
            ? execute_data->function_state.function
            : current_fbc;
#else
    DDTRACE_G(original_context).calling_fbc = current_fbc->common.scope ? current_fbc : execute_data->func;
#endif
    zval *this = ddtrace_this(execute_data);
#if PHP_VERSION_ID < 70000
    zval *previous_this = DDTRACE_G(original_context).this;
    DDTRACE_G(original_context).this = this;
#else
    zend_object *previous_this = DDTRACE_G(original_context).this;
    DDTRACE_G(original_context).this = this ? Z_OBJ_P(this) : NULL;
#endif
    zend_class_entry *previous_calling_ce = DDTRACE_G(original_context).calling_ce;
#if PHP_VERSION_ID < 70000
    DDTRACE_G(original_context).calling_ce = DDTRACE_G(original_context).calling_fbc->common.scope;
#else
    DDTRACE_G(original_context).calling_ce = Z_OBJ(execute_data->This) ? Z_OBJ(execute_data->This)->ce : NULL;
#endif

    zend_bool wrapped = wrap_and_run(execute_data, &lookup_data TSRMLS_CC);

    DDTRACE_G(original_context).calling_ce = previous_calling_ce;
    DDTRACE_G(original_context).this = previous_this;
    DDTRACE_G(original_context).calling_fbc = previous_calling_fbc;
    DDTRACE_G(original_context).fbc = previous_fbc;
    if (wrapped) {
        return update_opcode_leave(execute_data TSRMLS_CC);
    } else {
        return default_dispatch(execute_data TSRMLS_CC);
    }
}

void ddtrace_class_lookup_acquire(ddtrace_dispatch_t *dispatch) { dispatch->acquired++; }

void ddtrace_class_lookup_release(ddtrace_dispatch_t *dispatch) {
    if (dispatch->acquired > 0) {
        dispatch->acquired--;
    }

    // free when no one has acquired this resource
    if (dispatch->acquired == 0) {
        ddtrace_dispatch_free_owned_data(dispatch);
        efree(dispatch);
    }
}
int find_method(zend_class_entry *ce, zval *name, zend_function **function) {
    return ddtrace_find_function(&ce->function_table, name, function);
}

zend_class_entry *ddtrace_target_class_entry(zval *class_name, zval *method_name TSRMLS_DC) {
    zend_class_entry *class = NULL;
#if PHP_VERSION_ID < 70000
    class = zend_fetch_class(Z_STRVAL_P(class_name), Z_STRLEN_P(class_name),
                             ZEND_FETCH_CLASS_DEFAULT | ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
#else
    class = zend_fetch_class_by_name(Z_STR_P(class_name), NULL, ZEND_FETCH_CLASS_DEFAULT | ZEND_FETCH_CLASS_SILENT);
#endif
    zend_function *method = NULL;

    if (class && find_method(class, method_name, &method) == SUCCESS) {
        if (method->common.scope != class) {
            class = method->common.scope;
            DD_PRINTF("Overriding Parent class method");
        }
    }

    return class;
}

int ddtrace_find_function(HashTable *table, zval *name, zend_function **function) {
    zend_function *ptr = ddtrace_function_get(table, name);
    if (!ptr) {
        return FAILURE;
    }

    if (function) {
        *function = ptr;
    }

    return SUCCESS;
}
