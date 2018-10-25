# Build libmemory_interface.so
lib_LTLIBRARIES = libmemory_interface.la

libmemory_interface_la_SOURCES = qmmf_memory_interface.cc
if TARGET_USES_GRALLOC1
libmemory_interface_la_SOURCES += qmmf_gralloc1_interface.cc
else
if TARGET_USES_GRALLOC2
libmemory_interface_la_SOURCES += qmmf_gralloc2_interface.cc
else
if TARGET_USES_GBM
libmemory_interface_la_SOURCES += qmmf_gbm_interface.cc
else
libmemory_interface_la_SOURCES += qmmf_gralloc_interface.cc
endif
endif
endif

libmemory_interface_la_CPPFLAGS = -I$(top_srcdir)/include
libmemory_interface_la_CPPFLAGS += -I$(top_srcdir)
libmemory_interface_la_CPPFLAGS += -I$(top_srcdir)/common/utils
libmemory_interface_la_CPPFLAGS += -I$(top_srcdir)
libmemory_interface_la_LDFLAGS  = -lutils -lhardware
if TARGET_USES_GBM
libmemory_interface_la_LDFLAGS  += -lgbm
endif
