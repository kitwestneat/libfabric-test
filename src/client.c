#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int init_client(struct net_info *ni)
{
    struct fi_cq_attr cq_attr = {
        .format = FI_CQ_FORMAT_DATA,
        .wait_obj = FI_WAIT_UNSPEC,
    };
    int rc;

    ni->client = calloc(1, sizeof(*ni->client));
    if (!ni->client)
    {
        rc = -1;
        GOTO(err, "cannot allocate server_info");
    }

    rc = fi_domain(ni->fabric, ni->fi, &ni->client->domain, NULL);
    if (rc < 0)
    {
        FI_GOTO(err1, "fi_domain");
    }

    rc = fi_endpoint(ni->client->domain, ni->fi, &ni->client->ep, NULL);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_endpoint");
    }

    rc = fi_ep_bind(ni->client->ep, (fid_t)ni->eq, 0);
    if (rc < 0)
    {
        FI_GOTO(err3, "fi_ep_bind");
    }

    rc = fi_cq_open(ni->client->domain, &cq_attr, &ni->client->cq, NULL);
    if (rc < 0)
    {
        FI_GOTO(err3, "fi_cq_close");
    }

    rc = fi_ep_bind(ni->client->ep, &ni->client->cq->fid, FI_RECV | FI_SEND);
    if (rc < 0)
    {
        FI_GOTO(err4, "fi_ep_bind");
    }

    rc = fi_enable(ni->client->ep);
    if (rc < 0)
    {
        FI_GOTO(err4, "fi_enable");
    }

    return 0;

err4:
    fi_close((fid_t)ni->client->cq);
err3:
    fi_close((fid_t)ni->client->ep);
err2:
    fi_close((fid_t)ni->client->domain);
err1:
    free(ni->client);
err:
    return rc;
}

int run_client(struct net_info *ni, const char *addr, unsigned short port)
{
    uint32_t event = 0;
    struct fi_eq_cm_entry cm_entry;
    struct sockaddr_in sin;
    int rc;
    char buf[] = "Hello world XX!";
    struct fi_cq_data_entry buf2;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, addr, &(sin.sin_addr));

    rc = fi_connect(ni->client->ep, &sin, NULL, 0);
    printf("fi_connect(%s:%d) = %d\n", addr, port, rc);
    if (rc != 0)
    {
        fprintf(stderr, "Error connecting: %d\n", rc);
        return rc;
    }

    // wait for connect event
    do
    {
        rc = fi_wait(ni->wait_set, 1000);
        if (rc < 0)
        {
            fprintf(stderr, "fi_wait rc=%d\n", rc);
            return rc;
        }

        rc = fi_eq_read(ni->eq, &event, &cm_entry, sizeof(cm_entry), 0);
        if (rc == -FI_EAGAIN)
        {
            fprintf(stderr, "timeout waiting for event\n");
        }
        else if (rc < 0)
        {
            fprintf(stderr, "fi_eq_read rc=%d\n", rc);
            return rc;
        }
    } while (rc == -FI_EAGAIN);
    fprintf(stderr, "got event: %d - %s\n", event, fi_tostr(&event, FI_TYPE_EQ_EVENT));

    for (int i = 0; i < 10; i++)
    {
        sprintf(buf, "Hello World %02d!", i);

        rc = fi_send(ni->client->ep, &buf, sizeof(buf), NULL, (fi_addr_t)NULL, NULL);
        fprintf(stderr, "fi_send rc=%d\n", rc);

        /* // XXX commented out because we don't wait for send events
        do
        {
            rc = fi_wait(ni->wait_set, 1000);
            fprintf(stderr, "fi_wait rc=%d\n", rc);
        } while (rc != FI_SUCCESS);
        fi_cq_read(ni->client->cq, &buf2, sizeof(buf2));
        fprintf(stderr, "fi_cq_read rc=%d\n", rc);

        printf("got a cq? %s %.*s\n", fi_tostr(&buf2.flags, FI_TYPE_CQ_EVENT_FLAGS), buf2.len, buf2.buf);
        */

        sleep(1);
    }
}

void close_client(struct net_info *ni)
{
    fi_close((fid_t)ni->client->domain);
    fi_close((fid_t)ni->client->cq);
    fi_close((fid_t)ni->client->ep);
    free(ni->client);
}
