TARGETS := AppletCompanion Sysmodule/source/version.h Sysmodule
TOPTARGETS := all clean mrproper

SYSCON_GIT_HASH := $(shell git rev-parse --short HEAD)
SYSCON_GIT_TAG := $(shell git describe --tags `git rev-list --tags --max-count=1`)
SYSCON_GIT_TAG_COMMIT_COUNT := $(shell git rev-list  `git rev-list --tags --no-walk --max-count=1`..HEAD --count)

.PHONY: $(TOPTARGETS) $(TARGETS)

all: $(TARGETS)

libhiddatainterpreter:
	$(MAKE) -C ../lib/HIDDataInterpreter/build/switch

AppletCompanion:
	$(MAKE) -C AppletCompanion

Sysmodule/source/version.h: ../.git/HEAD ../.git/index
	echo "#pragma once" > $@
	echo "namespace syscon::version" >> $@
	echo "{" >> $@
	echo "const char * syscon_tag = \"$(SYSCON_GIT_TAG)\";" >> $@
	echo "const char * syscon_git_hash = \"$(SYSCON_GIT_HASH)\";" >> $@
	echo "const unsigned int syscon_commit_count = $(SYSCON_GIT_TAG_COMMIT_COUNT);" >> $@
	echo "}" >> $@

Sysmodule: libhiddatainterpreter Sysmodule/source/version.h
	$(MAKE) -C Sysmodule

clean:
	$(MAKE) -C AppletCompanion clean
	$(MAKE) -C Sysmodule clean
	
mrproper: clean
	$(MAKE) -C ../lib/HIDDataInterpreter/build/switch clean