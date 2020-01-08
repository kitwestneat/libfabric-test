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

int cmd_count = 0;
void do_cmd(struct network_request *cmd_rq, enum net_cmd_type type);

void do_put(struct network_request *cmd_rq)
{
    printf("do_put()\n");
    do_cmd(cmd_rq, PUT);
}

void do_get(struct network_request *cmd_rq)
{
    printf("do_get()\n");
    sprintf(cmd_rq->cxn->bulk_buf, "This is client speaking, cmd_count = %d\n", cmd_count);
    do_cmd(cmd_rq, GET);
}

void print_put(struct network_request *rq)
{
    printf("received PUT from server: %s\n", rq->cxn->bulk_buf);

    do_get(rq);
}

void wait_for_complete(struct network_request *rq)
{
    if (rq->cxn->cmd_buf->type == PUT)
    {
        rq->callback = print_put;
    }
    else
    {
        rq->callback = do_put;
    }

    cmd_recv(rq);
}

void do_cmd(struct network_request *cmd_rq, enum net_cmd_type type)
{
    cmd_rq->callback = wait_for_complete;

    struct network_cmd *cmd = cmd_rq->cxn->cmd_buf;
    cmd->type = type;

    // op_addr doesn't actually do anything here, just dummy data
    cmd->op_addr = get_next_addr();

    cmd_send(cmd_rq);
    cmd_count++;
}

int initial_request(struct network_request *cmd_rq)
{
    do_get(cmd_rq);
}

int run_client(struct net_info *ni, const char *addr, unsigned short port)
{
    struct network_request cmd_rq;

    connect_to_server(ni, addr, port);

    cmd_rq.cxn = ni->connection_list;

    ni->connection_list->bulk_buf = alloc_bulk_buf();
    ni->connection_list->cmd_buf = alloc_cmd_buf();

    struct network_cmd *cmd = cmd_rq.cxn->cmd_buf;

    // set up rma
    cmd->rma_iov.addr = get_bulk_offset(cmd_rq.cxn->bulk_buf);
    cmd->rma_iov.len = BULK_SIZE;
    cmd->rma_iov.key = fi_mr_key(get_bulk_mr());

    cmd->rma.rma_iov_count = 1;

    initial_request(&cmd_rq);

    while (cmd_count < 10)
    {
        int rc = fi_wait(ni->wait_set, 1000);
        if (rc == -FI_ETIMEDOUT)
        {
            printf("wait timeout\n");
            continue;
        }
        else if (rc < 0)
        {
            fprintf(stderr, "Error waiting: %d\n", rc);
            continue;
        }

        printf("got event\n");
        process_cq_events(ni->connection_list);
    }
}

void close_client(struct net_info *ni)
{
    fi_close((fid_t)ni->connection_list->cq);
    fi_close((fid_t)ni->connection_list->ep);

    free_cmd_buf(ni->connection_list->cmd_buf);

    free_bulk_buf(ni->connection_list->bulk_buf);
    free(ni->connection_list);
}