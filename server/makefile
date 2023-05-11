#------------------------------------------------------------------------------
# makefile for 5.1 assesment
#
# Use: make [PLATFORM] [TARGET]
#
# Build Targets:
#      
#      all - Builds and links all source files to predefined target
#	   default - Builds and links all source files to predefined target
#      clean - removes all generated files
#
#------------------------------------------------------------------------------
SRC ?= aesdsocket.c
TARGET ?= aesdsocket
OBJS := $(SRC:.c=.o)
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt

all:  $(TARGET)

default: all

$(TARGET) : $(TARGET).c
	$(CC) $(CFLAGS) -o $(INCLUDES) $(TARGET) $(SRC) $(LDFLAGS)
	
.PHONY: clean
clean:
	rm -f *.o $(TARGET)
