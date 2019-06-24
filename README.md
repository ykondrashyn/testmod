# testmod
### Kernel module implementing a character device

This module implements 3 character devices with same dynamically allocated major number and 3 minor numbers, each character device is independent and suport following methods:

*Write method*: Appends each new string to the buffer without clearing previously written strings. If buffer is full -- blocks until buffer is cleared.

*Read method*: Reads all the strings that were written into internal buffer, blocks if buffer is empty or has less data than requested.

*IOCTLs method*:

    1) Clear last written string

    2) Clear Full Buffer

    3) Return free bytes left in the internal buffer
