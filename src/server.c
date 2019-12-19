#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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

void process_cmd(struct network_request *rq);

static void print_ep_name(struct fid_pep *ep)
{
    void *addr;
    struct sockaddr_in *sin;
    size_t addrlen = 0;

    fi_getname((fid_t)ep, NULL, &addrlen);
    if (!addrlen)
    {
        fprintf(stderr, "unable to getname\n");
        return;
    }

    addr = calloc(1, addrlen);
    fi_getname((fid_t)ep, addr, &addrlen);

    sin = (struct sockaddr_in *)addr;

    printf("ep addr: %s:%d\n", inet_ntoa(sin->sin_addr), sin->sin_port);

    free(addr);
}

int init_server(struct net_info *ni)
{
    int rc;

    rc = fi_passive_ep(ni->fabric, ni->fi, &ni->pep, NULL);
    if (rc < 0)
    {
        FI_GOTO(err1, "fi_passive_ep");
    }

    print_ep_name(ni->pep);

    rc = fi_pep_bind(ni->pep, (fid_t)ni->eq, 0);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_pep_bind");
    }

    rc = fi_listen(ni->pep);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_listen");
    }

    return 0;

err2:
    fi_close((fid_t)ni->pep);
err1:
    fi_close((fid_t)ni->domain);
err:
    return rc;
}

void close_connection(struct connection *cxn)
{
    // tcp provider doesn't like having the cq closed before the ep
    fi_close((fid_t)cxn->ep);
    fi_close((fid_t)cxn->cq);

    free_bulk_buf(cxn->bulk_buf);
    free_cmd_buf(cxn->cmd_buf);
}

void close_server(struct net_info *ni)
{
    while (ni->connection_list)
    {
        struct connection *cxn = ni->connection_list;
        ni->connection_list = cxn->next;

        close_connection(cxn);
        free(cxn);
    }

    fi_close((fid_t)ni->pep);
}

int add_connection(struct net_info *ni, struct fi_eq_cm_entry *cm_entry)
{

    if (!cm_entry->info->domain_attr->domain && ni->domain)
    {
        fprintf(stderr, "cm entry has no domain set, but ni has domain set\n");
    }

    if (cm_entry->info->domain_attr->domain != ni->domain)
    {
        fprintf(stderr, "cm entry domain does not equal ni domain\n");
    }

    struct connection *cxn;

    int rc = setup_connection(ni, &cxn, cm_entry->info);
    if (rc < 0)
    {
        FI_GOTO(done, "setup_connection");
    }

    // XXX this should probably be stored in the connection so we can free it on disconnect
    struct network_request *rq = malloc(sizeof(struct network_request));
    rq->cxn = cxn;
    rq->callback = process_cmd;

    // libfabric doesn't give us an event notification for the client send unless there's a buffer
    // posted
    cmd_recv(rq);

    printf("accepting\n");
    rc = fi_accept(cxn->ep, NULL, 0);
    if (rc < 0)
    {
        FI_GOTO(done, "fi_accept");
    }

done:
    return rc;
}

int del_connection(struct net_info *ni, struct fi_eq_cm_entry *cm_entry)
{
    struct connection *cxn = ni->connection_list;
    struct connection **cxn_ptr = &ni->connection_list;
    while (cxn)
    {
        if ((fid_t)cxn->ep == cm_entry->fid)
        {
            printf("deleting client %d\n", cxn->client_id);
            *cxn_ptr = cxn->next;
            close_connection(cxn);
            free(cxn);

            return 0;
        }
        else
        {
            cxn_ptr = &cxn->next;
            cxn = cxn->next;
        }
    }

    return -ENOENT;
}

void process_eq_events(struct net_info *ni)
{
    uint32_t event;
    struct fi_eq_cm_entry cm_entry;
    int rc;

    do
    {
        rc = fi_eq_read(ni->eq, &event, &cm_entry, sizeof cm_entry, 0);
        if (rc == -FI_EAGAIN)
        {
            return;
        }
        else if (rc == -FI_EAVAIL)
        {
            struct fi_eq_err_entry eqee;

            rc = fi_eq_readerr(ni->eq, &eqee, 0);

            if (rc < 0)
            {
                fprintf(stderr, "warning - fi_cq_readerr: rc=%d\n", rc);
            }

            fprintf(stderr, "CM error detected: %s [%d]\n",
                    fi_eq_strerror(ni->eq, eqee.prov_errno, eqee.err_data, NULL, 0), eqee.err);
            return;
        }
        else if (rc < 0)
        {
            fprintf(stderr, "got error trying to read eq event: %d\n", rc);
            return;
        }

        switch (event)
        {
        case FI_CONNREQ:
            printf("Connecting...\n");
            add_connection(ni, &cm_entry);
            break;
        case FI_CONNECTED:
            printf("Connected\n");
            break;
        case FI_SHUTDOWN:
            printf("Disconnected\n");
            del_connection(ni, &cm_entry);
            break;
        default:
            fprintf(stderr, "unknown event: %d - %s\n", event, fi_tostr(&event, FI_TYPE_EQ_EVENT));
            break;
        }
    } while (rc != 0);
}

bool keep_running = 1;
void handle_sigint()
{
    keep_running = 0;
}

int run_server(struct net_info *ni)
{
    signal(SIGINT, handle_sigint);
    while (keep_running)
    {
        int rc;

        rc = fi_wait(ni->wait_set, 1000);

        if (rc == -FI_ETIMEDOUT)
        {
            continue;
        }
        else if (rc < 0)
        {
            fprintf(stderr, "Error waiting: %d\n", rc);
            continue;
        }

        printf("Got event\n");
        process_eq_events(ni);
        process_all_cq_events(ni);
    }
}

void send_complete(struct network_request *rq)
{
    rq->callback = process_cmd;
    cmd_recv(rq);
}

void finish_get_cmd(struct network_request *rq)
{
    printf("finish_get_cmd: received %s\n", rq->cxn->bulk_buf);

    rq->callback = send_complete;
    cmd_send(rq);
}

void finish_put_cmd(struct network_request *rq)
{
    printf("finish_put_cmd: sent data\n");

    rq->callback = send_complete;
    cmd_send(rq);
}

void process_cmd(struct network_request *rq)
{
    struct network_cmd *cmd = rq->cxn->cmd_buf;
    cmd->rma.rma_iov = &cmd->rma_iov;
    cmd->rma.rma_iov_count = 1;

    printf("process_cmd, type %d\n", cmd->type);
    if (cmd->type == GET)
    {
        rq->callback = finish_get_cmd;
        bulk_read(rq, &cmd->rma);
    }
    else
    {
        sprintf(rq->cxn->bulk_buf, "Hello world from the server, addr: %x\n", cmd->op_addr);
        rq->callback = finish_put_cmd;
        bulk_write(rq, &cmd->rma);
    }
}
