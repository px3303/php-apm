/* hcapm extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_hcapm.h"


// 版本兼容宏
#if PHP_VERSION_ID < 70000
#define ZSTR_VAL(zstr)  (zstr)->val
#define ZSTR_LEN(zstr)  (zstr)->len
#endif

// 拼接字符串
static char *hp_concat_char(const char *s1, size_t len1, const char *s2, size_t len2, const char *seperator, size_t sep_len)
{
    char *result = emalloc(len1+len2+sep_len+1);

    strcpy(result, s1);
    strcat(result, seperator);
    strcat(result, s2);
    result[len1+len2+sep_len] = '\0';

    return result;
}

// 获取函数名
static char *hp_get_function_name(zend_execute_data *data TSRMLS_DC)
{
    const char        *cls = NULL;
    char              *ret = NULL;
    zend_function      *curr_func;

    if (!data) {
        return NULL;
    }

#if PHP_VERSION_ID < 70000
    const char        *func = NULL;
    curr_func = data->function_state.function;
    func = curr_func->common.function_name;

    if (!func) {
        return NULL;
    }

    if (curr_func->common.scope) {
        cls = curr_func->common.scope->name;
    } else if (data->object) {
        cls = Z_OBJCE(*data->object)->name;
    }

    if (cls) {
        char* sep = "::";
        ret = hp_concat_char(cls, strlen(cls), func, strlen(func), sep, 2);
    } else {
        ret = estrdup(func);
    }
#else
    zend_string *func = NULL;
    curr_func = data->func;
    func = curr_func->common.function_name;

    if (!func) {
        return NULL;
    } else if (curr_func->common.scope != NULL) {
        char* sep = "::";
        cls = curr_func->common.scope->name->val;
        ret = hp_concat_char(cls, curr_func->common.scope->name->len, func->val, func->len, sep, 2);
    } else {
        ret = emalloc(ZSTR_LEN(func)+1);
        strcpy(ret, ZSTR_VAL(func));
        ret[ZSTR_LEN(func)] = '\0';
    }
#endif

    return ret;
}

// 获取系统时间
static uint64_t cycle_timer()
{
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC, &s);

    return s.tv_sec * 1000000 + s.tv_nsec / 1000;
}

// 追踪数据添加到全局存储对象
#define RECORD(call_id, wt, entry_id, func)                                         \
    do {                                                                            \
        int is_record = 0;                                                          \
        cJSON *record =  cJSON_CreateObject();                                      \
        if (cJSON_AddNumberToObject(record, "id", call_id) == NULL) {               \
            goto end;                                                               \
        }                                                                           \
        if (cJSON_AddNumberToObject(record, "wt", wt) == NULL) {                    \
            goto end;                                                               \
        }                                                                           \
        if (cJSON_AddNumberToObject(record, "pid", entry_id) == NULL) {             \
            goto end;                                                               \
        }                                                                           \
        if (cJSON_AddStringToObject(record, "name", func) == NULL) {                \
            goto end;                                                               \
        }                                                                           \
        cJSON_AddItemToArray(TWG(records), record);                                 \
        is_record = 1;                                                              \
                                                                                    \
end:                                                                                \
        if (!is_record) {                                                           \
            cJSON_Delete(record);                                                   \
        }                                                                           \
    } while (0)

// 开启函数追踪
#define BEGIN_PROFILING(call_id, func, start_time, execute_data)                    \
    do {                                                                            \
        if (!TWG(enabled) || TWG(records) == NULL) {                                \
            break;                                                                  \
        }                                                                           \
        func = hp_get_function_name(execute_data TSRMLS_CC);                        \
        if (!func) {                                                                \
            break;                                                                  \
        }                                                                           \
        call_id = ++TWG(incr_id);                                                   \
        TWG(entry_id) = call_id;                                                    \
        start_time = cycle_timer();                                                 \
    } while (0)

// 关闭函数追踪
#define END_PROFILING(call_id, func, entry_id, start_time)                          \
    do {                                                                            \
        if (!func) {                                                                \
            break;                                                                  \
        }                                                                           \
        uint64 wt = cycle_timer() - (start_time);                                   \
        TWG(entry_id) = entry_id;                                                   \
        RECORD(call_id, wt, entry_id, func);                                        \
        efree(func);                                                                \
    } while (0)

// 发送数据
#define SEND(data_json)                                                             \
    do {                                                                            \
        printf("%s\n", data_json);      /* TODO */                                  \
    } while (0)

#if PHP_VERSION_ID >= 70000
static void (*_zend_execute_ex) (zend_execute_data *execute_data);
static void (*_zend_execute_internal) (zend_execute_data *execute_data, zval *return_value);
#elif PHP_VERSION_ID < 50500
static void (*_zend_execute) (zend_op_array *ops TSRMLS_DC);
static void (*_zend_execute_internal) (zend_execute_data *data, int ret TSRMLS_DC);
#else
static void (*_zend_execute_ex) (zend_execute_data *execute_data TSRMLS_DC);
static void (*_zend_execute_internal) (zend_execute_data *data, struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif

#if PHP_MAJOR_VERSION == 7
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data);
#elif PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute (zend_op_array *ops TSRMLS_DC);
#else
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data TSRMLS_DC);
#endif
#if PHP_MAJOR_VERSION == 7
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value);
#elif PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, int ret TSRMLS_DC);
#else
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, struct _zend_fcall_info *fci, int ret TSRMLS_DC);
#endif


