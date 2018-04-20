/*
 * phprenderer.c
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <main/php.h>
#include <main/php_output.h>
#include <ext/spl/spl_exceptions.h>
#include <ext/standard/info.h>
#include <ext/standard/php_array.h>
#include <ext/standard/php_filestat.h>
#include <ext/standard/php_string.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_execute.h>
#include <Zend/zend_string.h>
#include "php_phprenderer.h"

struct phprenderer_object {
    zend_string* template_path;
    zval attributes;
    zend_object zo;
};

zend_class_entry* phprenderer_ce = NULL;
static zend_object_handlers ce_handlers;
static zend_string* i_getbody = NULL;
static zend_string* i_write   = NULL;

static inline struct phprenderer_object* phprenderer_fetch_object(zend_object* obj)
{
    return (struct phprenderer_object*)((char*)(obj) - XtOffsetOf(struct phprenderer_object, zo));
}

static zend_object* create_object(zend_class_entry* ce)
{
    struct phprenderer_object* res = ecalloc(1, sizeof(struct phprenderer_object) + zend_object_properties_size(ce));

    res->template_path = NULL;
    array_init_size(&res->attributes, 0);

    zend_object_std_init(&res->zo, ce);
    object_properties_init(&res->zo, ce);
    res->zo.handlers = &ce_handlers;
    return &res->zo;
}

static void free_object(zend_object* object)
{
    struct phprenderer_object* obj = phprenderer_fetch_object(object);

    if (obj->template_path) {
        zend_string_release(obj->template_path);
    }

    zval_dtor(&obj->attributes);
    zend_object_std_dtor(&obj->zo);
}

static void set_template_path(struct phprenderer_object* obj, zend_string* path)
{
    if (obj->template_path) {
        zend_string_release(obj->template_path);
    }

    if (path) {
        obj->template_path = php_trim(path, ZEND_STRL("\\/"), 2);
        size_t len         = ZSTR_LEN(obj->template_path);
        obj->template_path = zend_string_extend(obj->template_path, len+1, 0);
        ZSTR_VAL(obj->template_path)[len] = '/';
    }
    else {
        obj->template_path = zend_string_init(ZEND_STRL("/"), 0);
    }
}

static zend_execute_data* find_symbol_table(zend_execute_data* ex)
{
    while (ex && (!ex->func || !ZEND_USER_CODE(ex->func->common.type))) {
        ex = ex->prev_execute_data;
    }

    return ex;
}

static char* build_path(zend_string* dir, zend_string* file)
{
    size_t dir_len  = ZSTR_LEN(dir);
    size_t file_len = ZSTR_LEN(file);
    char* s         = ecalloc(1, dir_len + file_len + 1);

    memcpy(s,           ZSTR_VAL(dir),  dir_len);
    memcpy(s + dir_len, ZSTR_VAL(file), file_len);
    return s;
}

/**
 * @note s will be freed upon return
 * @param s
 */
static void execute_script(char* s)
{
    zval r;

    zend_file_handle fh;
    fh.filename      = s;
    fh.free_filename = 1;
    fh.type          = ZEND_HANDLE_FILENAME;
    fh.opened_path   = NULL;
    fh.handle.fp     = NULL;

    ZVAL_UNDEF(&r);

    /* this will call zend_destroy_file_handle() for fh */
    zend_execute_scripts(ZEND_INCLUDE, &r, 1, &fh);
    zval_dtor(&r);
}

static int kill_globals(zval* pDest, int num_args, va_list args, zend_hash_key* hash_key)
{
    const char* GLOBALS = "GLOBALS";
    if (
            hash_key->key && ZSTR_LEN(hash_key->key) == strlen(GLOBALS)
         && ZSTR_HASH(hash_key->key) == zend_inline_hash_func(ZEND_STRL("GLOBALS"))
         && !memcmp(ZSTR_VAL(hash_key->key), GLOBALS, strlen(GLOBALS))
    ) {
        return ZEND_HASH_APPLY_REMOVE;
    }

    return ZEND_HASH_APPLY_KEEP;
}

