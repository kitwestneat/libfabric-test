#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

struct net_info
{
    struct fi_info *fi;
    struct fid_fabric *fabric;
    struct fid_wait *wait_set;
    struct fid_eq *eq;

    struct peer_info *peer_list;
};

/*
struct server_info
{
    struct fid_pep *pep;
    struct server_connection *connection_list;
};

struct server_connection
{
    int client_id;
    struct server_connection *next;
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_cq *cq;
    void *read_buf;
};

struct client_info
{
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_cq *cq;
};
*/

struct peer_info
{
    int peer_id;
    int credits;
    struct fid_ep *ep;
    struct fid_cq *cq;
    struct peer_info *next;
};

enum net_request_type
{
    NRT_GET,
    NRT_PUT
};

struct net_request
{
    enum net_request_type nr_type;

    void *buf;
    size_t len;

    struct peer_info *cxn;
};

int init_network(struct net_info *ni, bool is_server);
void close_network(struct net_info *ni);

int get_network_wait_fd(struct net_info *ni);

int init_server(struct net_info *ni);
int run_server(struct net_info *ni);
void close_server(struct net_info *ni);

int init_client(struct net_info *ni);
int run_client(struct net_info *ni, const char *addr, unsigned short port);
void close_client(struct net_info *ni);

#endif
