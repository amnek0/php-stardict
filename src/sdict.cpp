// $Id: sdict.cpp 4 2008-05-02 06:59:09Z glinus $

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_sdict.h"
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <clocale>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <map>

static char ini_data_dir[] = "sdict.data_dir";

int le_sdict_library;
int le_sdict_library_persist;

ZEND_DECLARE_MODULE_GLOBALS(sdict)

static function_entry sdict_functions[] = {
    PHP_FE(sdict_query, NULL)
    PHP_FE(sdict_open,  NULL)
    PHP_FE(sdict_popen, NULL)
    PHP_FE(sdict_close, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry sdict_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_SDICT_EXTNAME,
    sdict_functions,
    PHP_MINIT(sdict),
    PHP_MSHUTDOWN(sdict),
    PHP_RINIT(sdict),
    NULL,
    PHP_MINFO(sdict),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SDICT_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SDICT
ZEND_GET_MODULE(sdict)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("sdict.data_dir", "/usr/share/dict", PHP_INI_ALL, NULL)
PHP_INI_END()

static void php_sdict_init_globals(zend_sdict_globals *sdict_globals)
{
}

PHP_RINIT_FUNCTION(sdict)
{
    SDICT_G(counter) = 0;
    return SUCCESS;
}

static void php_sdict_library_persist_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    php_sdict_library *sdict= (php_sdict_library *)rsrc->ptr;
    if (sdict) {
        if (sdict->lib) {
            delete sdict->lib;
        }
        if (sdict->data_dir) {
            pefree(sdict->data_dir, 1);
        }
        pefree(sdict, 1);
    }
}

static void php_sdict_library_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    php_sdict_library *sdict= (php_sdict_library *)rsrc->ptr;
    if (sdict) {
        if (sdict->lib) {
            delete sdict->lib;
        }
        if (sdict->data_dir) {
            efree(sdict->data_dir);
        }
        efree(sdict);
    }
}

PHP_MINIT_FUNCTION(sdict)
{
    le_sdict_library = zend_register_list_destructors_ex(
            php_sdict_library_dtor, NULL, PHP_SDICT_LIBRARY_RES_NAME, module_number);
    le_sdict_library_persist = zend_register_list_destructors_ex(
            NULL, php_sdict_library_persist_dtor, PHP_SDICT_LIBRARY_RES_NAME, module_number);

    ZEND_INIT_MODULE_GLOBALS(sdict, php_sdict_init_globals, NULL);

    REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(sdict)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

/**
 * Load dictionaries from Directory
 * @param string directory
 * @return TRUE on success, FALSE on failure
 */
PHP_FUNCTION(sdict_load)
{
    php_sdict_library *sdict;
    Library *library;
    zval *zlibrary;
    char *directory;
    int name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zlibrary, &directory, &name_len) == FAILURE) {
        RETURN_FALSE;
    }

    strlist_t dicts_dir_list, empty_list, disable_list;
    dicts_dir_list.push_back(directory);

    ZEND_FETCH_RESOURCE2(sdict, php_sdict_library*, &zlibrary, -1, PHP_SDICT_LIBRARY_RES_NAME, le_sdict_library, le_sdict_library_persist);
    library = sdict->lib;

    library->load(dicts_dir_list, empty_list, disable_list);

    RETURN_TRUE;
}

/**
 * Do Query
 * @param string word
 * @return array matches
 */
