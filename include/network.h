#ifndef NETWORK_H
#define NETWORK_H

struct net_info
{
};

struct net_peer_data
{
};

enum net_cmd_type
{
    NCT_GET = 0,
    NCT_PUT
};

struct net_cmd
{
    int nc_txn_id;
    enum net_cmd_type nc_type;
    void *nc_bulk_buf;
};

int net_wait(struct net_info *net);

#endif