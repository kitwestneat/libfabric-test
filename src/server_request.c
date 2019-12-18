#include <assert.h>
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

void bulk_recv(struct network_request *rq)
{
    fi_recv(rq->cxn->ep, rq->cxn->bulk_buf, BULK_SIZE, fi_mr_desc(get_bulk_mr()), 0, rq);
}

void bulk_send(struct network_request *rq)
{
    fi_send(rq->cxn->ep, rq->cxn->bulk_buf, BULK_SIZE, fi_mr_desc(get_bulk_mr()), 0, rq);
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
            /* // for some reason this returns -1
            if (rc )
            {
                FI_GOTO(done, "fi_cq_readerr");
            }
            */

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

        if (cqde.flags & FI_RECV || cqde.flags & FI_SEND)
        {
            struct network_request *rq = cqde.op_context;

            printf("Got message! client #%d len %d rq %p, rq->cb %p\n", cxn->client_id, cqde.len,
                   rq, rq->callback);

            if (rq->callback != NULL)
            {
                printf("running callback\n");
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