// 用户自定义函数执行流程
#if PHP_VERSION_ID >= 70000
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data) {
    zend_execute_data *real_execute_data = execute_data;
#elif PHP_VERSION_ID < 50500
ZEND_DLEXPORT void hp_execute (zend_op_array *ops TSRMLS_DC) {
    zend_execute_data *execute_data = EG(current_execute_data);
    zend_execute_data *real_execute_data = execute_data;
#else
ZEND_DLEXPORT void hp_execute_ex (zend_execute_data *execute_data TSRMLS_DC) {
    zend_op_array *ops = execute_data->op_array;
    zend_execute_data    *real_execute_data = execute_data->prev_execute_data;
#endif
    uint64 entry_id = TWG(entry_id);
    uint64 call_id = 0;
    uint64 start_time = 0;
    char *func = NULL;

    // 开启函数追踪
    BEGIN_PROFILING(call_id, func, start_time, real_execute_data);

    // 执行函数
#if PHP_VERSION_ID < 50500
    _zend_execute(ops TSRMLS_CC);
#else
    _zend_execute_ex(execute_data TSRMLS_CC);
#endif

    // 关闭函数追踪
    END_PROFILING(call_id, func, entry_id, start_time);
}

// 内置函数执行流程
#if PHP_VERSION_ID >= 70000
ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data, zval *return_value) {
#elif PHP_VERSION_ID < 50500

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
                                       int ret TSRMLS_DC) {
#else

ZEND_DLEXPORT void hp_execute_internal(zend_execute_data *execute_data,
                                       struct _zend_fcall_info *fci, int ret TSRMLS_DC) {
#endif
    uint64 entry_id = TWG(entry_id);
    uint64 call_id = 0;
    uint64 start_time = 0;
    char *func = NULL;

    // 开启函数追踪
    BEGIN_PROFILING(call_id, func, start_time, execute_data);

    // 执行函数
    if (!_zend_execute_internal) {
#if PHP_VERSION_ID >= 70000
        execute_internal(execute_data, return_value TSRMLS_CC);
#elif PHP_VERSION_ID < 50500
        execute_internal(execute_data, ret TSRMLS_CC);
#else
        execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
    } else {
        /* call the old override */
#if PHP_VERSION_ID >= 70000
        _zend_execute_internal(execute_data, return_value TSRMLS_CC);
#elif PHP_VERSION_ID < 50500
        _zend_execute_internal(execute_data, ret TSRMLS_CC);
#else
        _zend_execute_internal(execute_data, fci, ret TSRMLS_CC);
#endif
    }

    // 关闭函数追踪
    END_PROFILING(call_id, func, entry_id, start_time);
}

// 扩展信息
PHP_MINFO_FUNCTION(hcapm)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "hcapm support", "enabled");
	php_info_print_table_end();
}

ZEND_DECLARE_MODULE_GLOBALS(hcapm)

zend_module_entry hcapm_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
	PHP_HCAPM_EXTNAME,			    /* Extension name */
	NULL,
    PHP_MINIT(hcapm),               /* Module init callback */
    PHP_MSHUTDOWN(hcapm),           /* Module shutdown callback */
    PHP_RINIT(hcapm),               /* Request init callback */
    PHP_RSHUTDOWN(hcapm),           /* Request shutdown callback */
    PHP_MINFO(hcapm),               /* Module info callback */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_HCAPM_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HCAPM
ZEND_GET_MODULE(hcapm)
#endif

// 开启PHP函数拦截
void aop_enable()
{
#if PHP_VERSION_ID < 50500
    _zend_execute = zend_execute;
    zend_execute  = hp_execute;
#else
    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex  = hp_execute_ex;
#endif

    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal = hp_execute_internal;
}

// 关闭PHP函数拦截
void aop_disable()
{
#if PHP_VERSION_ID < 50500
    zend_execute = _zend_execute;
#else
    zend_execute_ex = _zend_execute_ex;
#endif

    zend_execute_internal = _zend_execute_internal;
}

// 请求开始钩子
PHP_RINIT_FUNCTION(hcapm)
{
    TWG(start_time) = cycle_timer();
    TWG(entry_id) = 0;
    TWG(incr_id) = 0;
    TWG(enabled) = 1;
    TWG(records) = cJSON_CreateArray();

    if (TWG(records) == NULL) {
        TWG(enabled) = 0; // 创建全局保存对象失败，关闭函数追踪
    }

	return SUCCESS;
}

// 请求结束钩子
PHP_RSHUTDOWN_FUNCTION(hcapm)
{
    uint64 wt = cycle_timer() - TWG(start_time);
    char *data_json = NULL;
    cJSON *data =  cJSON_CreateObject();

    if (TWG(records) == NULL) {
        goto end;
    }

    cJSON_AddItemToObject(data, "records", TWG(records));

    if (cJSON_AddNumberToObject(data, "wt", wt) == NULL) {
        goto end;
    }

    // data_json = cJSON_PrintUnformatted(data);            // prod
    data_json = cJSON_Print(data);                          // dev
    if (data_json == NULL) {
        goto end;
    }
    SEND(data_json);
    cJSON_free(data_json);

end:
    cJSON_Delete(data);

    return SUCCESS;
}

// 扩展装载钩子
PHP_MINIT_FUNCTION(hcapm)
{
    aop_enable();

    return SUCCESS;
}

// 扩展卸载钩子
PHP_MSHUTDOWN_FUNCTION(hcapm)
{
    aop_disable();

    return SUCCESS;
}
