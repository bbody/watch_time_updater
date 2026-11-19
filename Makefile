# watch_time_updater — build with arduino-cli
# https://arduino.github.io/arduino-cli/
#
# Usage:
#   make deps          # once: ESP32 core + libraries
#   make verify        # compile (CI / sanity check)
#   make compile       # same as verify
#   make upload        # compile and flash (set PORT if needed)
#   make monitor       # serial monitor after upload
#   make uploadfs      # write board data (time zone rules + ZoneDetect map) to flash
#   make test          # host unit tests (no board needed)
#   make tzdb          # force a fresh download of the time zone rules
#   make zonedb        # build ZoneDetect's timezone16.bin (GPS position → zone, country)
#   make distclean     # clean + delete cached downloads (~90 MB)
#
# Examples:
#   make upload PORT=/dev/cu.usbmodem101
#   make upload FQBN=esp32:esp32:esp32wrover

ARDUINO_CLI ?= arduino-cli

# Keep libraries inside the project (./.arduino/libraries) instead of the
# shared ~/Documents/Arduino/libraries. The ESP32 core stays in ~/Library/Arduino15.
export ARDUINO_DIRECTORIES_USER ?= $(CURDIR)/.arduino
SKETCH      := .
BUILD_DIR   := build
# arduino-cli gets its own folder: it may delete files it does not recognise
# in its build path, which would take the data outputs below with it.
ARDUINO_BUILD_DIR := $(BUILD_DIR)/arduino
PARTITIONS_CSV := $(ARDUINO_BUILD_DIR)/partitions.csv
FQBN        ?= esp32:esp32:esp32wrover

# Auto-pick the first USB serial port, e.g. /dev/cu.usbserial-0001
# (skips Bluetooth/debug ports; override with make upload PORT=...)
PORT        ?= $(shell $(ARDUINO_CLI) board list 2>/dev/null | awk 'NR>1 && $$1 ~ /usbserial|usbmodem|ttyUSB|ttyACM/ {print $$1; exit}')

MONITOR_BAUD ?= 115200

ESP32_INDEX_URL := https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Each name is quoted: library names contain spaces.
LIBRARIES := \
	"Adafruit BusIO" \
	"Adafruit GFX Library" \
	"Adafruit GC9A01A"

# Time zone table source (pinned commit; override with TZDB_REV=master)
TZDB_REV    ?= fb5fa340cfa7599467358a347e5d6e6724d92bb2
TZDB_BASE   := https://raw.githubusercontent.com/nayarsystems/posix_tz_db/$(TZDB_REV)
# Downloads are cached per revision, so changing TZDB_REV fetches again.
TZDB_CACHE  := $(BUILD_DIR)/tzdb/$(TZDB_REV)

# Large downloads are cached outside build/ so `make clean` keeps them.
CACHE_DIR   := .cache

# ZoneDetect database (out_v1/timezone16.bin), built from source data with
# ZoneDetect's own builder: https://github.com/BertoldVdb/ZoneDetect
ZD_REV      ?= 81ede727f8771eda507150a95237cc8c518666d6
ZD_RAW      := https://raw.githubusercontent.com/BertoldVdb/ZoneDetect/$(ZD_REV)
ZD_SRC      := $(CACHE_DIR)/zonedetect-$(ZD_REV)
SHAPELIB_VERSION ?= 1.6.3
SHAPELIB_TGZ := $(CACHE_DIR)/shapelib-$(SHAPELIB_VERSION).tar.gz
TZBB_RELEASE ?= 2026a
TZBB_ZIP    := $(CACHE_DIR)/timezones-with-oceans-$(TZBB_RELEASE).shapefile.zip
NE_ZIP      := $(CACHE_DIR)/ne_10m_admin_0_countries_lakes.zip
ZD_DIR      := $(BUILD_DIR)/zonedetect
ZD_BUILDER  := $(ZD_DIR)/builder
ZONE_DB     := $(ZD_DIR)/out_v1/timezone16.bin
ZD_LABEL    := zonedetect
ZD_NOTICE   := Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL).
HOST_CC     ?= cc

