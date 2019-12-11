#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rdma/fabric.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>

#include "log.h"
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
    //hints->ep_attr->type = FI_EP_RDM;
    hints->ep_attr->type = FI_EP_MSG;
    //hints->fabric_attr->prov_name = strdup("tcp");
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
    struct fi_wait_attr wait_attr = {
        .wait_obj = FI_WAIT_UNSPEC};
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

    rc = fi_eq_open(ni->fabric, &eq_attr, &ni->eq, NULL);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_eq_open");
    }

    return 0;

err2:
    fi_close((fid_t)ni->fabric);
err1:
    free_fi(ni->fi);
err:
    return rc;
}

void close_network(struct net_info *ni)
{
    fi_close((fid_t)ni->eq);
    fi_close((fid_t)ni->wait_set);
    fi_close((fid_t)ni->fabric);
    free_fi(ni->fi);
}
