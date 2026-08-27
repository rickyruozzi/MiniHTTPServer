# Makefile per MiniHTTPserver

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

SERVER_SRC = HTTPserver.c HTMLmanagement.c
TARGET = server

# --- Percorsi Windows ---
MYSQL_WIN_INC = C:/Program Files/MySQL/MySQL Server 8.0/include
MYSQL_WIN_LIB = C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.lib

OPENSSL_WIN_INC = C:/Program Files/OpenSSL-Win64/include
# Puntiamo alla cartella x64/MD (oppure cambia MD in MT se MD non esiste)
OPENSSL_WIN_LIB_DIR = C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD

# --- Configurazione Linux ---
LINUX_CFLAGS = $(CFLAGS) -I/usr/include/mysql
LINUX_LIBS = -lpthread -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lssl -lcrypto

# --- Configurazione Windows / MinGW ---
WINDOWS_CFLAGS = $(CFLAGS) -D_WIN32 -I"$(MYSQL_WIN_INC)" -I"$(OPENSSL_WIN_INC)"
WINDOWS_LIBS = -L"$(OPENSSL_WIN_LIB_DIR)" -lssl -lcrypto "$(MYSQL_WIN_LIB)" -lws2_32

# Target default
all: windows

linux:
	$(CC) $(LINUX_CFLAGS) $(SERVER_SRC) -o $(TARGET) $(LINUX_LIBS)

windows:
	$(CC) $(WINDOWS_CFLAGS) $(SERVER_SRC) -o $(TARGET).exe $(WINDOWS_LIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all linux windows clean