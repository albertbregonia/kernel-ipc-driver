# Kernel IPC Module

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
# ensure that the running kernel version matches the header version 
# and gcc is compatible with that version as well
sudo pacman -S gcc linux linux-headers
```

## Motivation
This project was originally for my operating systems class to learn about general differences when programming in userspace vs the kernel. I decided to revamp the project with better structured code to be a better reference when doing larger future projects.

Furthermore, my language of choice these days is usually Rust. As Linux supports this out of the box now, I plan to do a small rewrite of the module to highlight the differences. As I write C code again, I am always reminded of the many pitfalls that Rust prevents inherently (through the use of lifetimes and memory ownership)

## Architecture

```
--------------------------------
|  1. kernel_ipc_driver_init   | 
--------------------------------                
```
 After running `sudo insmod` this function is ran, it initializes the netlink socket "server", and then places the pointer in the `GLOBAL_STATE` struct. `GLOBAL_STATE` is just a way to have a collection of all the global variables. This is nicer than having them all scattered about at the top of the file as it shows intent. I'm not usually a fan of these because 9/10 times you can just use an argument to a function and it's better practice to follow dependency injection. However, this case is that 10th time and since `netlink_msg_handler` will be called internally when a netlink message is received. Therefore, we cannot pass it in as an argument to the function without some sort of workaround. This is fine as the `GLOBAL_STATE` vars live as long as the driver lives.
 
```
        ----------------------------            note: REGISTER is abbreviation for the enums REGISTER_PUBLISHER and REGISTER_SUBSCRIBER
        |  2. netlink_msg_handler  |                  BROADCAST is also of the same type
        ----------------------------
                    |
                    v
        [ parse_netlink_msg_header ]
                    |
                    v
                    | 
           ---- <type == REGISTER?> -------------
           | yes                             no |
           v                                    | 
        [ register_process ]                    |
           |                                    |
           v                                    v
        <pub or sub?>                    <type == BROADCAST?>
     pub |       | sub                     |            |
         v       v                     yes |            | no                    
   [add to pub/sub linked list]            |            ----------------------------
             |                             v                                       |
             |                      < is_publisher_alive >                         |
             |                             |        |                              |
             |                         yes |        |  no                          |
             |                             v        |                              |
             |                       [ broadcast ]  |                              |
             |                             |        |                              |
             |                             |        |                              |
             |                             v        v                              v
             ------------------------------+--------+-------------------------> [ end ]
            
```
As previously mentioned `netlink_msg_handler` is a callback invoked upon any new netlink message received, passed in as a `struct sk_buff*`. The workflow is shown above at a high level not including helpers and internals that will be explained here.


## Other Notes
- In the [`.vscode`](/.vscode) folder, I created two configurations so that I could use IntelliSense while developing. It is important to know to switch between "Kernel" and "Userspace" configurations when developing for either or so that IntelliSense can index the correct headers and dependencies. Therefore, IntelliSense will show errors for userspace code when using the "Kernel" configuration and vice-versa.
- Furthermore, IntelliSense in VSCode false-positively highlights the strings for the `MODULE_` macros in `#include <linux/module.h>` with the error:
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
