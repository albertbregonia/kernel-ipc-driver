#pragma once
#include <linux/list.h>
#include <linux/pid.h>
#include <net/sock.h>
#include <linux/types.h>

// NOTE: this can be used in kernel ONLY

enum ErrorCode {
    OK,
    INVALID_ARG,
    INVALID_PAYLOAD,
    ALLOCATE_NEW_NETLINK_MSG_FAILURE,
};

enum ExitCode {
    NETLINK_INIT_FAILURE,
};

struct PIDNode {
    __u32 pid;
    struct list_head links;
};

// prototype - compiling kernel complains about it
struct sk_buff* new_netlink_message(size_t len);

struct sk_buff* new_netlink_message(size_t len) {
    // note: GFP_KERNEL is a memory-allocation flag 
    struct sk_buff *buffer = nlmsg_new(len, GFP_KERNEL);
    if (!buffer) {
        return NULL;
    }
    struct nlmsghdr *netlink_msg = nlmsg_put(buffer, 0, 0, NLMSG_DONE, len, 0);
    if (!netlink_msg) {
        nlmsg_free(buffer);
        return NULL;
    }
    return buffer;
}