# LittleFS image, written to the partition labelled "spiffs".
# Files for the board's file system are assembled here at build time;
# nothing under it is source code.
FS_DIR      := $(BUILD_DIR)/fs
TZ_CSV      := $(FS_DIR)/tz/zones.csv
FS_IMAGE    := $(BUILD_DIR)/littlefs.bin
FS_LABEL    := spiffs
UPLOAD_BAUD ?= 921600
ARDUINO_DATA = $(shell $(ARDUINO_CLI) config get directories.data 2>/dev/null)
MKLITTLEFS  ?= $(lastword $(sort $(wildcard $(ARDUINO_DATA)/packages/esp32/tools/mklittlefs/*/mklittlefs)))
ESPTOOL     ?= $(lastword $(sort $(wildcard $(ARDUINO_DATA)/packages/esp32/tools/esptool_py/*/esptool)))

HOST_CXX    ?= c++
TEST_DIR    := $(BUILD_DIR)/test

.PHONY: all verify compile upload monitor clean distclean deps install-board install-libs help test tzdb zonedb fs-image uploadfs

all: compile

help:
	@echo "Targets: deps install-board install-libs verify compile upload uploadfs monitor test tzdb zonedb clean distclean"
	@echo "FQBN=$(FQBN)"
	@echo "PORT=$(PORT)"

install-board:
	$(ARDUINO_CLI) core update-index --additional-urls "$(ESP32_INDEX_URL)"
	$(ARDUINO_CLI) core install esp32:esp32 --additional-urls "$(ESP32_INDEX_URL)"

# Installs each library on its own so one failure doesn't block the rest.
# A library whose folder already exists is skipped, even if arduino-cli
# didn't install it (e.g. copied in by hand).
install-libs:
	$(ARDUINO_CLI) lib update-index
	@dir="$(ARDUINO_DIRECTORIES_USER)"; \
	status=0; \
	for lib in $(LIBRARIES); do \
		folder="$$dir/libraries/$$(printf '%s' "$$lib" | tr ' ' '_')"; \
		if [ -d "$$folder" ]; then \
			echo "$$lib: already present ($$folder), skipping"; \
		elif ! $(ARDUINO_CLI) lib install "$$lib"; then \
			status=1; \
		fi; \
	done; \
	exit $$status

deps: install-board install-libs

verify compile:
	$(ARDUINO_CLI) compile \
		--build-path "$(ARDUINO_BUILD_DIR)" \
		--fqbn "$(FQBN)" \
		"$(SKETCH)"

upload: compile
	@if [ -z "$(PORT)" ]; then \
		echo "No serial port found. Run '$(ARDUINO_CLI) board list' and retry with PORT=..."; \
		exit 1; \
	fi
	$(ARDUINO_CLI) upload \
		--build-path "$(ARDUINO_BUILD_DIR)" \
		-p "$(PORT)" \
		--fqbn "$(FQBN)" \
		"$(SKETCH)"

monitor:
	@if [ -z "$(PORT)" ]; then \
		echo "Set PORT= for monitor"; \
		exit 1; \
	fi
	$(ARDUINO_CLI) monitor -p "$(PORT)" -c baudrate=$(MONITOR_BAUD)

TEST_BIN := $(TEST_DIR)/test_timezones
ZONE_TEST_BIN := $(TEST_DIR)/test_zonelookup

$(TEST_BIN): test/test_timezones.cpp src/time/TimeZones.cpp src/time/TimeZones.h
	@mkdir -p "$(TEST_DIR)"
	$(HOST_CXX) -std=c++17 -Wall -Wextra -Werror \
		test/test_timezones.cpp src/time/TimeZones.cpp -o "$@"

$(ZONE_TEST_BIN): test/test_zonelookup.cpp src/time/ZoneLookup.cpp src/time/ZoneLookup.h \
		src/time/TimeZones.cpp src/time/TimeZones.h src/third_party/zonedetect/zonedetect.c
	@mkdir -p "$(TEST_DIR)"
	$(HOST_CC) -O2 -w -c src/third_party/zonedetect/zonedetect.c -o "$(TEST_DIR)/zonedetect.o"
	$(HOST_CXX) -std=c++17 -Wall -Wextra -Werror \
		test/test_zonelookup.cpp src/time/ZoneLookup.cpp src/time/TimeZones.cpp \
		"$(TEST_DIR)/zonedetect.o" -o "$@"

