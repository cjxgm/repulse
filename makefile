
CC = gcc
CFLAGS = -pthread -lm -Wall #-Werror -Ofast

all: repulse
st: repulse.c
clean:
	rm -f repulse
cleanall: clean
	rm -f music.rpulmod
rebuild: clean all
debug: all
	sudo ./repulse

