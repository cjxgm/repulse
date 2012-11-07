
CC = gcc
CFLAGS = -pthread -lm -Wall #-Werror -Ofast

all: st
st: st.c
clean:
	rm -f st
cleanall: clean
	rm -f music.stlm
rebuild: clean all
debug: all
	sudo ./st

