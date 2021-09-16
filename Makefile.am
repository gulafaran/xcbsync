EXTRA_DIST = BUGS COPYING autogen.sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = unagi.pc

SUBDIRS = include src rendering plugins

dist-hook: ChangeLog

ChangeLog:
	git log > $(top_srcdir)/ChangeLog || \
		echo 'Could not generate ChangeLog, git directory not available?'

MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog
