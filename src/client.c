#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int init_client(struct net_info *ni)
{
    return setup_connection(ni, NULL, ni->fi);
}

void wait_for_cq(struct fid_cq *cq, struct fid_wait *wait_set, int mask)
{
    struct fi_cq_data_entry buf;
    int rc;

    // fi_wait doesn't tell you if there's already a CQ waiting, so need to check before wait
    rc = fi_cq_read(cq, &buf, sizeof(buf));
    fprintf(stderr, "fi_cq_read rc=%d\n", rc);
    if (mask && !(buf.flags & mask))
    {
        rc = -1;
    }

    // wait for send cq
    while (rc <= 0)
    {
        rc = fi_wait(wait_set, 1000);
        fprintf(stderr, "fi_wait rc=%d\n", rc);

        rc = fi_cq_read(cq, &buf, sizeof(buf));
        fprintf(stderr, "fi_cq_read rc=%d\n", rc);

        if (mask && !(buf.flags & mask))
        {
            rc = -1;
        }
    }

    // printf("got a cq? %s %.*s\n", fi_tostr(&buf.flags, FI_TYPE_CQ_EVENT_FLAGS), buf.len,
    // buf.buf);
    return;
}

int connect_to_server(struct net_info *ni, const char *addr, unsigned short port)
{
    uint32_t event = 0;

    struct fi_eq_cm_entry cm_entry;
    struct sockaddr_in sin;
    int rc;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, addr, &(sin.sin_addr));

    rc = fi_connect(ni->connection_list->ep, &sin, NULL, 0);
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
            FI_GOTO(done, "fi_wait");
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

done:
    return rc;
}

int addr = 0x4000;
int get_next_addr()
{
    addr += BULK_SIZE;

    return addr;
}

int len = 1234;
int get_next_len()
{
    len = (len + 3) % BULK_SIZE;

    return len;
}

void do_cmd(struct network_request *bulk_rq, struct network_request *cmd_rq,
            enum net_cmd_type type);

void do_put(struct network_request *bulk_rq)
{
    printf("do_put()\n");
    do_cmd(bulk_rq, bulk_rq->rq_data, PUT);
}

void do_get(struct network_request *bulk_rq)
{
    printf("do_get()\n");
    do_cmd(bulk_rq, bulk_rq->rq_data, GET);
}

int cmd_count = 0;
void do_cmd(struct network_request *bulk_rq, struct network_request *cmd_rq, enum net_cmd_type type)
{
    cmd_rq->callback = NULL;
    cmd_rq->cxn->cmd_buf->type = type;

    // addr and len don't actually do anything here, just dummy data
    cmd_rq->cxn->cmd_buf->addr = get_next_addr();
    cmd_rq->cxn->cmd_buf->len = get_next_len();

    if (type == PUT)
    {
        bulk_rq->callback = do_get;
        bulk_recv(bulk_rq);
    }
    else
    {
        bulk_rq->callback = do_put;
        bulk_send(bulk_rq);
    }

    cmd_send(cmd_rq);
    cmd_count++;
}

int initial_request(struct network_request *cmd_rq, struct network_request *bulk_rq)
{
    bulk_rq->rq_data = cmd_rq;
    do_get(bulk_rq);
}

int run_client(struct net_info *ni, const char *addr, unsigned short port)
{
    struct network_request cmd_rq, bulk_rq;

    connect_to_server(ni, addr, port);

    cmd_rq.cxn = bulk_rq.cxn = ni->connection_list;

    ni->connection_list->bulk_buf = alloc_bulk_buf();
    ni->connection_list->cmd_buf = alloc_cmd_buf();

    initial_request(&cmd_rq, &bulk_rq);

    while (cmd_count < 10)
    {
        int rc = fi_wait(ni->wait_set, 1000);
        if (rc == -FI_ETIMEDOUT)
        {
            continue;
        }
        else if (rc < 0)
        {
            fprintf(stderr, "Error waiting: %d\n", rc);
            continue;
        }

        process_cq_events(ni->connection_list);
    }
}

void close_client(struct net_info *ni)
{
    fi_close((fid_t)ni->connection_list->cq);
    fi_close((fid_t)ni->connection_list->ep);
    free(ni->connection_list);
}