static void fetch(zval* return_value, zend_execute_data* execute_data, struct phprenderer_object* intern, zend_string* tpl, zval* data)
{
    if (UNEXPECTED(!intern->template_path)) {
        zend_throw_exception(spl_ce_LogicException, "fetch() called on uninitialized object", 0);
        return;
    }

    zval r;
    char* s = build_path(intern->template_path, tpl);
    php_stat(s, strlen(s), FS_IS_FILE, &r);

    if (!zend_is_true(&r)) {
        zend_throw_exception_ex(spl_ce_RuntimeException, 0, "View cannot render `%s` because the template does not exist", s);
        efree(s);
        return;
    }

    zend_rebuild_symbol_table();
    zend_execute_data* ex = find_symbol_table(execute_data);
    if (UNEXPECTED(!ex)) {
        efree(s);
        zend_throw_exception(spl_ce_LogicException, "Internal error: unable to find a symbol table", 0);
        return;
    }

    zval merged;
    ZVAL_ZVAL(&merged, &intern->attributes, 1, 0);

    if (data) {
        php_array_merge(Z_ARRVAL(merged), Z_ARRVAL_P(data));
    }

    zend_hash_apply_with_arguments(Z_ARRVAL(merged), kill_globals, 0);

    zend_detach_symbol_table(ex);
    zend_array* old  = ex->symbol_table;
    ex->symbol_table = Z_ARRVAL(merged);

    php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS);
    execute_script(s);

    if (!EG(exception)) {
        php_output_get_contents(return_value);
    }

    php_output_discard();

    zval_dtor(&merged);
    ex->symbol_table = old;
    zend_attach_symbol_table(ex);
    zend_rebuild_symbol_table();
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, __construct)
{
    struct phprenderer_object* intern = NULL;
    zend_string* path = NULL;
    zval* attrs       = NULL;
    zval* object      = getThis();

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "|P!a!", &path, &attrs)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    set_template_path(intern, path);

    if (attrs) {
        ZVAL_ZVAL(&intern->attributes, attrs, 1, 0);
    }
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, render)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();
    zval* response;
    zend_string* tpl;
    zval* data = NULL;
    zval s;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "zP|a!", &response, &tpl, &data)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    fetch(&s, execute_data, intern, tpl, data);
    if (UNEXPECTED(EG(exception))) {
        return;
    }

    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    zval callable;
    zval method;
    zval body;

    ZVAL_STR(&method, zend_string_copy(i_getbody));

    array_init_size(&callable, 2);
    zend_hash_real_init(Z_ARRVAL(callable), 1);
    ZEND_HASH_FILL_PACKED(Z_ARRVAL(callable)) {
        Z_ADDREF_P(response);
        ZEND_HASH_FILL_ADD(response);
        ZEND_HASH_FILL_ADD(&method);
    } ZEND_HASH_FILL_END();

    zend_fcall_info_init(&callable, 0, &fci, &fcc, NULL, NULL);
    zend_fcall_info_call(&fci, &fcc, &body, NULL);
    if (UNEXPECTED(EG(exception))) {
        zval_dtor(&callable);
        zval_dtor(&s);
        return;
    }

    zend_hash_clean(Z_ARRVAL(callable)); /* This will destroy method */
    ZVAL_STR(&method, zend_string_copy(i_write));

    add_next_index_zval(&callable, &body);
    add_next_index_zval(&callable, &method);

    zend_fcall_info_init(&callable, 0, &fci, &fcc, NULL, NULL);
    zend_fcall_info_argn(&fci, 1, &s);
    zend_fcall_info_call(&fci, &fcc, &body, NULL);
    zend_fcall_info_args_clear(&fci, 1); /* This will destroy s */
    zval_dtor(&callable); /* This will destroy body and method */

    RETURN_ZVAL(response, 1, 0);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, fetch)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();
    zend_string* tpl;
    zval* data = NULL;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "P|a!", &tpl, &data)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    fetch(return_value, execute_data, intern, tpl, data);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, getAttributes)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();

    if (FAILURE == zend_parse_parameters_none()) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    RETURN_ZVAL(&intern->attributes, 1, 0);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, setAttributes)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();
    zval* attrs;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "a", &attrs)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    zval_dtor(&intern->attributes);
    ZVAL_ZVAL(&intern->attributes, attrs, 1, 0);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, addAttribute)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();
    zend_string* key;
    zval* value;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "Sz", &key, &value)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    Z_TRY_ADDREF_P(value);
    zend_symtable_update(Z_ARRVAL(intern->attributes), key, value);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, getAttribute)
{
    struct phprenderer_object* intern = NULL;
    zval* object = getThis();
    zend_string* key;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "S", &key)) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));
    zval* ret = zend_symtable_find(Z_ARRVAL(intern->attributes), key);
    if (!ret) {
        RETURN_FALSE;
    }

    RETURN_ZVAL(ret, 1, 0);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, getTemplatePath)
{
    struct phprenderer_object* intern = NULL;
    zval* object                      = getThis();

    if (FAILURE == zend_parse_parameters_none()) {
        return;
    }

    intern = phprenderer_fetch_object(Z_OBJ_P(object));

    if (EXPECTED(intern->template_path)) {
        RETURN_STR_COPY(intern->template_path);
    }

    zend_throw_exception(spl_ce_LogicException, "getTemplatePath() called on uninitialized object", 0);
}

