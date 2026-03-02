#ifndef CAN_UTILS_H
#define CAN_UTILS_H

#include <stdint.h>

int can_open_socket(const char* interface_name);
void can_close_socket(int socket_fd);
int can_send_frame(int socket_fd, uint32_t can_id, const uint8_t* data, uint8_t len);
int can_read_frame(int socket_fd, uint32_t* can_id, uint8_t* data, uint8_t* len);

#endif // CAN_UTILS_H