PHP_FUNCTION(sdict_query)
{
    php_sdict_library *sdict;
    Library *library;
    zval *zlibrary;
    char *word;
    int name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zlibrary, &word, &name_len) == FAILURE) {
        RETURN_FALSE;
    }

    ++SDICT_G(counter);
    ZEND_FETCH_RESOURCE2(sdict, php_sdict_library*, &zlibrary, -1, PHP_SDICT_LIBRARY_RES_NAME, le_sdict_library, le_sdict_library_persist);
    library = sdict->lib;

    std::string query;
    analyze_query(word, query);

    if (query.empty()) RETURN_NULL();

    gsize bytes_read;
    gsize bytes_written;
    GError *err=NULL;
    char *str=NULL;
    if (false/*!utf8_input*/)
        str=g_locale_to_utf8(word, -1, &bytes_read, &bytes_written, &err);
    else
        str=g_strdup(word);

    if (NULL==str) RETURN_NULL(); 
    if (str[0]=='\0') RETURN_NULL();

    TSearchResultList res_list;

    switch (analyze_query(str, query)) {
        case qtFUZZY:
            library->LookupWithFuzzy(query, res_list);
            break;
        case qtREGEXP:
            library->LookupWithRule(query, res_list);
            break;
        case qtSIMPLE:
            library->SimpleLookup(str, res_list);
            if (res_list.empty())
                library->LookupWithFuzzy(str, res_list);
            break;
        case qtDATA:
            library->LookupData(query, res_list);
            break;
        default:
            /*nothing*/;
    }

    if (res_list.empty()) RETURN_NULL();

    bool show_all_results=true;
    typedef std::map< string, int, std::less<string> > DictResMap;
    DictResMap res_per_dict;
    for(TSearchResultList::iterator ptr=res_list.begin(); ptr!=res_list.end(); ++ptr){
        std::pair<DictResMap::iterator, DictResMap::iterator> r = 
            res_per_dict.equal_range(ptr->bookname);
        DictResMap tmp(r.first, r.second);
        if (tmp.empty()) //there are no yet such bookname in map
            res_per_dict.insert(DictResMap::value_type(ptr->bookname, 1));
        else {
            ++((tmp.begin())->second);
            if (tmp.begin()->second>1) {
                show_all_results=false;
                break;
            }
        }
    }

    if (!show_all_results) {
        array_init(return_value);
        zval *multi;
        ALLOC_INIT_ZVAL(multi);
        array_init(multi);
        add_assoc_string(return_value, "type", "match", 1);
        for (size_t i=0; i<res_list.size(); ++i) {
            zval *sub;
            ALLOC_INIT_ZVAL(sub);
            array_init(sub);
            add_assoc_string(sub, "bookname", (char *)res_list[i].bookname.c_str(), 1);
            add_assoc_string(sub, "def", (char *)res_list[i].def.c_str(), 1);
            add_next_index_zval(multi, sub);
        }
        add_assoc_zval(return_value, "result", multi);
    } else {
        string loc_bookname, loc_def, loc_exp;
        zval *result;
        ALLOC_INIT_ZVAL(result);
        array_init(return_value);
        array_init(result);
        add_assoc_string(return_value, "type", "return", 1);
        for (PSearchResult ptr = res_list.begin(); ptr!=res_list.end();++ptr) {
            zval *sub;
            ALLOC_INIT_ZVAL(sub);
            array_init(sub);
            add_assoc_string(sub, "bookname", (char *)ptr->bookname.c_str(), 1);
            add_assoc_string(sub, "def", (char *)ptr->def.c_str(), 1);
            add_assoc_string(sub, "exp", (char *)ptr->exp.c_str(), 1);
            add_next_index_zval(result, sub);
        }
        add_assoc_zval(return_value, "result", result);
    }
}

/* Resource Init */
PHP_FUNCTION(sdict_open)
{
    php_sdict_library *sdict;
    char *directory;
    int name_len;

    if (0 == ZEND_NUM_ARGS()) {
        directory = INI_STR(ini_data_dir);
    }
    else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &directory, &name_len) == FAILURE) {
        RETURN_FALSE;
    }

    strlist_t dicts_dir_list, empty_list, disable_list;
    dicts_dir_list.push_back(directory);

    sdict = (php_sdict_library *)emalloc(sizeof(php_sdict_library));
    sdict->lib = new Library(true, true);
    sdict->data_dir = estrndup(directory, name_len);
    sdict->lib->load(dicts_dir_list, empty_list, disable_list);
    ZEND_REGISTER_RESOURCE(return_value, sdict, le_sdict_library);

}

/* Resource Persist Init */
PHP_FUNCTION(sdict_popen)
{
    php_sdict_library *sdict;
    Library *lib;
    char *directory;
    char *key;
    int key_len, name_len;
    list_entry *le, new_le;

    if (0 == ZEND_NUM_ARGS()) {
        directory = INI_STR(ini_data_dir);
    }
    else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &directory, &name_len) == FAILURE) {
        RETURN_FALSE;
    }

    strlist_t dicts_dir_list, empty_list, disable_list;
    dicts_dir_list.push_back(directory);

    /* Look for an established resource */
    key_len = spprintf(&key, 0, "sdict_library__%s", directory);
    if (zend_hash_find(&EG(persistent_list), key, key_len + 1,(void **)&le) == SUCCESS) {
        /* An entry for this library already exists */
        ZEND_REGISTER_RESOURCE(return_value, le->ptr, le_sdict_library_persist);
        efree(key);
        return;
    }

    /* New library, allocate a structure */
    sdict = (php_sdict_library *)pemalloc(sizeof(php_sdict_library), 1);
    ZEND_REGISTER_RESOURCE(return_value, sdict, le_sdict_library_persist);
    sdict->lib = new Library(true, true);
    sdict->lib->load(dicts_dir_list, empty_list, disable_list);
    sdict->data_dir = (char *)pemalloc(name_len + 1, 1);
    memcpy(sdict->data_dir, directory, name_len + 1);

    /* Store a reference in the persistence list */
    new_le.ptr = sdict;
    new_le.type = le_sdict_library_persist;
    zend_hash_add(&EG(persistent_list), key, key_len + 1, &new_le, sizeof(list_entry), NULL);

    efree(key);
}

/* Resource Delete */
PHP_FUNCTION(sdict_close)
{
    zval *zlibrary;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zlibrary) == FAILURE) {
        RETURN_FALSE;
    }

    zend_list_delete(Z_LVAL_P(zlibrary));
    RETURN_TRUE;
}

/* phpinfo */
PHP_MINFO_FUNCTION(sdict)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "StarDict Support", "enabled" );
    php_info_print_table_row(2, "Dictionary Repos", INI_STR(ini_data_dir) );
    php_info_print_table_row(2, "Global Counter",   SDICT_G(counter) );
    php_info_print_table_end();

}
