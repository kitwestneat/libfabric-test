#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>

#include "log.h"
#include "mem.h"
#include "network.h"

static void print_long_info(struct fi_info *info)
{
    struct fi_info *cur;
    for (cur = info; cur; cur = cur->next)
    {
        printf(fi_tostr(cur, FI_TYPE_INFO));
    }
}

static void print_short_info(struct fi_info *info)
{
    struct fi_info *cur;
    for (cur = info; cur; cur = cur->next)
    {
        printf("provider: %s\n", cur->fabric_attr->prov_name);
        printf("    fabric: %s\n", cur->fabric_attr->name),
            printf("    domain: %s\n", cur->domain_attr->name),
            printf("    version: %d.%d\n", FI_MAJOR(cur->fabric_attr->prov_version),
                   FI_MINOR(cur->fabric_attr->prov_version));
        printf("    type: %s\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
        printf("    protocol: %s\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
        printf("    addr format: %s\n", fi_tostr(&cur->addr_format, FI_TYPE_ADDR_FORMAT));
    }
}

struct fi_info *get_fi(bool is_source)
{
    struct fi_info *fi, *hints;
    int rc;

    hints = fi_allocinfo();
    hints->mode = FI_LOCAL_MR;
    hints->caps = FI_RMA;
    // hints->ep_attr->type = FI_EP_RDM;
    hints->ep_attr->type = FI_EP_MSG;
    // hints->fabric_attr->prov_name = strdup("tcp");
    hints->fabric_attr->prov_name = strdup("sockets");

    rc = fi_getinfo(FI_VERSION(1, 4), "127.0.0.1", "1701", is_source ? FI_SOURCE : 0, hints, &fi);

    fi_freeinfo(hints);

    if (rc)
    {
        GOTO(err, "cannot get fabric info");
    }

    return fi;

err:
    return NULL;
}

void free_fi(struct fi_info *info)
{
    fi_freeinfo(info);
}

int init_network(struct net_info *ni, bool is_server)
{
    struct fi_wait_attr wait_attr = {.wait_obj = FI_WAIT_UNSPEC};
    struct fi_eq_attr eq_attr = {
        .wait_obj = FI_WAIT_SET,
    };
    int rc = 0;

    ni->fi = get_fi(is_server);
    if (!ni->fi)
    {
        rc = -1;
        GOTO(err, "error calling get_fi");
    }

    print_short_info(ni->fi);

    rc = fi_fabric(ni->fi->fabric_attr, &ni->fabric, NULL);
    if (rc < 0)
    {
        FI_GOTO(err1, "fi_fabric");
    }

    rc = fi_wait_open(ni->fabric, &wait_attr, &ni->wait_set);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_wait_open");
    }

    eq_attr.wait_set = ni->wait_set;

    printf("opening eq\n");
    rc = fi_eq_open(ni->fabric, &eq_attr, &ni->eq, NULL);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_eq_open");
    }

    rc = fi_domain(ni->fabric, ni->fi, &ni->domain, NULL);
    if (rc < 0)
    {
        FI_GOTO(err3, "fi_domain");
    }

    init_memory(ni);

    ni->connection_list = NULL;

    return 0;

err3:
    fi_close((fid_t)ni->eq);
err2:
    fi_close((fid_t)ni->fabric);
err1:
    free_fi(ni->fi);
err:
    return rc;
}

void close_network(struct net_info *ni)
{
    close_memory(ni);

    fi_close((fid_t)ni->domain);
    fi_close((fid_t)ni->eq);
    fi_close((fid_t)ni->wait_set);
    fi_close((fid_t)ni->fabric);
    free_fi(ni->fi);
}

int next_client_id = 0;
int get_client_id()
{
    return next_client_id++;
}

int setup_connection(struct net_info *ni, struct connection **cxn_ptr, struct fi_info *info)
{
    struct fi_cq_attr cq_attr = {
        .format = FI_CQ_FORMAT_DATA, .wait_obj = FI_WAIT_SET, .wait_set = ni->wait_set};
    struct connection *cxn = calloc(1, sizeof(struct connection));
    int rc = 0;

    if (cxn_ptr != NULL)
    {
        *cxn_ptr = cxn;
    }

    cxn->client_id = get_client_id();

    printf("add_connection %d\n", cxn->client_id);

    cxn->bulk_buf = alloc_bulk_buf();
    cxn->cmd_buf = alloc_cmd_buf();

    rc = fi_endpoint(ni->domain, info, &cxn->ep, NULL);
    if (rc)
    {
        FI_GOTO(err, "fi_endpoint");
    }

    rc = fi_ep_bind(cxn->ep, (fid_t)ni->eq, 0);
    if (rc)
    {
        FI_GOTO(err1, "fi_ep_bind");
    }

    // segfaults without cqs set up
    rc = fi_cq_open(ni->domain, &cq_attr, &cxn->cq, NULL);
    if (rc)
    {
        FI_GOTO(err1, "fi_cq_open");
    }

    // if these flags are wrong, this will silently fail
    rc = fi_ep_bind(cxn->ep, &cxn->cq->fid, FI_RECV | FI_TRANSMIT);
    if (rc)
    {
        FI_GOTO(err2, "fi_ep_bind");
    }

    printf("enabling endpoint\n");
    rc = fi_enable(cxn->ep);
    if (rc)
    {
        FI_GOTO(err2, "fi_enable");
    }

    // XXX lock?
    cxn->next = ni->connection_list;
    ni->connection_list = cxn;

    return 0;

err2:
    fi_close((fid_t)cxn->cq);
err1:
    fi_close((fid_t)cxn->ep);

err:

    free_bulk_buf(cxn->bulk_buf);
    free_cmd_buf(cxn->cmd_buf);
    free(cxn);

    return rc;
}