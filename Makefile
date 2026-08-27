# Makefile per MiniHTTPserver
# Usa: make all
# oppure: make linux
# oppure: make windows (con MySQL)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

SERVER_SRC = HTTPserver.c HTMLmanagement.c
TARGET = server

# Percorsi MySQL su Windows
MYSQL_WIN_INC = C:/Program Files/MySQL/MySQL Server 8.0/include
MYSQL_WIN_LIB_DIR = C:/Program Files/MySQL/MySQL Server 8.0/lib
# Usiamo direttamente il percorso del file .lib per evitare problemi con -lmysql in MinGW
MYSQL_WIN_LIB = "$(MYSQL_WIN_LIB_DIR)/libmysql.lib"

# --- Linux ---
LINUX_CFLAGS = $(CFLAGS) -I/usr/include/mysql
LINUX_LIBS = -lpthread -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lssl -lcrypto

# --- Windows / MinGW ---
WINDOWS_CFLAGS = $(CFLAGS) -D_WIN32 -I"$(MYSQL_WIN_INC)" 
WINDOWS_LIBS = -lws2_32 $(MYSQL_WIN_LIB) -lssl -lcrypto

# Target default
all: windows

linux:
	$(CC) $(LINUX_CFLAGS) $(SERVER_SRC) -o $(TARGET) $(LINUX_LIBS)

windows:
	$(CC) $(WINDOWS_CFLAGS) $(SERVER_SRC) -o $(TARGET).exe $(WINDOWS_LIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all linux windows clean