NAME		 = slog
VERSION		 = 1.1.0

PREFIX		 = /usr/local
MANPREFIX	 = ${PREFIX}/man

BIN		 = slog
SRC		 = slog.c
MAN1		 = slog.1

OBJ		 = ${SRC:.c=.o}

CFLAGS		+= `pkg-config --cflags lowdown`
LDFLAGS		+= `pkg-config --libs lowdown`

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<

all: ${BIN}

clean:
	rm -f ${OBJ} ${BIN} ${NAME}-${VERSION}.tar.gz

dist:
	mkdir -p ${NAME}-${VERSION}
	cp -f ${SRC} ${MAN1} CHANGES LICENSE.md Makefile README.md ${NAME}-${VERSION}
	cp -rf example ${NAME}-${VERSION}
	tar czf ${NAME}-${VERSION}.tar.gz ${NAME}-${VERSION}
	rm -rf ${NAME}-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/man/man1
	for f in ${BIN}; do install -m 0555 $$f ${DESTDIR}${PREFIX}/bin; done
	for f in ${MAN1}; do install -m 0444 $$f ${DESTDIR}${PREFIX}/man/man1; done

uninstall:
	for f in ${BIN}; do rm -f ${DESTDIR}${PREFIX}/bin/$$f; done
	for f in ${MAN1}; do rm -f ${DESTDIR}${PREFIX}/man/man1/$$f; done

slog: slog.o
	${CC} -o $@ slog.o ${LDFLAGS}

.PHONY: all clean dist install uninstall
