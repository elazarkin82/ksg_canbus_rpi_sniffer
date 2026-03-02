#include "emulator.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <poll.h>
#include <errno.h>

int can_open_socket(const char* interface_name) {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        return -1;
    }

    strcpy(ifr.ifr_name, interface_name);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("Ioctl");
        close(s);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        close(s);
        return -1;
    }

    return s;
}

void can_close_socket(int socket_fd) {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

int can_send_frame(int socket_fd, uint32_t can_id, const uint8_t* data, uint8_t len) {
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));

    frame.can_id = can_id;
    frame.can_dlc = len > 8 ? 8 : len;
    memcpy(frame.data, data, frame.can_dlc);

    if (write(socket_fd, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("Write");
        return -1;
    }
    return 0;
}

int can_read_frame(int socket_fd, uint32_t* can_id, uint8_t* data, uint8_t* len) {
    struct pollfd pfd;
    pfd.fd = socket_fd;
    pfd.events = POLLIN;

    // Wait for data with 100ms timeout
    // This allows the main loop to check g_running periodically
    int ret = poll(&pfd, 1, 100);

    if (ret < 0) {
        if (errno == EINTR) return 0; // Interrupted by signal, just return
        perror("Poll");
        return -1;
    }

    if (ret == 0) {
        // Timeout - no data available
        return 0;
    }

    if (pfd.revents & POLLIN) {
        struct can_frame frame;
        int nbytes = read(socket_fd, &frame, sizeof(frame));

        if (nbytes < 0) {
            perror("Read");
            return -1;
        }

        if (nbytes < (int)sizeof(struct can_frame)) {
            return 0; // Incomplete frame
        }

        *can_id = frame.can_id;
        *len = frame.can_dlc;
        memcpy(data, frame.data, frame.can_dlc);

        return 1; // Success (Data read)
    }

    return 0;
}
