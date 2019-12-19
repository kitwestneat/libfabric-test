#include <assert.h>
#include <rdma/fi_rma.h>

#include <rdma/fi_endpoint.h>
#include <stdio.h>

#include "log.h"
#include "mem.h"
#include "network.h"

void cmd_recv(struct network_request *rq)
{
    fi_recv(rq->cxn->ep, rq->cxn->cmd_buf, sizeof(struct network_cmd), fi_mr_desc(get_cmd_mr()), 0,
            rq);
}

void cmd_send(struct network_request *rq)
{
    fi_send(rq->cxn->ep, rq->cxn->cmd_buf, sizeof(struct network_cmd), fi_mr_desc(get_cmd_mr()), 0,
            rq);
}

/*
"By default, the remote endpoint does not generate an event or notify the user when a memory
region has been accessed by an RMA read or write operation.  However, immediate data may be
associated with an RMA write operation.  RMA writes with immediate data will generate a
completion entry at the remote endpoint, so that the immediate data may be delivered."

I haven't been able to get this to work with the sockets provider.
*/
void bulk_op(struct network_request *rq, struct fi_msg_rma *msg, bool is_read)
{
    struct iovec iov = {
        .iov_base = rq->cxn->bulk_buf,
        .iov_len = BULK_SIZE,
    };
    void *desc = fi_mr_desc(get_bulk_mr());

    msg->msg_iov = &iov;
    msg->desc = &desc;
    msg->iov_count = 1;
    msg->context = rq;

    if (is_read)
    {
        fi_readmsg(rq->cxn->ep, msg, 0);
    }
    else
    {
        fi_writemsg(rq->cxn->ep, msg, 0);
    }
}

void bulk_read(struct network_request *rq, struct fi_msg_rma *msg)
{
    bulk_op(rq, msg, 1);
}

void bulk_write(struct network_request *rq, struct fi_msg_rma *msg)
{
    bulk_op(rq, msg, 0);
}

void process_cq_events(struct connection *cxn)
{
    struct fi_cq_data_entry cqde;
    int rc;

    cqde.op_context = NULL;

    if (!cxn)
    {
        return;
    }

    do
    {
        rc = fi_cq_read(cxn->cq, &cqde, 1);
        if (rc == -FI_EAGAIN)
        {
            return;
        }
        else if (rc == -FI_EAVAIL)
        {
            struct fi_cq_err_entry cqee;
            rc = fi_cq_readerr(cxn->cq, &cqee, 0);
            if (rc < 0)
            {
                fprintf(stderr, "warning - fi_cq_readerr: rc=%d\n", rc);
            }

            struct network_request *rq = cqee.op_context;
            rq->rq_res = cqee.err;

            fprintf(stderr, "Request error detected: %s [%d]\n",
                    fi_cq_strerror(cxn->cq, cqee.prov_errno, cqee.err_data, NULL, 0), cqee.err);
            return;
        }
        else if (rc < 0)
        {
            FI_GOTO(done, "fi_cq_read");
        }

        if (cqde.flags & (FI_RECV | FI_SEND | FI_READ | FI_WRITE))
        {
            struct network_request *rq = cqde.op_context;

            printf("message - client #%d len %d rq %p\n", cxn->client_id, cqde.len, rq);
            fprintf(stderr, "cq flags: %d - %s\n", cqde.flags,
                    fi_tostr(&cqde.flags, FI_TYPE_CQ_EVENT_FLAGS));

            if (rq && rq->callback != NULL)
            {
                printf("running callback, cb=%p\n", rq->callback);
                rq->callback(rq);
            }
            else
            {
                printf("request done\n");
            }
        }
        else
        {
            fprintf(stderr, "unknown cq flags: %d - %s\n", cqde.flags,
                    fi_tostr(&cqde.flags, FI_TYPE_CQ_EVENT_FLAGS));
            fprintf(stderr, "buf: %*s\n", cqde.len - 1, cqde.buf);
            fprintf(stderr, "data: %s\n", cqde.data);
            break;
        }
    } while (rc != 0);

done:
    return;
}

void process_all_cq_events(struct net_info *ni)
{
    struct connection *cxn = ni->connection_list;
    while (cxn)
    {
        process_cq_events(cxn);
        cxn = cxn->next;
    }
}
