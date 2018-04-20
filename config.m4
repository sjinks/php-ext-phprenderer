PHP_ARG_ENABLE(phprenderer, whether to enable PHP Renderer extension [ --enable-phprenderer  Enable PHP Renderer extension])

if test "$PHP_PHPRENDERER" = "yes"; then
	AC_DEFINE([HAVE_PHPRENDERER], [1], [Whether PHP Renderer extension is enabled])
	PHP_NEW_EXTENSION([phprenderer], [phprenderer.c], $ext_shared,, [-Wall -std=gnu99])
fi
