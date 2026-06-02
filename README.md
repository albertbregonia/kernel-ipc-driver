# Kernel IPC Driver

A Linux kernel module that performs IPC (Inter-Process Communication) by ingesting messages from publishers and broadcasting them back to subscribing processes over a `netlink` socket.

### Learning Objectives:
- kernel level library constructs (eg. `netlink`, `list_head`, `kmalloc`, `kfree`)
- concurrency (eg. pthreads, mutexes in C)
- kernel module development process (eg. `make` with linux headers, `lsmod`, `insmod`, `rmmod`, `dmesg`)
- OS concepts (userspace, kernel, POSIX)
- Seeing ASCII Tux when my laptop kernel panics
- Setting up VSCode IntelliSense to work with Linux headers

***Yes, this could very well be simplified by using multicast with `netlink` and is the better way of implementing IPC with `netlink`.***. I did explore this idea- and I understand that it ***is*** the better option as relying on the well-vetted, well-established library will always triumph over a rework. 

However, as a reference repo, the kernel code would've simply been a `netlink_kernel_create` call with a handler that performs a `nlmsghdr` clone and `nlmsg_multicast` with a group id. Too simple, too much abstraction, ***not enough learning***.

## Dependencies
I wrote and tested this on my laptop running Arch Linux kernel version `7.0.10-arch1-1`

```bash
# ensure that the running kernel version matches the header version and gcc is compatible with that as well
sudo pacman -S gcc linux linux-headers
```

TODO: I plan on making a `dockerfile` for this so I don't have to restart my laptop when it inevitably kernel panics.

## Motivation
This project was originally for my operating systems class to learn about general differences when programming in userspace vs the kernel. I decided to revamp the project with better structured code to be a better reference when doing larger future projects.

Furthermore, my language of choice these days is usually Rust. As Linux supports this out of the box now, I plan to do a small rewrite of the module to highlight the differences. As I write C code again, I am always reminded of the many pitfalls that Rust prevents inherently (through the use of lifetimes and memory ownership)

## Architecture
TODO

## Other Notes
- The `make` system expects the makefile to be `Makefile` with a capital M (in Linux unlike Windows, files are case-sensitive)
- IntelliSense in VSCode false-positively highlights the strings for the `MODULE_` macros in `#include <linux/module.h>` with the error:
    ```
    expression must have integral type
    ```
    I spent way too long trying to fix this to no avail. Therefore, I used the preprocessor directive so I could move on with my life.
    ```
    #ifndef __INTELLISENSE__
    ...
    #endif
    ```
- Usually, I would try to organize with a modern file structure, `src/` with all the source code, `build/` with all the build artifacts and then other minor 1-off files and scripts at the root `/` of the directory. However, the idiomatic way is to build in the `src/` as I've learned that Linux strictly believes in the monolith. I believe that this is terrible and clutters the directory a ton. However, it was way too cumbersome to fix myself, so I left it alone.
