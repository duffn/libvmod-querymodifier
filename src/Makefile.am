AM_CFLAGS = $(VARNISHAPI_CFLAGS)

# Modules

vmod_LTLIBRARIES = \
	libvmod_querymodifier.la

libvmod_querymodifier_la_LDFLAGS = $(VMOD_LDFLAGS)
libvmod_querymodifier_la_SOURCES = vmod_querymodifier.c
nodist_libvmod_querymodifier_la_SOURCES = \
	vcc_querymodifier_if.c \
	vcc_querymodifier_if.h

@BUILD_VMOD_QUERYMODIFIER@

# Test suite

AM_TESTS_ENVIRONMENT = \
	PATH="$(abs_builddir):$(VARNISH_TEST_PATH):$(PATH)" \
	LD_LIBRARY_PATH="$(VARNISH_LIBRARY_PATH)"
TEST_EXTENSIONS = .vtc
VTC_LOG_COMPILER = varnishtest -v
AM_VTC_LOG_FLAGS = \
	-p vcl_path="$(abs_top_srcdir)/vcl:$(VARNISHAPI_VCLDIR)" \
	-p vmod_path="$(abs_builddir)/.libs:$(vmoddir):$(VARNISHAPI_VMODDIR)"

TESTS = $(VMOD_TESTS)

# Documentation

dist_doc_DATA = \
	vmod_querymodifier.vcc \
	$(TESTS)

dist_man_MANS = \
	vmod_querymodifier.3


.rst.1:
	$(AM_V_GEN) $(RST2MAN) $< $@
