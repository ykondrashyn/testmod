# testmod
### Kernel module implementing a character device

This module implements 3 character devices with the same dynamically allocated major number and 3 minor numbers, each character device is independent and support the following methods:

*Write method*: Appends each new string to the buffer without clearing previously written strings. If the buffer is full -- blocks until the buffer is cleared.

*Read method*: Reads all the strings that were written into an internal buffer, blocks if the buffer is empty or has less data than requested.

*IOCTLs method*:

  1) Clear last written string

  2) Clear Full Buffer

  3) Return free bytes left in the internal buffer
