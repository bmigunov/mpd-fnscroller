CC = gcc
LDFLAGS = -lpthread -lmpdclient
SRC = main.c runtime.c server.c client.c
CFLAGS = -Wall -Werror -fpic




all: $(EXECUTABLE)

$(EXECUTABLE): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(EXECUTABLE)
