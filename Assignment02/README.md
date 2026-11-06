# Embedded Linux - Assignment 02

## Contents

- app/: Application source code and host/ARM binaries.
- driver/: SMS sensor kernel module source and build files.
- logs/: Jitter measurements, perf, strace and QEMU logs.
- output/: ARM application, kernel module and QEMU boot assets.

## Application

The application uses multiple threads:
- Sensor thread
- Processor thread
- Logger thread
- Alert thread

## Driver

Contains the SMS sensor kernel module.

## Performance Evaluation

Logs and plots include:
- FIFO scheduling jitter
- Other scheduling policy jitter
- Cyclic timing measurements
- perf report
- strace output
- QEMU session logs
