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
						  src/vs4/applyrff4.cpp \
						  src/vs4/applyrff4.hpp \
						  src/vs4/d2vsource4.cpp \
						  src/vs4/d2vsource4.hpp \
						  src/vs4/directrender4.cpp \
						  src/vs4/directrender4.hpp \
						  src/vs4/vapoursynth4.cpp

libd2vsource_la_LDFLAGS = -no-undefined -avoid-version $(PLUGINLDFLAGS)

libd2vsource_la_LIBADD = $(libavcodec_LIBS) $(libavformat_LIBS) $(libavutil_LIBS)
