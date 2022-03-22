# Configuration

CC		= gcc
LD		= gcc
AR		= ar
CFLAGS		= -g -std=gnu99 -Wall -Iinclude -fPIC
LDFLAGS		= -Llib -pthread
ARFLAGS		= rcs

# Variables

CLIENT_HEADERS  = $(wildcard include/mq/*.h)
CLIENT_SOURCES  = $(wildcard src/*.c)
CLIENT_OBJECTS  = $(CLIENT_SOURCES:.c=.o)
CLIENT_LIBRARY  = lib/libmq_client.a

# Rules

all:	$(CLIENT_LIBRARY)

%.o:			%.c $(CLIENT_HEADERS)
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(CLIENT_LIBRARY):	$(CLIENT_OBJECTS)
	@echo "Linking   $@"
	@$(AR) $(ARFLAGS) $@ $^

application: src/shell.o $(CLIENT_LIBRARY)
	@echo "Making chat application"
	@$(LD) $(LDFLAGS) -o $@ $^

clean:
	@echo "Removing  objects"
	@rm -f $(CLIENT_OBJECTS)

	@echo "Removing  libraries"
	@rm -f $(CLIENT_LIBRARY)

.PRECIOUS: %.o
