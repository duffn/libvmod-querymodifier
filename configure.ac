AC_PREREQ([2.68])
AC_INIT([libvmod-querymodifier], [0.1])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.12 -Wall -Werror foreign parallel-tests])
AM_SILENT_RULES([yes])
AM_PROG_AR

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])

AC_ARG_WITH([rst2man],
	AS_HELP_STRING(
		[--with-rst2man=PATH],
		[Location of rst2man (auto)]),
	[RST2MAN="$withval"],
	[AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], [])])

AC_ARG_ENABLE([asan],
	AS_HELP_STRING([--enable-asan],
		[enable address sanitizer (default is NO)]),
	[
		ASAN_FLAGS="-fsanitize-recover=address"
		AC_DEFINE([ENABLE_ASAN], [1],
			[Define to 1 if ASAN sanitizer is enabled.])
	], [])

if test -n "$ASAN_FLAGS"; then
	AX_CHECK_COMPILE_FLAG([$ASAN_FLAGS -fsanitize-address-use-after-scope],
		[ASAN_FLAGS="$ASAN_FLAGS -fsanitize-address-use-after-scope"],
		[])
fi

CFLAGS="$CFLAGS $ASAN_FLAGS"
LDFLAGS="$LDFLAGS $ASAN_FLAGS"
VARNISH_PREREQ([7.0.0])
VARNISH_VMODS([querymodifier])

VMOD_TESTS="$(cd $srcdir/src && echo vtc/*.vtc)"
AC_SUBST(VMOD_TESTS)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	libvmod-querymodifier.spec
])

AC_OUTPUT

AS_ECHO("
	==== $PACKAGE_STRING ====

	varnish:      $VARNISH_VERSION
	prefix:       $prefix
	vmoddir:      $vmoddir
	vcldir:       $vcldir
	pkgvcldir:    $pkgvcldir

	compiler:     $CC
	cflags:       $CFLAGS
	ldflags:      $LDFLAGS
")
