# AI Recommendations plugin
libai_recommendations_plugin_la_SOURCES = \
	ai_recommendations.c \
	profile_manager.c \
	feature_extractor.c \
	recommendation_engine.c \
	metadata_analyzer.c

# Headers that should be distributed  
libai_recommendations_plugin_la_SOURCES += \
	ai_recommendations.h \
	profile_manager.h \
	feature_extractor.h \
	recommendation_engine.h \
	metadata_analyzer.h \
	custom_compat.h

# C/C++ compilation flags
libai_recommendations_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/include
libai_recommendations_plugin_la_CFLAGS = $(AM_CFLAGS)
libai_recommendations_plugin_la_CXXFLAGS = $(AM_CXXFLAGS)

# Linking
libai_recommendations_plugin_la_LIBADD = $(LIBM)
libai_recommendations_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)' \
	-avoid-version -module

# Add to misc libraries
misc_LTLIBRARIES = libai_recommendations_plugin.la