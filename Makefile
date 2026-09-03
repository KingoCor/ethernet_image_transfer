CC       = gcc
CFLAGS   = -Wall -Wextra
LDFLAGS  =

BUILD_DIR = build
SRC_DIR = src

CFLAGS += -I$(SRC_DIR)/platform

UNAME_S := $(shell uname -s 2>/dev/null || echo "Unknown")
ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
endif
ifeq ($(OS),Windows_NT)
    PLATFORM = win
endif

ifdef CROSS_WIN
    PLATFORM = win
    CC = x86_64-w64-mingw32-gcc
endif

ifeq ($(PLATFORM),win)
    LDFLAGS += -lws2_32
    CFLAGS  += -D_WIN32_WINNT=0x0600
    SOCKET_SRC = $(SRC_DIR)/platform/socket_win.c
	TARGET_CLIENT = client.exe
	TARGET_SERVER = server.exe
else ifeq ($(PLATFORM),linux)
    SOCKET_SRC = $(SRC_DIR)/platform/socket_linux.c
	TARGET_CLIENT = client
	TARGET_SERVER = server
else
    $(error Platform not supported)
endif

OBJS = $(BUILD_DIR)/socket.o $(BUILD_DIR)/client.o $(BUILD_DIR)/server.o
TARGETS = $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/socket.o: $(SOCKET_SRC) $(SRC_DIR)/platform/socket.h $(SRC_DIR)/platform/error.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/client.o: $(SRC_DIR)/client.c $(SRC_DIR)/platform/socket.h $(SRC_DIR)/platform/error.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/server.o: $(SRC_DIR)/server.c $(SRC_DIR)/platform/socket.h $(SRC_DIR)/platform/error.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

client: $(BUILD_DIR)/client.o $(BUILD_DIR)/socket.o
	$(CC) $^ $(LDFLAGS) -o $@

server: $(BUILD_DIR)/server.o $(BUILD_DIR)/socket.o
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGETS)
