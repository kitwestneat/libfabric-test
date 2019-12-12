#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

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

    ni->server = calloc(1, sizeof(*ni->server));
    if (!ni->server)
    {
        rc = -1;
        GOTO(err, "cannot allocate server_info");
    }

    rc = fi_passive_ep(ni->fabric, ni->fi, &ni->server->pep, NULL);
    if (rc < 0)
    {
        FI_GOTO(err1, "fi_passive_ep");
    }

    print_ep_name(ni->server->pep);

    rc = fi_pep_bind(ni->server->pep, (fid_t)ni->eq, 0);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_pep_bind");
    }

    rc = fi_listen(ni->server->pep);
    if (rc < 0)
    {
        FI_GOTO(err2, "fi_listen");
    }

    ni->server->connection_list = NULL;

    return 0;

err2:
    fi_close((fid_t)ni->server->pep);
err1:
    free(ni->server);
err:
    return rc;
}

void close_connection(struct server_connection *cxn)
{
    fi_close((fid_t)cxn->cq);
    fi_close((fid_t)cxn->ep);
    fi_close((fid_t)cxn->domain);
    if (cxn->read_buf)
        free(cxn->read_buf);
}

void close_server(struct net_info *ni)
{
    while (ni->server->connection_list)
    {
        struct server_connection *cxn = ni->server->connection_list;
        ni->server->connection_list = cxn->next;

        close_connection(cxn);
        free(cxn);
    }

    fi_close((fid_t)ni->server->pep);
    free(ni->server);
}

int add_connection(struct net_info *ni, struct fi_eq_cm_entry *cm_entry)
{
    struct fi_cq_attr cq_attr = {
        .format = FI_CQ_FORMAT_DATA,
        .wait_obj = FI_WAIT_SET,
        .wait_set = ni->wait_set};
    int len = 4096;
    struct server_connection *cxn = calloc(1, sizeof(struct server_connection));
    int rc = 0;
    bool domain_init = 0;

    printf("add_connection\n");

    cxn->read_buf = malloc(4096);
    if (!cxn->read_buf)
    {
        fprintf(stderr, "cannot allocate read_buf");
    }
    else
    {
        printf("read_buf %p\n", cxn->read_buf);
    }

    if (!cm_entry->info->domain_attr->domain)
    {
        rc = fi_domain(ni->fabric, cm_entry->info, &cxn->domain, NULL);
        if (rc)
        {
            FI_GOTO(err, "fi_domain");
        }
        domain_init = 1;
    }

    rc = fi_endpoint(cxn->domain, cm_entry->info, &cxn->ep, NULL);
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
    rc = fi_cq_open(cxn->domain, &cq_attr, &cxn->cq, NULL);
    if (rc)
    {
        FI_GOTO(err1, "fi_cq_open");
    }

    // if these flags are wrong, this will silently fail
    fi_ep_bind(cxn->ep, &cxn->cq->fid, FI_RECV);
    if (rc)
    {
        FI_GOTO(err2, "fi_ep_bind");
    }

    fi_enable(cxn->ep);

    // libfabric doesn't give us an event notification for the client send unless there's a buffer posted
    printf("posted buffer!\n");
    fi_recv(cxn->ep, cxn->read_buf, 4096, NULL, 0, NULL);

    fi_accept(cxn->ep, NULL, 0);

    // XXX lock?
    cxn->next = ni->server->connection_list;
    ni->server->connection_list = cxn;

    return 0;

err2:
    fi_close((fid_t)cxn->cq);
err1:
    fi_close((fid_t)cxn->ep);

err:
    if (domain_init)
    {
        fi_close((fid_t)cxn->domain);
    }

    free(cxn->read_buf);
    free(cxn);

    return rc;
}

int del_connection(struct net_info *ni, struct fi_eq_cm_entry *cm_entry)
{
    struct server_connection *cxn = ni->server->connection_list;
    if ((fid_t)cxn->ep == cm_entry->fid)
    {
        ni->server->connection_list = NULL;
        close_connection(cxn);
        free(cxn);
    }
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

void process_cq_events(struct net_info *ni)
{
    struct server_connection *cxn = ni->server->connection_list;
    struct fi_cq_data_entry cqde;
    int rc;

    if (!cxn)
    {
        return;
    }

    do
    {
        struct fi_cq_err_entry err_entry;
        rc = fi_cq_readerr(cxn->cq, &err_entry, 0);
        printf("fi_cq_readerr rc=%d err_entry.err: %d\n", rc, err_entry.err);
        rc = fi_cq_read(cxn->cq, &cqde, 1);
        printf("fi_cq_read=%d\n", rc);
        if (rc == -FI_EAGAIN)
        {
            return;
        }
        else if (rc < 0)
        {
            fprintf(stderr, "got error trying to read cq event: %d\n", rc);
            return;
        }

        if (cqde.flags & FI_RECV)
        {
            printf("Got message! %*s\n", cqde.len, cqde.buf);
            fi_recv(cxn->ep, cxn->read_buf, 4096, NULL, 0, NULL);
        }
        else
        {
            fprintf(stderr, "unknown cq flags: %d - %s\n", cqde.flags, fi_tostr(&cqde.flags, FI_TYPE_CQ_EVENT_FLAGS));
            fprintf(stderr, "buf: %*s\n", cqde.len - 1, cqde.buf);
            fprintf(stderr, "data: %s\n", cqde.data);
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
        process_cq_events(ni);
    }
}
