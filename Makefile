.PHONY: all clean

CFLAGS	+= -W -Wall -Wextra -Wpedantic
CFLAGS	+= `pkg-config --cflags lowdown`
LDFLAGS	+= `pkg-config --libs lowdown`

SRC	 = slog.c
BIN	 = slog

OBJ	 = ${SRC:.c=.o}

all: ${BIN}

clean:
	rm -f ${BIN} ${OBJ}

.o:
	${CC} ${LDFLAGS} -o $@

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<

slog: slog.o
	${CC} -o $@ slog.o ${LDFLAGS}
