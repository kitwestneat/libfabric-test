#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

struct net_info {
    struct fi_info *fi;
    struct fid_fabric *fabric;
    struct fid_wait *wait_set;
    struct fid_eq *eq;

    struct server_info *server;
    struct client_info *client;
};

struct server_info {
    struct fid_pep *pep;
    struct server_connection *connection_list;
};

struct server_connection {
    struct server_connection *next;
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_cq *cq;
    void *read_buf;
};

struct client_info {
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_cq *cq;
};

int init_network(struct net_info *ni, bool is_server);
void close_network(struct net_info *ni);

int init_server(struct net_info *ni);
int run_server(struct net_info *ni);
void close_server(struct net_info *ni);

int init_client(struct net_info *ni);
int run_client(struct net_info *ni, const char *addr, unsigned short port);
void close_client(struct net_info *ni);

#endif
