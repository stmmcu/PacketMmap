#include "rawsocket.h"


int create_socket(char* eth_name, int socket_type)
{
    // create a socket
    int socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_TYPE));
    if(socket_fd == -1)
    {
        perror("Creating socket failed");
        exit(0);
    }

    if(socket_type == TRANSMISSION) {
        // get interface index
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, eth_name);
        if(ioctl(socket_fd, SIOCGIFINDEX, &ifr) == -1) {
            perror("ioctl failed");
            exit(0);
        }
        int ifindex = ifr.ifr_ifindex;

        // set sockaddr_ll struct to prepare binding.
        struct sockaddr_ll socket_addr;
        memset(&socket_addr, 0, sizeof(socket_addr));
        socket_addr.sll_family = AF_PACKET;
        socket_addr.sll_protocol = htons(ETH_TYPE);
        socket_addr.sll_ifindex = ifindex;

        // bind the socket descriptor to socket address
        if(bind(socket_fd, (struct sockaddr *)&socket_addr, sizeof(struct sockaddr_ll)) == -1) {
            perror("bind failed");
            exit(0);
        }
    }

    // allocate circular buffer (ring)
    struct tpacket_req req = {TP_BLOCK_SIZE, TP_BLOCK_NR, TP_FRAME_SIZE, TP_FRAME_NR};
    if(socket_type == TRANSMISSION) {
        if(setsockopt(socket_fd, SOL_PACKET, PACKET_TX_RING, (void *) &req, sizeof(req)))
        {
            perror("Setsockopt TX_RING failed");
            exit(0);
        }
    }
    else {
        if(setsockopt(socket_fd, SOL_PACKET, PACKET_RX_RING, (void *) &req, sizeof(req)))
        {
            perror("Setsockopt RX_RING failed");
            exit(0);
        }
    }

    return socket_fd;
}


struct tframe* map_ring(int socket_fd)
{
    // map buffer to user area
    struct tframe *circular_buffer = (struct tframe *) mmap(0,
        TP_FRAME_NR * TP_FRAME_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        socket_fd,
        0);
    if (circular_buffer == (void *)-1)
    {
        perror("Map ring failed");
        exit(0);
    }

    return circular_buffer;
}


void close_socket(int socket_fd, struct tframe* buffer)
{
    if(munmap(buffer, TP_FRAME_NR * TP_FRAME_SIZE))
    {
        perror("Munmap failed");
    }
    close(socket_fd);
}
