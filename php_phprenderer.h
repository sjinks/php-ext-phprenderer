/*
 * php_phprenderer.h
 *
 */

#ifndef PHP_PHPRENDERER_H
#define PHP_PHPRENDERER_H

#include <main/php.h>

#define PHP_PHPRENDERER_EXTNAME  "phprenderer"
#define PHP_PHPRENDERER_EXTVER   "1.0"

#if defined(__GNUC__) && __GNUC__ >= 4
#   define PHPRENDERER_VISIBILITY_HIDDEN __attribute__((visibility("hidden")))
#else
#   define PHPRENDERER_VISIBILITY_HIDDEN
#endif

#ifdef COMPILE_DL_PHPRENDERER
PHPRENDERER_VISIBILITY_HIDDEN
#endif
extern zend_module_entry phprenderer_module_entry;

extern zend_class_entry* phprenderer_ce;

#endif /* PHP_PHPRENDERER_H */
