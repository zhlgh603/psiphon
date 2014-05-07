DESTDIR =
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

GOBUILDFLAGS =

all: meek-relay

meek-relay: *.go
	go build $(GOBUILDFLAGS)

install: meek-relay
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -f meek-relay "$(DESTDIR)$(BINDIR)"

clean:
	rm -f meek-relay

fmt:
	go fmt

.PHONY: all install clean fmt