test: $(TEST_BIN) $(ZONE_TEST_BIN) $(TZ_CSV) $(ZONE_DB)
	"$(TEST_BIN)" "$(TZ_CSV)"
	"$(ZONE_TEST_BIN)" "$(ZONE_DB)" "$(TZ_CSV)"

# Download posix_tz_db's zones.csv (+ its licence) for TZDB_REV. The file is
# only kept if it passes the parser tests.
$(TZDB_CACHE)/zones.csv: | $(TEST_BIN)
	@mkdir -p "$(TZDB_CACHE)"
	curl -fsSL -o "$@.tmp" "$(TZDB_BASE)/zones.csv"
	curl -fsSL -o "$(TZDB_CACHE)/LICENSE" "$(TZDB_BASE)/LICENSE"
	"$(TEST_BIN)" "$@.tmp"
	mv "$@.tmp" "$@"

$(TZ_CSV): $(TZDB_CACHE)/zones.csv
	@mkdir -p "$(dir $@)"
	cp "$(TZDB_CACHE)/zones.csv" "$@"
	cp "$(TZDB_CACHE)/LICENSE" "$(dir $@)LICENSE"

tzdb:
	rm -rf "$(TZDB_CACHE)" "$(FS_DIR)/tz"
	$(MAKE) "$(TZ_CSV)"

# --- ZoneDetect database -------------------------------------------------
# Downloads (cached in .cache/), then the same steps as ZoneDetect's
# database/builder/makedb.sh, for out_v1/timezone16.bin only.

$(ZD_SRC)/builder.cpp $(ZD_SRC)/zoneToAlpha.h:
	@mkdir -p "$(ZD_SRC)"
	curl -fsSL -o "$@.tmp" "$(ZD_RAW)/database/builder/$(notdir $@)"
	mv "$@.tmp" "$@"

$(SHAPELIB_TGZ):
	@mkdir -p "$(CACHE_DIR)"
	curl -fsSL -o "$@.tmp" "https://github.com/OSGeo/shapelib/releases/download/v$(SHAPELIB_VERSION)/shapelib-$(SHAPELIB_VERSION).tar.gz"
	mv "$@.tmp" "$@"

$(NE_ZIP):
	@mkdir -p "$(CACHE_DIR)"
	curl -fSL -o "$@.tmp" "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_0_countries_lakes.zip"
	mv "$@.tmp" "$@"

$(TZBB_ZIP):
	@mkdir -p "$(CACHE_DIR)"
	curl -fSL -o "$@.tmp" "https://github.com/evansiroky/timezone-boundary-builder/releases/download/$(TZBB_RELEASE)/timezones-with-oceans.shapefile.zip"
	mv "$@.tmp" "$@"

# ZoneDetect's builder, linked against shapelib built from source (only the
# three files it needs), so nothing has to be installed system-wide.
$(ZD_BUILDER): $(ZD_SRC)/builder.cpp $(ZD_SRC)/zoneToAlpha.h $(SHAPELIB_TGZ)
	rm -rf "$(ZD_DIR)/shapelib"
	@mkdir -p "$(ZD_DIR)/shapelib"
	tar -xzf "$(SHAPELIB_TGZ)" -C "$(ZD_DIR)/shapelib" --strip-components=1
	for f in shpopen dbfopen safileio; do \
		$(HOST_CC) -O2 -w -I "$(ZD_DIR)/shapelib" -c "$(ZD_DIR)/shapelib/$$f.c" -o "$(ZD_DIR)/shapelib/$$f.o" || exit 1; \
	done
	$(HOST_CXX) -std=c++11 -O2 -w -I "$(ZD_DIR)/shapelib" "$(ZD_SRC)/builder.cpp" \
		"$(ZD_DIR)/shapelib/shpopen.o" "$(ZD_DIR)/shapelib/dbfopen.o" "$(ZD_DIR)/shapelib/safileio.o" \
		-o "$@"

