# Embedded Linux - LAB3

## Contents

- apps/: GDB debugging target application.
- initramfs/: Embedded Linux initramfs image.
- packages/: ARM gdbserver package.
- rootfs_config/: Init and BusyBox configuration.
- part_b/: Memory debugging with Valgrind.
- part_c/: System tracing with strace and perf.
- part_d/: Real-time testing with cyclictest.

## GDB Remote Debugging

The LAB3 initramfs includes:
- /usr/bin/gdbserver
- /usr/bin/buggy_app
- /usr/bin/buggy_app_static

The buggy application demonstrates:
- Array out-of-bounds access.
- Use-after-free.
- Integer overflow.

## Part B
Contains leaky_app source, binary and Valgrind logs.

## Part C
Contains trace_app source, binary, perf and strace logs.

## Part D
Contains cyclictest output and histogram.

## Validation
The initramfs archive passed gzip integrity validation.
The archive contains gdbserver and both buggy_app binaries.
