CC=gcc
VERSION=v1.0
ARCH=x86-64
BIN=xcbsync
RENDER=render.so
OPACITY=opacity.so

EXTRA_CFLAGS=-march=$(ARCH) -mtune=native -g
PKGFLAGS=xcb-atom xcb-aux xcb-composite xcb-damage xcb-event xcb-ewmh xcb-glx xcb-icccm xcb-image xcb-keysyms xcb xcb-present xcb-proto xcb-randr xcb-render xcb-renderutil xcb-util xcb-xfixes xcb-xinerama xkbcommon xkbcommon-x11
CFLAGS=$(EXTRA_CFLAGS) `pkg-config --cflags $(PKGFLAGS)` $(INCLUDE)
LINKER=-lev `pkg-config --libs $(PKGFLAGS)`
INCLUDE=-Iinclude/


SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEPS = $(wildcard include/*.h)

RENDERSRC= $(wildcard rendering/*.c)
RENDEROBJ= $(RENDERSRC:.c=.o)
OPACITYSRC= $(wildcard plugins/*.c)
OPACITYOBJ = $(OPACITYSRC:.c=.o)

render: xcbsync opacity
	$(CC) $(CFLAGS) $(LINKER) -fpic -c $(RENDERSRC) -o $(RENDEROBJ)
	$(CC) $(CLFAGS) $(LINKER) -shared -o rendering/$(RENDER) $(RENDEROBJ)

opacity:
	$(CC) $(CFLAGS) $(LINKER) -fpic -c $(OPACITYSRC) -o $(OPACITYOBJ) 
	$(CC) $(CFLAGS) $(LINKER) -shared -o plugins/$(OPACITY) $(OPACITYOBJ)

xcbsync: $(OBJ)
	$(CC) $(OBJ) $(LINKER) -rdynamic -o src/$(BIN)

%.o: %.c $(DEPS)
	$(CC) -c $(CFLAGS) $< -o $@

install: render
	install -D -m755 src/$(BIN) $(DESTDIR)/usr/bin/$(BIN)
	install -D -m755 rendering/$(RENDER) $(DESTDIR)/usr/lib/xcbsync/rendering/$(RENDER)
	install -D -m755 plugins/$(OPACITY) $(DESTDIR)/usr/lib/xcbsync/plugins/$(OPACITY)

.PHONY: uninstall
uninstall:

.PHONY: clean
clean:
	rm src/*.o src/$(BIN) rendering/*.o rendering/$(RENDER) plugins/*.o plugins/$(OPACITY)