# The builder reads naturalearth/ relative to its working directory.
$(ZONE_DB): $(ZD_BUILDER) $(NE_ZIP) $(TZBB_ZIP)
	rm -rf "$(ZD_DIR)/naturalearth" "$(ZD_DIR)/timezone"
	@mkdir -p "$(ZD_DIR)/naturalearth" "$(ZD_DIR)/timezone" "$(ZD_DIR)/out_v1"
	unzip -oq "$(NE_ZIP)" -d "$(ZD_DIR)/naturalearth"
	unzip -oq "$(TZBB_ZIP)" -d "$(ZD_DIR)/timezone"
	cd "$(ZD_DIR)" && ./builder T timezone/combined-shapefile-with-oceans \
		out_v1/timezone16.bin.tmp 16 "$(ZD_NOTICE)" 1 > builder.log 2>&1 \
		|| { cat builder.log; exit 1; }
	mv "$@.tmp" "$@"
	rm -rf "$(ZD_DIR)/naturalearth" "$(ZD_DIR)/timezone"
	@echo "Built $@ ($$(wc -c < "$@" | tr -d ' ') bytes, timezone-boundary-builder $(TZBB_RELEASE)); log: $(ZD_DIR)/builder.log"

zonedb: $(ZONE_DB)

# --- Board data upload ---------------------------------------------------

# partitions.csv in the build dir is produced by a compile; it gives the data
# partitions' offsets and sizes.
$(PARTITIONS_CSV): partitions.csv
	$(MAKE) compile

# Print "<offset> <size>" of a partition from the built partition table.
partition_info = awk -F, '{gsub(/[ \t]/, "")} $$1 == "$(1)" {print $$4, $$5}' "$(PARTITIONS_CSV)"

fs-image: $(PARTITIONS_CSV) $(TZ_CSV)
	@if [ ! -x "$(MKLITTLEFS)" ]; then echo "mklittlefs not found (run 'make deps')"; exit 1; fi
	@set -e; \
	set -- $$($(call partition_info,$(FS_LABEL))); \
	if [ -z "$$2" ]; then echo "No '$(FS_LABEL)' partition in $(PARTITIONS_CSV)"; exit 1; fi; \
	echo "LittleFS image: $(FS_DIR)/ → $(FS_IMAGE) ($$2 bytes)"; \
	"$(MKLITTLEFS)" -c "$(FS_DIR)" -b 4096 -p 256 -s $$(($$2)) "$(FS_IMAGE)"

uploadfs: fs-image $(ZONE_DB)
	@if [ -z "$(PORT)" ]; then \
		echo "No serial port found. Run '$(ARDUINO_CLI) board list' and retry with PORT=..."; \
		exit 1; \
	fi
	@if [ ! -x "$(ESPTOOL)" ]; then echo "esptool not found (run 'make deps')"; exit 1; fi
	@set -e; \
	set -- $$($(call partition_info,$(FS_LABEL))); fs_offset=$$1; \
	set -- $$($(call partition_info,$(ZD_LABEL))); zd_offset=$$1; zd_size=$$2; \
	if [ -z "$$zd_offset" ]; then echo "No '$(ZD_LABEL)' partition in $(PARTITIONS_CSV)"; exit 1; fi; \
	db_size=$$(wc -c < "$(ZONE_DB)" | tr -d ' '); \
	if [ $$db_size -gt $$(($$zd_size)) ]; then \
		echo "$(ZONE_DB) ($$db_size bytes) doesn't fit the '$(ZD_LABEL)' partition ($$(($$zd_size)) bytes)"; exit 1; \
	fi; \
	"$(ESPTOOL)" --chip esp32 --port "$(PORT)" --baud $(UPLOAD_BAUD) \
		write-flash $$fs_offset "$(FS_IMAGE)" $$zd_offset "$(ZONE_DB)"

clean:
	rm -rf "$(BUILD_DIR)"

distclean: clean
	rm -rf "$(CACHE_DIR)"
