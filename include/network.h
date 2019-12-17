#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAGIC 0x12345678
struct network_handshake
{
    uint64_t magic;
    uint64_t bulk_key;
    uint64_t cmd_key;
};

struct net_info
{
    struct fi_info *fi;
    struct fid_fabric *fabric;
    struct fid_wait *wait_set;
    struct fid_eq *eq;

    struct server_info *server;
    struct client_info *client;

    struct fid_domain *domain;

    struct network_handshake local_keys;
};

struct server_info
{
    struct fid_pep *pep;
    struct server_connection *connection_list;
};

struct server_connection
{
    int client_id;
    struct server_connection *next;
    struct net_info *ni;
    struct fid_ep *ep;
    struct fid_cq *cq;

    struct network_handshake remote_keys;

    void *bulk_buf;
    struct network_cmd *cmd_buf;
};

struct server_request
{
    void (*callback)(struct server_request *rq);
    struct server_connection *cxn;
};

struct client_info
{
    struct fid_ep *ep;
    struct fid_cq *cq;
};

enum net_cmd_type
{
    GET = 0,
    PUT
};

struct network_cmd
{
    enum net_cmd_type type;
    size_t len;
    uint64_t addr;
};

int init_network(struct net_info *ni, bool is_server);
void close_network(struct net_info *ni);

int init_server(struct net_info *ni);
int run_server(struct net_info *ni);
void close_server(struct net_info *ni);

int init_client(struct net_info *ni);
int run_client(struct net_info *ni, const char *addr, unsigned short port);
void close_client(struct net_info *ni);
void cmd_recv(struct server_request *rq);

#endif
