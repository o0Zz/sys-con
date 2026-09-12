# version.h is a real generated file, not a phony target. It used to be listed in TARGETS,
# which .PHONY then marked as always-out-of-date, so it was regenerated on every build
# regardless of the .git/HEAD dependency below. The app target depends on it explicitly.
VERSION_HEADER := app/version.h

TARGETS := companion app
TOPTARGETS := all clean mrproper

ATMOSPHERE_GIT_HASH := $(shell git -C ../lib/Atmosphere-libs rev-parse --short HEAD)
SYSCON_GIT_HASH := $(shell git rev-parse --short HEAD)
SYSCON_GIT_TAG := $(shell git describe --tags `git rev-list --tags --max-count=1`)
SYSCON_GIT_TAG_COMMIT_COUNT := $(shell git rev-list  `git rev-list --tags --no-walk --max-count=1`..HEAD --count)
DEBUG := 0

.PHONY: $(TOPTARGETS) $(TARGETS)

all: $(VERSION_HEADER) $(TARGETS)

libstratosphere:
ifeq ($(ATMOSPHERE),1) 
ifeq ($(ATMOSPHERE_BUILD_ENABLED),1) 
	$(MAKE) -C ../lib/Atmosphere-libs/libstratosphere -j8 ATMOSPHERE_BUILD_FOR_DEBUGGING=$(DEBUG)
endif
endif

libhiddatainterpreter:
	$(MAKE) -C ../lib/HIDDataInterpreter/build/switch

companion:
	$(MAKE) -C companion

$(VERSION_HEADER): ../.git/HEAD ../.git/index
	@echo "Generating version.h..."
	@echo "#pragma once" > $@.tmp
	@echo "namespace syscon::version" >> $@.tmp
	@echo "{" >> $@.tmp
	@echo "const char * syscon_tag = \"$(SYSCON_GIT_TAG)\";" >> $@.tmp
	@echo "const char * syscon_git_hash = \"$(SYSCON_GIT_HASH)\";" >> $@.tmp
	@echo "const unsigned int syscon_commit_count = $(SYSCON_GIT_TAG_COMMIT_COUNT);" >> $@.tmp
	@echo "const char * atmosphere_version = \"$(ATMOSPHERE_VERSION)-$(ATMOSPHERE_GIT_HASH)\";" >> $@.tmp
	@echo "}" >> $@.tmp
	@if [ -f $@ ] && cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

app: libstratosphere libhiddatainterpreter $(VERSION_HEADER)
	$(MAKE) -C app DEBUG=$(DEBUG) ATMOSPHERE_BUILD_FOR_DEBUGGING=$(DEBUG)

clean:
	$(MAKE) -C companion clean
	$(MAKE) -C app clean
	
mrproper: clean
ifeq ($(ATMOSPHERE),1)
	$(MAKE) -C ../lib/Atmosphere-libs/libstratosphere clean
endif
	$(MAKE) -C ../lib/HIDDataInterpreter/build/switch clean