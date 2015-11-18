THIRDPARTY_BUILD_DIR="$srcdir/thirdparty/build"
R3_SOURCE="thirdparty/r3"

AC_PROG_CC
AC_PROG_CC_STDC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memset strchr strdup strndup strstr])

PHP_ARG_WITH(pcre-dir, libpcre install dir,
[  --with-pcre-dir=DIR PCRE: libpcre install prefix])

PHP_ADD_INCLUDE("$srcdir/thirdparty/build/include")
PHP_ADD_INCLUDE("$R3_SOURCE/include")
PHP_ADD_INCLUDE("$R3_SOURCE/3rdparty")
PHP_ADD_INCLUDE("$phpincludedir/ext/pcre/pcrelib")

PHP_ARG_ENABLE(php_ext_web_util, whether to enable php_ext_web_util support,
Make sure that the comment is aligned:
[ --enable-php_ext_web_util Enable php_ext_web_util support])

MODULES="
    $R3_SOURCE/3rdparty/zmalloc.c
    $R3_SOURCE/src/str.c
    $R3_SOURCE/src/token.c
    $R3_SOURCE/src/slug.c
    $R3_SOURCE/src/edge.c
    $R3_SOURCE/src/node.c
    $R3_SOURCE/src/list.c
    $R3_SOURCE/src/match_entry.c
    php_ext_web_util.c
    src/bstring.c
    src/web_util_r3.c
    src/web_util_http_parser.c
"
PHP_NEW_EXTENSION(php_ext_web_util, $MODULES, $ext_shared)

PHP_ADD_MAKEFILE_FRAGMENT([Makefile.thirdparty])

dnl PHP_EXT_WEB_UTIL_SHARED_DEPENDENCIES="$THIRDPARTY_BUILD_DIR/lib/libhttp_parser.o"
dnl EXTRA_LDFLAGS="$EXTRA_LDFLAGS $THIRDPARTY_BUILD_DIR/lib/libhttp_parser.o"

  AC_CHECK_DECL(
    [HAVE_BUNDLED_PCRE],
    [AC_CHECK_HEADERS(
      [ext/pcre/php_pcre.h],
      [
        PHP_ADD_EXTENSION_DEP([r3], [pcre])
      ],
      ,
      [[#include "main/php.h"]]
    )],[
      dnl Detect pcre
      if test "$PHP_PCRE_DIR" != "yes" ; then
        AC_MSG_CHECKING([for PCRE headers location])
        for i in $PHP_PCRE_DIR $PHP_PCRE_DIR/include $PHP_PCRE_DIR/include/pcre $PHP_PCRE_DIR/local/include; do
          test -f $i/pcre.h && PCRE_INCDIR=$i
        done
      else
        AC_MSG_CHECKING([for PCRE headers location])
        for i in /usr/include /usr/local/include /usr/local/include/pcre /opt/local/include; do
          test -f $i/pcre.h && PCRE_INCDIR=$i
        done
      fi

      if test -z "$PCRE_INCDIR"; then
        AC_MSG_ERROR([Could not find pcre.h in $PHP_PCRE_DIR])
      fi
      AC_MSG_RESULT([PCRE Include: $PCRE_INCDIR])
      
      
      AC_MSG_CHECKING([for PCRE library location])
      if test "$PHP_PCRE_DIR" != "yes" ; then
        for j in $PHP_PCRE_DIR $PHP_PCRE_DIR/$PHP_LIBDIR; do
          test -f $j/libpcre.a || test -f $j/libpcre.$SHLIB_SUFFIX_NAME && PCRE_LIBDIR=$j
        done
      else
        for j in /usr/lib /usr/local/lib /opt/local/lib; do
          test -f $j/libpcre.a || test -f $j/libpcre.$SHLIB_SUFFIX_NAME && PCRE_LIBDIR=$j
        done
      fi

      if test -z "$PCRE_LIBDIR" ; then
        AC_MSG_ERROR([Could not find libpcre.(a|$SHLIB_SUFFIX_NAME) in $PHP_PCRE_DIR])
      fi
      AC_MSG_RESULT([$PCRE_LIBDIR])

      AC_DEFINE(HAVE_PCRE, 1, [ ])
      PHP_ADD_INCLUDE($PCRE_INCDIR)
      PHP_ADD_LIBRARY_WITH_PATH(pcre, $PCRE_LIBDIR, PHP_EXT_WEB_UTIL_SHARED_LIBADD)
    ], [[#include "php_config.h"]]
  )
shared_objects_php_ext_web_util="$THIRDPARTY_BUILD_DIR/lib/libhttp_parser.o $shared_objects_php_ext_web_util"
PHP_SUBST(PHP_EXT_WEB_UTIL_SHARED_LIBADD)
dnl PHP_SUBST(PHP_EXT_WEB_UTIL_SHARED_DEPENDENCIES)
