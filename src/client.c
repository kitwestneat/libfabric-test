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

int init_client(struct net_info *ni) {
    int rc;

    ni->client = calloc(1, sizeof(*ni->client));
    if (!ni->client) {
        rc = -1;
        GOTO(err, "cannot allocate server_info");
    }

    rc = fi_domain(ni->fabric, ni->fi, &ni->client->domain, NULL);
    if (rc < 0) {
        FI_GOTO(err1, "fi_domain");
    }

    rc = fi_endpoint(ni->client->domain, ni->fi, &ni->client->ep, NULL);
    if (rc < 0) {
        FI_GOTO(err2, "fi_endpoint");
    }

    rc = fi_ep_bind(ni->client->ep, (fid_t)ni->eq, 0);
    if (rc < 0) {
        FI_GOTO(err3, "fi_ep_bind");
    }

    rc = fi_enable(ni->client->ep);

    return 0;

err3:
    fi_close((fid_t)ni->client->ep);
err2:
    fi_close((fid_t)ni->client->domain);
err1:
    free(ni->client);
err:
    return rc;
}

int run_client(struct net_info *ni, const char *addr, unsigned short port) {
    uint32_t event;
    struct fi_eq_cm_entry cm_entry;
    struct sockaddr_in sin;
    int rc;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, addr, &(sin.sin_addr));

    rc = fi_connect(ni->client->ep, &sin, NULL, 0);
    printf("fi_connect(%s:%d) = %d\n", addr, port, rc);

    fi_eq_sread(ni->eq, &event, &cm_entry, sizeof(cm_entry), -1, 0);

    printf("got event!");
}

void close_client(struct net_info *ni) {
    free((fid_t)ni->client->domain);
    free(ni->client);
}

