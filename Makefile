CC       = gcc
CFLAGS   = -Wall -Wextra
LDFLAGS  = -lm

BUILD_DIR = build
SRC_DIR = src

CFLAGS += -I$(SRC_DIR) -I$(SRC_DIR)/platform

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

OBJS = $(BUILD_DIR)/socket.o $(BUILD_DIR)/protocol.o
OBJ_CLIENT = $(BUILD_DIR)/client.o 
OBJ_SERVER = $(BUILD_DIR)/server.o
TARGETS = $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/socket.o: $(SOCKET_SRC) $(SRC_DIR)/platform/socket.h $(SRC_DIR)/platform/error.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/platform/socket.h $(SRC_DIR)/platform/error.h $(SRC_DIR)/protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_CLIENT): $(BUILD_DIR)/client.o $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(TARGET_SERVER): $(BUILD_DIR)/server.o $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGETS)
