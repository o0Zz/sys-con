.PHONY: all build clean mrproper dist distclean print-version

GIT_TAG := $(shell git describe --tags `git rev-list --tags --max-count=1`)
GIT_TAG_COMMIT_COUNT := +$(shell git rev-list  `git rev-list --tags --no-walk --max-count=1`..HEAD --count)
ifeq ($(GIT_TAG_COMMIT_COUNT),+0)
	GIT_TAG_COMMIT_COUNT := 
endif

# The sysmodule's Atmosphere title ID. Also hardcoded, unavoidably, in two files the
# toolchain reads directly: src/app/sys-con.json (the NPDM config, three times)
# and dist/atmosphere/contents/<TITLE_ID>/ (a directory name). Change all three together.
TITLE_ID			:= 690000000000000D

ATMOSPHERE			?= 0
ATMOSPHERE_BUILD_ENABLED ?= 0
ATMOSPHERE_VERSION	?= 1.7.x
SOURCE_DIR			:= src
OUT_DIR				:= out
DIST_DIR			:= dist
OUT_ZIP				:= sys-con-$(GIT_TAG)$(GIT_TAG_COMMIT_COUNT).zip

all: build
	rm -rf $(OUT_DIR)
	mkdir -p $(OUT_DIR)/atmosphere/contents/$(TITLE_ID)/flags
	mkdir -p $(OUT_DIR)/config/sys-con
	mkdir -p $(OUT_DIR)/switch/
	touch $(OUT_DIR)/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag
	cp $(SOURCE_DIR)/app/sys-con.nsp $(OUT_DIR)/atmosphere/contents/$(TITLE_ID)/exefs.nsp
	cp $(SOURCE_DIR)/companion/sys-con.nro $(OUT_DIR)/switch/sys-con.nro
	cp -r $(DIST_DIR)/. $(OUT_DIR)/
	@echo [DONE] sys-con compiled successfully. All files have been placed in $(OUT_DIR)/

build:
	$(MAKE) -C $(SOURCE_DIR) ATMOSPHERE=$(ATMOSPHERE) ATMOSPHERE_BUILD_ENABLED=$(ATMOSPHERE_BUILD_ENABLED)

# Single source of truth for the release version string. The CI workflow reads this rather
# than reimplementing the `git describe` logic above, which it used to duplicate verbatim.
print-version:
	@echo $(GIT_TAG)$(GIT_TAG_COMMIT_COUNT)

clean:
	$(MAKE) -C $(SOURCE_DIR) clean
	rm -rf $(OUT_DIR)
	rm -f $(OUT_ZIP)
	
mrproper: clean
	$(MAKE) -C $(SOURCE_DIR) mrproper

dist: clean all
	cd $(OUT_DIR)/ && zip -r ../$(OUT_ZIP) .
	
# Discards ALL local changes in the Atmosphere-libs submodule working tree.
# Opt-in only: `make distclean RESET_ATMOSPHERE=1`. It used to run unconditionally as part
# of distclean, which silently destroyed any work in progress in that submodule.
atmosphere_$(ATMOSPHERE_VERSION):
ifeq ($(RESET_ATMOSPHERE),1)
	cd lib/Atmosphere-libs && git reset --hard
else
	@echo "[SKIP] Not resetting lib/Atmosphere-libs (pass RESET_ATMOSPHERE=1 to force)"
endif

distclean: mrproper atmosphere_$(ATMOSPHERE_VERSION) all
	cd $(OUT_DIR)/ && zip -r ../$(OUT_ZIP) .
