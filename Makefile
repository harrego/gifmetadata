TARGET=gifmetadata
CC=gcc
VERSION=v0.0.1
#SOURCE=.gitignore Makefile main.c README.md
CFLAGS=-Wall
#CFLAGS=-fsanitize=address -Wall
LIBS=-lm

OBJS = main.o cli.o gif.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LIBS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o *.tar.gz gifmetadata

# mac_x86_64: $(OBJS)
#	mkdir -p build/mac_x86_64
#	$(CC) $(OBJS) $(CFLAGS) $(LIBS) -o build/mac_x86_64/$(TARGET) -target x86_64-apple-macos10.4
	
# mac_arm: main.c
#	mkdir -p build/mac_arm
#	$(CC) main.c -o build/mac_arm/$(TARGET) -target arm64-apple-macos11
	
# mac_universal: mac_x86_64 mac_arm
#	mkdir -p build/mac_universal
#	lipo -create -output build/mac_universal/$(TARGET) build/mac_x86_64/$(TARGET) build/mac_arm/$(TARGET)
	
# host_64: main.c
#	mkdir -p build/host_64
#	$(CC) main.c -o build/host_64/$(TARGET) -m64
	
# host_32: main.c
#	mkdir -p build/host_32
#	$(CC) main.c -o build/host_32/$(TARGET) -m32

# source_tar: $(SOURCE)
#	tar -czvf $(TARGET)_$(VERSION)_source.tar.gz $(SOURCE)
