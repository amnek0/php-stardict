PHP_ARG_ENABLE(sdict, whether to enable StarDict support,
[ --enable-sdict   Enable StarDict support])

if test "$PHP_SDICT" = "yes"; then
  AC_DEFINE(HAVE_SDICT, 1, [Whether you have StarDict])
  PHP_NEW_EXTENSION(sdict, src/sdict.cpp src/libwrapper.cpp src/utils.cpp src/lib/dictziplib.cpp src/lib/distance.cpp src/lib/lib.cpp, $ext_shared)
fi

dnl PHP_ADD_INCLUDE(/usr/include/glib-2.0)
dnl PHP_ADD_INCLUDE(/usr/lib64/glib-2.0/include)

GLIB2_INCLINE=`pkg-config --cflags glib-2.0`
GLIB2_LIBLINE=`pkg-config --libs glib-2.0`
PHP_EVAL_INCLINE($GLIB2_INCLINE)
PHP_EVAL_LIBLINE($GLIB2_LIBLINE, GLIB2_SHARED_LIBADD)

PHP_ADD_LIBRARY_WITH_PATH(stdc++, /usr/lib/, CPPEXT_SHARED_LIBADD)
PHP_REQUIRE_CXX()
PHP_SUBST(GLIB2_SHARED_LIBADD CPPEXT_SHARED_LIBADD)

LDFLAGS="${LDFLAGS} ${GLIB2_LIBLINE}"
