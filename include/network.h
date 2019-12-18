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

    // server only
    struct fid_pep *pep;

    struct connection *connection_list;

    struct fid_domain *domain;

    struct network_handshake local_keys;
};

struct connection
{
    int client_id;
    struct connection *next;
    struct net_info *ni;
    struct fid_ep *ep;
    struct fid_cq *cq;

    void *bulk_buf;
    struct network_cmd *cmd_buf;
};

struct network_request
{
    void (*callback)(struct network_request *rq);
    struct connection *cxn;
    void *rq_data;

    int rq_res;
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

int setup_connection(struct net_info *ni, struct connection **cxn_ptr, struct fi_info *info);

void cmd_recv(struct network_request *rq);
void cmd_send(struct network_request *rq);

void bulk_recv(struct network_request *rq);
void bulk_send(struct network_request *rq);

void process_all_cq_events(struct net_info *ni);
void process_cq_events(struct connection *cxn);

#endif