static PHP_METHOD(WildWolf_Views_PhpRenderer, setTemplatePath)
{
    struct phprenderer_object* intern = NULL;
    zend_string* path;

    if (FAILURE == zend_parse_parameters_throw(ZEND_NUM_ARGS(), "P", &path)) {
        return;
    }

    zval* obj = getThis();
    intern    = phprenderer_fetch_object(Z_OBJ_P(obj));
    set_template_path(intern, path);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo___construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, templatePath, IS_STRING, 1)
    ZEND_ARG_ARRAY_INFO(0, attributes, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setAttributes, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, attributes, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_addAttribute, 0, 0, 2)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getAttribute, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setTemplatePath, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

#if PHP_VERSION_ID < 70200
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_render, 0, 2, IS_OBJECT, "Psr\\Http\\Message\\ResponseInterface", 0)
    ZEND_ARG_OBJ_INFO(0, response, Psr\\Http\\Message\\ResponseInterface, 0)
    ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
    ZEND_ARG_ARRAY_INFO(0, data, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fetch, 0, 1, IS_STRING, NULL, 0)
    ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
    ZEND_ARG_ARRAY_INFO(0, data, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_getAttributes, 0, 0, IS_ARRAY, NULL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_getTemplatePath, 0, 0, IS_STRING, NULL, 0)
ZEND_END_ARG_INFO()
#else
ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_render, 0, 2, "Psr\\Http\\Message\\ResponseInterface", 0)
    ZEND_ARG_OBJ_INFO(0, response, Psr\\Http\\Message\\ResponseInterface, 0)
    ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
    ZEND_ARG_ARRAY_INFO(0, data, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fetch, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
    ZEND_ARG_ARRAY_INFO(0, data, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_getAttributes, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_getTemplatePath, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()
#endif

static const zend_function_entry fe_wildwolf_views_phprenderer[] = {
    PHP_ME(WildWolf_Views_PhpRenderer, __construct,     arginfo___construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(WildWolf_Views_PhpRenderer, render,          arginfo_render,          ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, fetch,           arginfo_fetch,           ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, getAttributes,   arginfo_getAttributes,   ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, setAttributes,   arginfo_setAttributes,   ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, addAttribute,    arginfo_addAttribute,    ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, getAttribute,    arginfo_getAttribute,    ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, getTemplatePath, arginfo_getTemplatePath, ZEND_ACC_PUBLIC)
    PHP_ME(WildWolf_Views_PhpRenderer, setTemplatePath, arginfo_setTemplatePath, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static PHP_MINIT_FUNCTION(phprenderer)
{
    zend_class_entry ce;

    i_getbody = zend_new_interned_string(zend_string_init(ZEND_STRL("getbody"), 1));
    i_write   = zend_new_interned_string(zend_string_init(ZEND_STRL("write"),   1));

    INIT_CLASS_ENTRY(ce, "WildWolf\\Views\\PhpRenderer", fe_wildwolf_views_phprenderer);
    phprenderer_ce = zend_register_internal_class(&ce);
    if (EXPECTED(phprenderer_ce)) {
        phprenderer_ce->create_object = create_object;
        memcpy(&ce_handlers, zend_get_std_object_handlers(), sizeof(ce_handlers));
        ce_handlers.offset   = XtOffsetOf(struct phprenderer_object, zo);
        ce_handlers.free_obj = free_object;
        ce_handlers.dtor_obj = zend_objects_destroy_object;

        return SUCCESS;
    }

    return FAILURE;
}

static PHP_MSHUTDOWN_FUNCTION(phprenderer)
{
    if (i_getbody) {
        zend_string_release(i_getbody);
        zend_string_release(i_write);
    }

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(phprenderer)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "PHP Renderer Extension", "enabled");
    php_info_print_table_row(2, "Version", PHP_PHPRENDERER_EXTVER);
    php_info_print_table_end();
}

zend_module_entry phprenderer_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_PHPRENDERER_EXTNAME,
    NULL,
    PHP_MINIT(phprenderer),
    PHP_MSHUTDOWN(phprenderer),
    NULL,
    NULL,
    PHP_MINFO(phprenderer),
    PHP_PHPRENDERER_EXTVER,
    NO_MODULE_GLOBALS,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PHPRENDERER
ZEND_GET_MODULE(phprenderer)
#endif
