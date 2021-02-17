CC = gcc
LDFLAGS = -lpthread -lmpdclient
SRC = runtime.c server.c client.c main.c
CFLAGS = -Wall -Werror -fpic




all: $(EXECUTABLE)

$(EXECUTABLE): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(EXECUTABLE)
