# Makefile - Sensor Monitoring System
# Author: Student
# Student ID: SE204292
# Date: 12/11/2026

CC_HOST = gcc
CC_ARM = arm-linux-gnueabihf-gcc

CFLAGS = -Wall -Wextra -O2 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LIBS = -pthread -lrt -lm

SOURCES = sms_app.c \
          sensor_thread.c \
          processor_thread.c \
          alert_thread.c \
          logger_thread.c \
          circular_buffer.c

TARGET_HOST = sms_app_host
TARGET_ARM = sms_app_arm

.PHONY: all host arm clean

all: host

# Build cho Ubuntu x86
host:
	$(CC_HOST) $(CFLAGS) $(SOURCES) -o $(TARGET_HOST) $(LIBS)

# Build cho QEMU ARM
arm:
	$(CC_ARM) $(CFLAGS) -static $(SOURCES) -o $(TARGET_ARM) $(LIBS)

# Xoa cac file da bien dich
clean:
	rm -f $(TARGET_HOST) $(TARGET_ARM)
