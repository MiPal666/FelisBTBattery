PLUGIN := felisbtbattery.so
SRC := src/felisbtbattery.c

CC ?= gcc
PKG_CFLAGS := $(shell pkg-config --cflags ofono glib-2.0 dbus-1)
PKG_LIBS := $(shell pkg-config --libs glib-2.0 dbus-1)
PLUGIN_DIR ?= $(shell pkg-config --variable=plugindir ofono)

CFLAGS ?= -O2 -g
CFLAGS += -fPIC -Wall -Wextra -Wformat=2 -Wshadow
CPPFLAGS += $(PKG_CFLAGS)
LDFLAGS += -shared
LDLIBS += $(PKG_LIBS)

all: $(PLUGIN)

$(PLUGIN): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: $(PLUGIN)
	install -d $(DESTDIR)$(PLUGIN_DIR)
	install -m 0755 $(PLUGIN) $(DESTDIR)$(PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(PLUGIN)

.PHONY: all install clean
