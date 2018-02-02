CC=gcc
CPPFLAGS=-g -Wall
USERID=123456789
CLASSES=

all: server

server: $(CLASSES)
	$(CC) -o $@ $^ $(CPPFLAGS) $@.c



clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server  *.tar.gz

dist: tarball

tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
