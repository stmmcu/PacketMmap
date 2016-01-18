CPP_OPTS   = -Wall -O2
GEN_SRC    = ./rawsocket.c
LIBS       = -lpthread

default: main
main: transmission_capture.c
	gcc -g ${CPP_OPTS} -o bin transmission_capture.c ${GEN_SRC} ${LIBS}
clean:
	$(RM) -r bin
