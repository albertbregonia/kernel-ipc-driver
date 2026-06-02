#pragma once
#include <linux/types.h>
#include <linux/netlink.h>

// NOTE: this can be used in userspace or kernel

enum IPCMessageType {
    INVALID,
    REGISTER_SUBSCRIBER,
    REGISTER_PUBLISHER,
    BROADCAST,
};

struct IPCMessage {
    enum IPCMessageType type;
    __u32 from_pid;
    size_t content_length;
    char* content;
};

struct IPCMessage parse_netlink_msg_header(struct nlmsghdr *header);
void put_ipc_message_format(
    struct nlmsghdr *netlink_msg,
    enum IPCMessageType type, 
    char *msg, 
    size_t len
);
void put_ipc_register_message(struct nlmsghdr *netlink_msg, int subscriber);
char* put_ipc_message_type(struct nlmsghdr *netlink_msg, enum IPCMessageType type);

// IPCMessage is just a parsed wrapper over the `struct nlmsghdr`
// therefore, you must ensure that the lifetime of the data 
// is the same lifetime or earlier of the `struct nlmsghdr`.
// This is something that Rust inherently enforces.
struct IPCMessage parse_netlink_msg_header(struct nlmsghdr *header) {
    char *msg = (char*) NLMSG_DATA(header);
    struct IPCMessage ipc_msg = {
        .from_pid = header->nlmsg_pid, // netlink port ID is the process ID
        // first byte is an uint8_t for the type, the rest is string
        // there is probably a more idiomatic way to include a custom header
        // but we're keeping it simple
        .type = msg[0],
        .content = msg+1, // skip type byte, increment pointer
    };
    ipc_msg.content_length = NLMSG_PAYLOAD(header, 0); // 0 for no offset
    return ipc_msg;
}

// sets the first byte of the netlink msg payload to one of enum IPCMessageType
char* put_ipc_message_type(struct nlmsghdr *netlink_msg, enum IPCMessageType type) {
    char* payload_buffer = NLMSG_DATA(netlink_msg);
    payload_buffer[0] = type;
    return payload_buffer;
}

void put_ipc_message_format(
    struct nlmsghdr *netlink_msg,
    enum IPCMessageType type, 
    char *msg, 
    size_t len
) {
    // sets the first byte of the payload to the IPCMessageType enum value
    char* payload_buffer = put_ipc_message_type(netlink_msg, type);
    memcpy(payload_buffer+1, msg, len);
}

// NOTE: this is not idempotent! 
// therefore if you register multiple times, the driver will register it more than once.
// logically, this is fine as the driver does not echo messages back to the sender. it simply wastes memory
void put_ipc_register_message(struct nlmsghdr *netlink_msg, int subscriber) {
    enum IPCMessageType type = subscriber > 0 ? REGISTER_SUBSCRIBER : REGISTER_PUBLISHER;
    put_ipc_message_format(netlink_msg, type, "\0", 1);
}