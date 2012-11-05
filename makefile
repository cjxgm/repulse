
CC = gcc
CFLAGS = -pthread -lm -Wall #-Werror -Ofast

all: st
st: st.c
clean:
	rm -f st
rebuild: clean all
debug: all
	sudo ./st

