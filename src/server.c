#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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

static void print_ep_name(struct fid_pep *ep) {
    void *addr;
    struct sockaddr_in *sin;
    size_t addrlen = 0;

    fi_getname((fid_t)ep, NULL, &addrlen);

    addr = calloc(1, addrlen);
    fi_getname((fid_t)ep, addr, &addrlen);

    sin = (struct sockaddr_in *)addr;

    printf("ep addr: %s:%d\n", inet_ntoa(sin->sin_addr), sin->sin_port);

    free(addr);
}

int init_server(struct net_info *ni) {
    int rc;

    ni->server = calloc(1, sizeof(*ni->server));
    if (!ni->server) {
        rc = -1;
        GOTO(err, "cannot allocate server_info");
    }

    rc = fi_passive_ep(ni->fabric, ni->fi, &ni->server->pep, NULL);
    if (rc < 0) {
        FI_GOTO(err1, "fi_passive_ep");
    }

    print_ep_name(ni->server->pep);

    rc = fi_pep_bind(ni->server->pep, (fid_t)ni->eq, 0);
    if (rc < 0) {
        FI_GOTO(err2, "fi_pep_bind");
    }

    rc = fi_listen(ni->server->pep);
    if (rc < 0) {
        FI_GOTO(err2, "fi_listen");
    }

    return 0;

err2:
    fi_close((fid_t)ni->server->pep);
err1:
    free(ni->server);
err:
    return rc;
}

void close_server(struct net_info *ni) {
    fi_close((fid_t)ni->server->pep);
    free(ni->server);
}

int run_server(struct net_info *ni) {
    uint32_t event;
    struct fi_eq_cm_entry cm_entry;
    struct server_connection cxn;

    struct fi_cq_attr cq_attr = {
        .format = FI_CQ_FORMAT_DATA,
        .wait_obj = FI_WAIT_NONE
    };

    int len = 4096;
    void *buf = malloc(4096);

    fi_eq_sread(ni->eq, &event, &cm_entry, sizeof cm_entry, -1, 0);
    assert(event == FI_CONNREQ);
    printf("Got event!");


    if (!cm_entry.info->domain_attr->domain) {
        fi_domain(ni->fabric, cm_entry.info, &cxn.domain, NULL);
    }

    fi_endpoint(cxn.domain, cm_entry.info, &cxn.ep, NULL);

    fi_ep_bind(cxn.ep, (fid_t)ni->eq, 0);

    // segfaults without cqs set up
    fi_cq_open(cxn.domain, &cq_attr, &cxn.cq, NULL);
    fi_ep_bind(cxn.ep, &cxn.cq->fid, FI_RECV);

    fi_enable(cxn.ep);
    fi_recv(cxn.ep, &buf, len, NULL, 0, NULL);

    fi_accept(cxn.ep, NULL, 0);
}

