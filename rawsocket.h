/*
   Frame structure:

   - Start. Frame must be aligned to TPACKET_ALIGNMENT=16
   - struct tpacket_hdr
   - pad to TPACKET_ALIGNMENT=16
   - Packet data
   - Pad to align to TPACKET_ALIGNMENT=16
 */

#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netdb.h>
#include <net/if.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>


#define CAPTURE 0
#define TRANSMISSION 1

// Frames are grouped in blocks, frames_per_block = tp_block_size / tp_frame_size
// Constraint: TP_BLOCK_SIZE * TP_BLOCK_NR = TP_FRAME_SIZE * TP_FRAME_NR
#define TP_BLOCK_SIZE 4096
#define TP_BLOCK_NR 256
#define TP_FRAME_SIZE 2048
#define TP_FRAME_NR 512

#define ETH_TYPE ETH_P_ALL
#define TFRAME_HDRLEN TPACKET_ALIGN(sizeof(struct tpacket_hdr))

struct tframe {
    struct tpacket_hdr tf_tphdr __attribute__((aligned(TPACKET_ALIGNMENT)));
    char tf_data[TP_FRAME_SIZE - TFRAME_HDRLEN] __attribute__((aligned(TPACKET_ALIGNMENT)));
};


int create_socket(char* eth_name, int socket_type);

struct tframe* map_ring(int socket_fd);

void close_socket(int socket_fd, struct tframe* buffer);
