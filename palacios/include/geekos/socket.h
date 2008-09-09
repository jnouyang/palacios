#ifndef GEEKOS_SOCKET_H
#define GEEKOS_SOCKET_H

#include <geekos/ring_buffer.h>
#include <uip/uip.h>
#include <geekos/kthread.h>


typedef enum {WAITING, CLOSED, LISTEN, ESTABLISHED} sock_state_t;

struct socket {
  int in_use;
  struct Thread_Queue recv_wait_queue;
  struct ring_buffer *send_buf;
  struct ring_buffer *recv_buf;
  struct uip_conn *con;

  sock_state_t state;

};


void init_network();

int connect(const uchar_t ip_addr[4], ushort_t port);
int close(const int sockfd);
int recv(int sockfd, void * buf, uint_t len);
int send(int sockfd, void * buf, uint_t len);

void set_ip_addr(uchar_t addr[4]);


#endif