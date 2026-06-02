
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// non-standard C, POSIX constructs
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "ipc_message.h"

#define NETLINK_USER 31
#define MAX_PAYLOAD 128

struct Message {
    struct nlmsghdr* netlink_msg;
    struct msghdr socket_msg;
};

struct GlobalState {
    struct sockaddr_nl dest_addr;
    pid_t process_id;
    int netlink_fd;
};

// here we essentially build a `msghdr` for use with `sendmsg`
// but internally, all the allocations and values point to a `nlmsghdr`
struct Message new_message(int netlink_port_id, struct sockaddr_nl* dest_addr, size_t payload_size) {
    struct nlmsghdr *netlink_msg = malloc(NLMSG_SPACE(payload_size));
    // zero out netlink message header space - otherwise we may send "freed" data
    memset(netlink_msg, 0, NLMSG_SPACE(payload_size));
    netlink_msg->nlmsg_len = NLMSG_SPACE(payload_size);
    netlink_msg->nlmsg_pid = netlink_port_id;
    netlink_msg->nlmsg_flags = 0;
    // have iov refer to netlink_msg internals
    struct iovec* iov = malloc(sizeof(struct iovec));
    iov->iov_base = netlink_msg;
    iov->iov_len = netlink_msg->nlmsg_len;
    // create `socket_msg` to be used with `sendmsg`
    struct msghdr socket_msg = {
        .msg_name = dest_addr,
        .msg_namelen = sizeof(*dest_addr),
        .msg_iov = iov,
        .msg_iovlen = 1,
    };
    struct Message container = {
        netlink_msg,
        socket_msg,
    };
    return container;
}

void free_message(struct Message container) {
    free(container.socket_msg.msg_iov);
    free(container.netlink_msg);
}

void* publisher_thread(void* state_ptr) {
    struct GlobalState *state = (struct GlobalState*) state_ptr; 
    struct Message container = new_message(state->process_id, &state->dest_addr, MAX_PAYLOAD);
    put_ipc_register_message(container.netlink_msg, 0);

    // register with the kernel that this thread is going to be publishing
    int result = sendmsg(state->netlink_fd, &container.socket_msg, 0);
    if(result < 0) {
        fprintf(stderr, "Failed to send IPC registration message! Error Code: %d. Closing publisher thread...\n", result);
        free_message(container);
        return NULL;
    }
    printf("Registered for IPC publishing with kernel\n");

    // send broadcast messages reusing the netlink_msg payload buffer
    // we +1 here to the returned pointer to skip over the IPCMessage type byte in the payload
    char* payload_buffer = put_ipc_message_type(container.netlink_msg, BROADCAST) + 1;
    while(1) {
        printf("Send message to kernel: ");
        fgets(payload_buffer, MAX_PAYLOAD-1, stdin); // -1 to account for type byte
        payload_buffer[strlen(payload_buffer)-1] = '\0'; // fgets includes a '\n' remove it
        if(strcmp(payload_buffer, "exit") == 0) {
            printf("Exiting...\n");
            free_message(container);
            break;
        }
        result = sendmsg(state->netlink_fd, &container.socket_msg, 0);
        if(result < 0) {
            fprintf(stderr, "Failed to broadcast user input! Error Code: %d. Closing publisher thread...\n", result);
            free_message(container);
            break;
        }
    }
    return NULL;
}

void* subscriber_thread(void* state_ptr) {
    struct GlobalState *state = (struct GlobalState*) state_ptr; 
    struct Message container = new_message(state->process_id, &state->dest_addr, MAX_PAYLOAD);
    put_ipc_register_message(container.netlink_msg, 1);
    
    // register with the kernel that this thread is going to be subscribing
    int result = sendmsg(state->netlink_fd, &container.socket_msg, 0);
    if(result < 0) {
        fprintf(stderr, "Failed to send IPC registration message! Error Code: %d. Closing subscriber thread...\n", result);
        free_message(container);
        return NULL;
    }
    printf("Registered for IPC subscribing with kernel\n");
    char* payload_buffer = (char*) NLMSG_DATA(container.netlink_msg);
    while(1) {
        result = recvmsg(state->netlink_fd, &container.socket_msg, 0);
        if(result < 0) {
            fprintf(stderr, "Failed to receive IPC message! Error Code: %d. Closing subscriber thread...\n", result);
            free_message(container);
            break;
        }
        printf("Received message payload: `%s`\n", payload_buffer);
    }
    return NULL;
}

int main() {
    struct GlobalState state = {
        .process_id = getpid(),
    };
    struct sockaddr_nl src_addr;
    int netlink_socket_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (netlink_socket_fd < 0) {
        fprintf(stderr, "Failed to create netlink socket! Error Code: %d. Aborting...\n", netlink_socket_fd);
        return -1;
    }
    state.netlink_fd = netlink_socket_fd;

    // src_addr config
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = state.process_id; // netlink port ID will be the process ID

    // bind netlink port
    int result = bind(netlink_socket_fd, (struct sockaddr*) &src_addr, sizeof(src_addr));
    if(result < 0) {
        fprintf(stderr, "Failed to bind netlink port to process ID! Error Code: %d. Aborting...\n", result);
    }

    // set up destination address for all netlink messages
    memset(&state.dest_addr, 0, sizeof(state.dest_addr));
    state.dest_addr.nl_family = AF_NETLINK;
    state.dest_addr.nl_pid = 0; // destination port ID is 0 for kernel
    state.dest_addr.nl_groups = 0; // unicast

    // start publisher and subscriber threads
    // we can pass `&state` here without a mutex 
    // bc all values aren't concurrently modified
    pthread_t threads[2];
	pthread_create(&threads[0], NULL, publisher_thread, &state);
	pthread_create(&threads[1], NULL, subscriber_thread, &state);

    // block the process from closing by waiting on the publisher thread
	pthread_join(threads[0], NULL);

    // realistically, since this is userspace, the kernel will clean up the process
    // by freeing allocations, file descriptors and killing threads.
    // lowk this is just good practice.
    pthread_cancel(threads[1]); // kill subscriber thread
    close(state.netlink_fd); // free netlink socket
    return 0;
}
