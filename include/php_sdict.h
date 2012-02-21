// $Id: php_sdict.h 4 2008-05-02 06:59:09Z glinus $

#ifndef PHP_SDICT_H
#define PHP_SDICT_H 1

#ifdef ZTS
#include "TSRM.h"
#endif

#include "libwrapper.hpp"

ZEND_BEGIN_MODULE_GLOBALS(sdict)
    long counter;
ZEND_END_MODULE_GLOBALS(sdict)

#ifdef ZTS
#define SDICT_G(v) TSRMG(sdict_globals_id, zend_sdict_globals *, v)
#else
#define SDICT_G(v) (sdict_globals.v)
#endif

#define PHP_SDICT_VERSION "1.0"
#define PHP_SDICT_EXTNAME "sdict"

typedef struct _sdict_library {
    Library *lib;
    char *data_dir;
} php_sdict_library;

#define PHP_SDICT_LIBRARY_RES_NAME "StarDict Library"

PHP_MINIT_FUNCTION(sdict);
PHP_MSHUTDOWN_FUNCTION(sdict);
PHP_RINIT_FUNCTION(sdict);
PHP_MINFO_FUNCTION(sdict);

PHP_FUNCTION(sdict_query);

PHP_FUNCTION(sdict_open);
PHP_FUNCTION(sdict_popen);
PHP_FUNCTION(sdict_close);

extern zend_module_entry sdict_module_entry;
#define phpext_sdict_ptr &sdict_module_entry

#endif
