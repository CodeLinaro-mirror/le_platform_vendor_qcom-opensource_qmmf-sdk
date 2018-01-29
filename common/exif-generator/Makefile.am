# Build libexif_generator.so
lib_LTLIBRARIES = libexif_generator.la

libexif_generator_la_SOURCES = qmmf_exif_generator.cc

libexif_generator_la_CPPFLAGS = -I$(top_srcdir)/include
libexif_generator_la_CPPFLAGS += -I$(top_srcdir)/
libexif_generator_la_LDFLAGS  = -lutils -lcutils -lpthread -ldl -llog
