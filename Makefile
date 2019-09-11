LDFLAGS = -lX11 -L/usr/X11R6/lib
CFLAGS = -Wall

all: xspy

clean:
	rm -f *.o xspy

.PHONY: all clean

