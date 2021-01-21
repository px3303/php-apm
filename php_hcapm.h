/* hcapm extension for PHP */

#ifndef PHP_HCAPM_H
# define PHP_HCAPM_H

#ifdef ZTS
#include "TSRM.h"
#endif

#include "cJSON.h"

extern zend_module_entry hcapm_module_entry;
# define phpext_hcapm_ptr &hcapm_module_entry

# define PHP_HCAPM_VERSION "0.1.0"
# define PHP_HCAPM_EXTNAME "hcapm"

#if !defined(uint64)
typedef unsigned long long uint64;
#endif

#ifdef ZTS
#define TWG(v) TSRMG(hcapm_globals_id, zend_hcapm_globals *, v)
#else
#define TWG(v) (hcapm_globals.v)
#endif

ZEND_BEGIN_MODULE_GLOBALS(hcapm)
    int             enabled;            // 是否启用函数追踪
    uint64          start_time;         // 请求开始时间
    uint64          entry_id;           // 上个节点函数ID
    uint64          incr_id;            // 函数自增ID
    cJSON           *records;           // 全局追踪数据的保存对象
ZEND_END_MODULE_GLOBALS(hcapm)

extern ZEND_DECLARE_MODULE_GLOBALS(hcapm)

PHP_MINIT_FUNCTION(hcapm);
PHP_MSHUTDOWN_FUNCTION(hcapm);
PHP_RINIT_FUNCTION(hcapm);
PHP_RSHUTDOWN_FUNCTION(hcapm);
PHP_MINFO_FUNCTION(hcapm);


#endif	/* PHP_HCAPM_H */
