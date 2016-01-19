warning_flags = -Wall -Wextra -Wno-unused-parameter -Wshadow
common_cflags = -O2 $(warning_flags) -I$(srcdir)/src/core -D__STDC_CONSTANT_MACROS
AM_CXXFLAGS = $(common_cflags)

AM_CPPFLAGS = $(VapourSynth_CFLAGS) $(libavcodec_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS)

lib_LTLIBRARIES = libd2vsource.la

libd2vsource_la_SOURCES = src/core/compat.cpp \
						  src/core/compat.hpp \
						  src/core/d2v.cpp \
						  src/core/d2v.hpp \
						  src/core/decode.cpp \
						  src/core/decode.hpp \
						  src/core/gop.hpp \
						  src/vs/applyrff.cpp \
						  src/vs/applyrff.hpp \
						  src/vs/d2vsource.cpp \
						  src/vs/d2vsource.hpp \
						  src/vs/directrender.cpp \
						  src/vs/directrender.hpp \
						  src/vs/vapoursynth.cpp

libd2vsource_la_LDFLAGS = -no-undefined -avoid-version $(PLUGINLDFLAGS)

libd2vsource_la_LIBADD = $(libavcodec_LIBS) $(libavformat_LIBS) $(libavutil_LIBS)
