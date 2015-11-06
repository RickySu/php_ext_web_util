THIRDPARTY_BUILD_DIR="$srcdir/thirdparty/build"

dnl PHP_ADD_INCLUDE("$srcdir/thirdparty/r3/include")
PHP_ADD_INCLUDE("$srcdir/thirdparty/build/include")

PHP_ARG_ENABLE(php_ext_web_util, whether to enable php_ext_web_util support,
Make sure that the comment is aligned:
[ --enable-php_ext_web_util Enable php_ext_web_util support])

MODULES="
    php_ext_web_util.c
    src/bstring.c
    src/web_util_r3.c
    src/web_util_http_parser.c
"
PHP_NEW_EXTENSION(php_ext_web_util, $MODULES, $ext_shared)

PHP_ADD_MAKEFILE_FRAGMENT([Makefile.thirdparty])

PHP_EXT_WEB_UTIL_SHARED_DEPENDENCIES="$THIRDPARTY_BUILD_DIR/lib/libr3.a $THIRDPARTY_BUILD_DIR/lib/libhttp_parser.o"
EXTRA_LDFLAGS="$EXTRA_LDFLAGS $THIRDPARTY_BUILD_DIR/lib/libr3.a $THIRDPARTY_BUILD_DIR/lib/libhttp_parser.o"

PHP_SUBST(PHP_EXT_WEB_UTIL_SHARED_DEPENDENCIES)
