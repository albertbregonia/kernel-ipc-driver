#include <linux/module.h>
#include <linux/skbuff.h>

#include "kernel_ipc_driver.h"
#include "ipc_message.h"

#define NETLINK_USER 31

struct GlobalState {
    struct sock *netlink_socket;
    struct PIDNode subscribers;
    struct PIDNode publishers;
};

static struct GlobalState GLOBAL_STATE = {
    .netlink_socket=NULL,
    .subscribers={ .pid=-1, .links=LIST_HEAD_INIT(GLOBAL_STATE.subscribers.links) },
    .publishers={ .pid=-1, .links=LIST_HEAD_INIT(GLOBAL_STATE.publishers.links) },
};

static int register_process(struct IPCMessage ipc_msg) {
    // if this is not a register command, don't malloc
    if(ipc_msg.type != REGISTER_PUBLISHER &&
        ipc_msg.type != REGISTER_SUBSCRIBER) {
        printk(KERN_DEBUG "Attempted to register PID %d with non-register type: %d\n", ipc_msg.from_pid, ipc_msg.type);
        return INVALID_ARG;
    }
    // create new PIDNode
    struct PIDNode *new = kmalloc(sizeof(struct PIDNode), GFP_KERNEL);
    new->pid = ipc_msg.from_pid;
    INIT_LIST_HEAD(&new->links);

    if(ipc_msg.type == REGISTER_SUBSCRIBER) {
        list_add_tail(&new->links, &GLOBAL_STATE.subscribers.links);
        printk(KERN_INFO "Registered new subscriber process PID: %d\n", new->pid);
    } else if(ipc_msg.type == REGISTER_PUBLISHER) {
        list_add_tail(&new->links, &GLOBAL_STATE.publishers.links);
        printk(KERN_INFO "Registered new publisher process PID: %d\n", new->pid);
    }
    return OK;
}

static int broadcast(struct IPCMessage ipc_msg) {
    struct list_head *i, *temp;
    list_for_each_safe(i, temp, &GLOBAL_STATE.subscribers.links) { // for each sub PID
        struct PIDNode *sub_node = list_entry(i, struct PIDNode, links);

        // broadcast to subs that are not itself
        if (sub_node->pid == ipc_msg.from_pid) {
            continue;
        }
        // create netlink msg to send to userspace
        struct sk_buff* output = new_netlink_message(ipc_msg.content_length);
        if(!output) {        
            printk(KERN_INFO "Failed to allocate netlink output message\n");
            return ALLOCATE_NEW_NETLINK_MSG_FAILURE;
        }
        NETLINK_CB(output).dst_group = 0; // unicast
        struct nlmsghdr *netlink_msg = (struct nlmsghdr*) output->data;
        memcpy(NLMSG_DATA(netlink_msg), ipc_msg.content, ipc_msg.content_length); // copy message to buffer
        
        // send netlink msg to userspace, we do not need to free `output` bc `nlmsg_unicast` consumes it
        int result = nlmsg_unicast(GLOBAL_STATE.netlink_socket, output, sub_node->pid);
        if (result < 0) { 
            // subs that error, we treat as dead processes and remove them
            printk(KERN_ERR "Failed to broadcast message to PID: %d. Reason %d. Deleting...\n", result, sub_node->pid);
            list_del(i);
            kfree(sub_node);
        } else {
            printk(KERN_INFO "Sent message to PID: %d from PID: %d\n", sub_node->pid, ipc_msg.from_pid);
        }
    }
    return OK;
}

static inline bool is_process_alive(int pid) {
    return pid_task(find_vpid(pid), PIDTYPE_PID);
}

// if the publisher is in the list but not active, this will remove it and return false eventually
static bool is_publisher_alive(int pid) {
    bool seen = false;
    struct list_head *it, *temp; // iterators
    list_for_each_safe(it, temp, &GLOBAL_STATE.publishers.links) { // for each pub PID
        struct PIDNode *publisher_node = list_entry(it, struct PIDNode, links);
        if (!is_process_alive(publisher_node->pid)) {
            // free pubs that aren't active
            printk(KERN_ERR "PID: %d is not active. Deleting...\n", publisher_node->pid);
            list_del(it);
            kfree(publisher_node);
            continue;
        }
        if(!seen && publisher_node->pid == pid) {
            seen = true;
        }
    }
    return seen;
}

static void clear_pid_list(struct list_head *list) {
    struct list_head *i, *temp;
    list_for_each_safe(i, temp, list) { // for each sub PID
        struct PIDNode *node = list_entry(i, struct PIDNode, links);
        printk(KERN_DEBUG "Deleting and freeing PID: %d", node->pid);
        list_del(i); // delete from list
        kfree(node); // free memory
    }
}

static void netlink_msg_handler(struct sk_buff *skb) {
    printk(KERN_INFO "Handler invoked: %s\n", __FUNCTION__);
    struct nlmsghdr *header = (struct nlmsghdr*) skb->data;
    struct IPCMessage ipc_msg = parse_netlink_msg_header(header);
    printk(KERN_DEBUG "Parsed IPC message from PID: %d. Type: %d\n", ipc_msg.from_pid, ipc_msg.type);
    if(ipc_msg.type == REGISTER_PUBLISHER || 
        ipc_msg.type == REGISTER_SUBSCRIBER) {
        register_process(ipc_msg);
        return;
    }
    if(ipc_msg.type == BROADCAST && is_publisher_alive(ipc_msg.from_pid)) {
        printk(KERN_INFO "Broadcasting payload: `%s`\n", ipc_msg.content);
        int result = broadcast(ipc_msg);
        if(result != OK) {
            printk(KERN_ERR "Failure during broadcast: %d\n", result);
            return;
        }
        printk(KERN_INFO "Broadcasted received msg payload sucessfully\n");
    } else {
        printk(KERN_INFO "Unhandled IPC message Type: %d Content: `%s`\n", ipc_msg.type, ipc_msg.content);
    }
}

static int __init kernel_ipc_driver_init(void) {
    printk("Initializing Kernel IPC Driver using: %s\n", __FUNCTION__);
    struct netlink_kernel_cfg netlink_config = { .input = netlink_msg_handler };
    struct sock *netlink_socket = netlink_kernel_create(
        &init_net,
        NETLINK_USER,
        &netlink_config
    );
    if (!netlink_socket) {
        printk(KERN_ALERT "Error creating socket.\n");
        return NETLINK_INIT_FAILURE;
    }
    GLOBAL_STATE.netlink_socket = netlink_socket;
    return 0;
}

static void __exit kernel_ipc_driver_exit(void) {
    printk(KERN_INFO "Uninitializing Kernel IPC Driver using: %s\n", __FUNCTION__);
    clear_pid_list(&GLOBAL_STATE.publishers.links);
    clear_pid_list(&GLOBAL_STATE.subscribers.links);
    netlink_kernel_release(GLOBAL_STATE.netlink_socket);
}

module_init(kernel_ipc_driver_init);
module_exit(kernel_ipc_driver_exit);

// these will false-positive error with vscode IntelliSense despite being resolvable
#ifndef __INTELLISENSE__
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Albert Bregonia");
MODULE_DESCRIPTION("A kernel driver used to handle inter-process communication");
#endif
