NULL =

if ENABLE_SKEY
SUBDIRS = skey
endif

bin_PROGRAMS = cafe-terminal

BUILT_SOURCES = \
	terminal-marshal.c \
	terminal-marshal.h \
	terminal-resources.c \
	terminal-resources.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)

cafe_terminal_SOURCES= \
	cafevaluearray.c \
	cafevaluearray.h \
	eggshell.c \
	eggshell.h \
	profile-editor.c \
	profile-editor.h \
	terminal.c \
	terminal-accels.c \
	terminal-accels.h \
	terminal-app.c \
	terminal-app.h \
	terminal-close-button.h \
	terminal-close-button.c \
	terminal-debug.c \
	terminal-debug.h \
	terminal-encoding.c \
	terminal-encoding.h \
	terminal-info-bar.c \
	terminal-info-bar.h \
	terminal-intl.h \
	terminal-options.c \
	terminal-options.h \
	terminal-profile.c \
	terminal-profile.h \
	terminal-screen.c \
	terminal-screen.h \
	terminal-screen-container.c \
	terminal-screen-container.h \
	terminal-search-dialog.c \
	terminal-search-dialog.h \
	terminal-tab-label.c \
	terminal-tab-label.h \
	terminal-tabs-menu.c \
	terminal-tabs-menu.h \
	terminal-util.c \
	terminal-util.h \
	terminal-version.h \
	terminal-window.c \
	terminal-window.h \
	$(NULL)

if ENABLE_SKEY
cafe_terminal_SOURCES += \
	skey-popup.c \
	skey-popup.h \
	$(NULL)
endif

nodist_cafe_terminal_SOURCES= $(BUILT_SOURCES)

cafe_terminal_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERMINAL_RESOURCES_PATH_PREFIX="\"/org/cafe/terminal\"" \
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DTERM_HELPDIR="\"$(HELP_DIR)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DCDK_MULTIHEAD_SAFE \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DPANGO_DISABLE_SINGLE_INCLUDES \
	-DATK_DISABLE_SINGLE_INCLUDES \
	-DCDK_DISABLE_SINGLE_INCLUDES \
	-DCDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DCTK_DISABLE_SINGLE_INCLUDES \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

cafe_terminal_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

cafe_terminal_LDFLAGS = -lICE

cafe_terminal_LDADD = \
	$(TERM_LIBS)

if ENABLE_SKEY
cafe_terminal_LDADD += \
	skey/libskey.la
endif

cafe_terminal_SOURCES += \
	eggdesktopfile.c \
	eggdesktopfile.h \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	eggsmclient-xsmp.c \
	$(NULL)

cafe_terminal_CFLAGS += $(SMCLIENT_CFLAGS)
cafe_terminal_CPPFLAGS += -DEGG_SM_CLIENT_BACKEND_XSMP
cafe_terminal_LDADD += $(SMCLIENT_LIBS)

TYPES_H_FILES = \
	terminal-profile.h \
	$(NULL)

terminal-type-builtins.h: stamp-terminal-type-builtins.h
	@true

stamp-terminal-type-builtins.h: terminal-type-builtins.h.template $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbh \
	&& (cmp -s xgen-ttbh terminal-type-builtins.h || cp xgen-ttbh terminal-type-builtins.h ) \
	&& rm -f xgen-ttbh \
	&& echo timestamp > $(@F)

terminal-type-builtins.c: terminal-type-builtins.c.template $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbc \
	&& (cmp -s xgen-ttbc terminal-type-builtins.c || cp xgen-ttbc terminal-type-builtins.c ) \
	&& rm -f xgen-ttbc

terminal-marshal.h: $(srcdir)/terminal-marshal.list
	$(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
			--header \
			--internal > terminal-marshal.h.tmp \
	&& mv terminal-marshal.h.tmp terminal-marshal.h ) \
	|| ( rm -f terminal-marshal.h.tmp && exit 1 )

terminal-marshal.c: $(srcdir)/terminal-marshal.list
	$(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
				--body \
				--prototypes \
				--internal > terminal-marshal.c.tmp \
		&& mv terminal-marshal.c.tmp terminal-marshal.c ) \
	|| ( rm -f terminal-marshal.c.tmp && exit 1 )

terminal-resources.h terminal-resources.c: terminal.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/terminal.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name terminal $<

gsettingsschema_in_files = org.cafe.terminal.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

CLEANFILES = \
	stamp-terminal-type-builtins.h \
	cafe-terminal.schemas \
	$(gsettings_SCHEMAS) \
	stamp-terminal-type-builtins.h \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	encodings-dialog.ui \
	find-dialog.ui \
	keybinding-editor.ui \
	profile-manager.ui \
	profile-new-dialog.ui \
	profile-preferences.ui \
	skey-challenge.ui \
	terminal.about \
	terminal.xml \
	terminal.gresource.xml \
	terminal-marshal.list \
	terminal-type-builtins.c.template \
	terminal-type-builtins.h.template \
	extra-strings.c \
	$(about_DATA) \
	$(schema_in_files) \
	$(gsettingsschema_in_files) \
	$(uimanager_DATA) \
	$(builder_DATA) \
	$(NULL)

@GSETTINGS_RULES@

-include $(top_srcdir)/git.mk
