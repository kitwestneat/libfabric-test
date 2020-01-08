#ifndef RDMT_H
#define RDMT_H

#include "network.h"

struct rdmt_app_info
{
    struct net_info *net;
};

struct rdmt_peer
{
    struct net_peer_data *peer_net_data;
};

struct rdmt_peer_rq
{
    struct rdmt_peer *peer;

    int (*next_func)(struct rdmt_app_info *app, struct rdmt_peer_rq *rq);
    void *next_data;
};

struct rdmt_hello_data
{
    int rhd_txn_id;
    void *rhd_buf;
};

void send_hello(struct rdmt_app_info *app, struct rdmt_peer_rq *rq);
#endif