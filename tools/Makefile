

CC = gcc

THIRD_LIB = glib-2.0 libvirt
CFLAGS = $(shell pkg-config --cflags $(THIRD_LIB))
LDFLAGS = $(shell pkg-config --libs $(THIRD_LIB))

all: qemu_connect

XBOX = xargparse.o xstring.o

qemu_connect: qemu_connect.o $(XBOX)
	$(CC) $^ $(LDFLAGS) -o $@


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o qemu_connect