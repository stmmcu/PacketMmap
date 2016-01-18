#include <pthread.h>
#include <sys/time.h>
#include "rawsocket.h"

struct interval_counter {
    long sum;
    int count;
    long min;
    long max;
};

static struct tframe *s_transmission_buffer = NULL;
static int s_transmission_buffer_index = 0;
static struct tframe *s_capture_buffer = NULL;
static int s_capture_buffer_index = 0;
static struct ethhdr s_eth_hdr = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 0xffff};
static struct interval_counter itl_ctr = {0, 0, 2147483647L, 0};


void send_packet(int socket_fd)
{
    // get current time
    struct timeval tv;
    if(gettimeofday(&tv, NULL) == -1) {
        perror("get time failed");
        return;
    }

    // fill frame with data
    struct tframe *tf = &s_transmission_buffer[s_transmission_buffer_index];
    if(tf->tf_tphdr.tp_status == TP_STATUS_AVAILABLE) {
        char *data = tf->tf_data;
        int data_len = 0;

        memcpy(data, &s_eth_hdr, sizeof(struct ethhdr));
        data += sizeof(struct ethhdr);
        data_len += sizeof(struct ethhdr);

        memcpy(data, &tv, sizeof(struct timeval));
        data += sizeof(struct timeval);
        data_len += sizeof(struct timeval);

        // set tp_status to request for sending, and tp_len must be greater than 14
        tf->tf_tphdr.tp_len = data_len;
        tf->tf_tphdr.tp_status = TP_STATUS_SEND_REQUEST;

        s_transmission_buffer_index = (s_transmission_buffer_index + 1) % TP_FRAME_NR;
    }
    else {
        printf("TX_RING is not available\n");
    }

    // send frames with status equal to TP_STATUS_SEND_REQUEST
    if(send(socket_fd, NULL, 0, MSG_DONTWAIT) == -1) {
        perror("Send packet failed");
        return;
    }
}


int compare_ethhdr(struct ethhdr *eth_hdr1, struct ethhdr *eth_hdr2)
{
    if(eth_hdr1 == NULL || eth_hdr2 == NULL) {
        return 0;
    }
    char *ethhdr_str1 = (char *)eth_hdr1, *ethhdr_str2 = (char *)eth_hdr2;
    int retval = 1;
    int index;
    for(index = 0; index < sizeof(struct ethhdr); ++index) {
        if(ethhdr_str1[index] != ethhdr_str2[index]) {
            retval = 0;
            break;
        }
    }
    return retval;
}

void receive_packet(int socket_fd)
{
    struct timeval tv_recv, *tv_send;
    long send_recv_interval;
    struct tframe *tf;
    char *data;

    // create poll descriptor
    struct pollfd pfd;
    pfd.fd = socket_fd;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;
    int rev_num = 1;

    // wait for packets
    tf = &s_capture_buffer[s_capture_buffer_index];
    if (tf->tf_tphdr.tp_status == TP_STATUS_KERNEL) {
        rev_num = poll(&pfd, 1, 3);
        if(rev_num <= 0) {
            perror("poll error");
            return;
        }
    }

    // get current time
    if(gettimeofday(&tv_recv, NULL) == -1) {
        perror("get time failed");
        return;
    }

    // get data from RX_RING
    data = tf->tf_data;
    data += 34;
    // check whether the message comes from a certern sender
    if(compare_ethhdr(&s_eth_hdr, (struct ethhdr *)data) == 1) {
        data += sizeof(struct ethhdr);
        tv_send = (struct timeval *)data;
        // save send_recv interval
        send_recv_interval = (tv_recv.tv_sec - tv_send->tv_sec) * 1000000 + (tv_recv.tv_usec - tv_send->tv_usec);
        itl_ctr.sum += send_recv_interval;
        itl_ctr.count += 1;
        if(send_recv_interval < itl_ctr.min)
        {
            itl_ctr.min = send_recv_interval;
        }
        if(send_recv_interval > itl_ctr.max)
        {
            itl_ctr.max = send_recv_interval;
        }
        printf("interval_avg: %ld, interval_min: %ld, interval_max: %ld\n", itl_ctr.sum / itl_ctr.count, itl_ctr.min, itl_ctr.max);
    }

    tf->tf_tphdr.tp_status = TP_STATUS_KERNEL;
    s_capture_buffer_index = (s_capture_buffer_index + 1) % TP_FRAME_NR;

    // ignore left messages
    tf = &s_capture_buffer[s_capture_buffer_index];
    while(tf->tf_tphdr.tp_status != TP_STATUS_KERNEL) {
        tf->tf_tphdr.tp_status = TP_STATUS_KERNEL;
        s_capture_buffer_index = (s_capture_buffer_index + 1) % TP_FRAME_NR;
        tf = &s_capture_buffer[s_capture_buffer_index];
    }
}


int main()
{
    // // set current thread to high priority
    // struct sched_param sp;
    // sp.sched_priority = 99;
    // pthread_t cur_thread = pthread_self();
    // if (pthread_setschedparam(cur_thread, SCHED_RR, &sp) != 0)
    // {
    //    perror("set schedual parameters failed");
    // }

    int transmission_sockfd = create_socket("eth0", TRANSMISSION);
    s_transmission_buffer = map_ring(transmission_sockfd);
    int capture_sockfd = create_socket("eth1", CAPTURE);
    s_capture_buffer = map_ring(capture_sockfd);

    while(1) {
        send_packet(transmission_sockfd);
        receive_packet(capture_sockfd);
        if(itl_ctr.count > 10000) {
            break;
        }
    }

    close_socket(transmission_sockfd, s_transmission_buffer);
    close_socket(capture_sockfd, s_capture_buffer);
    return 0;
}
