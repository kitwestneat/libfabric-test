#include "rdmt.h"

#define HELLO_COUNT 10

int hello_sender(struct rdmt_app_info *app, struct rdmt_peer_rq *rq)
{
    struct rdmt_hello_data *hello_data = rq->next_data;

    int i = hello_data->rhd_txn_id++;
    if (i >= HELLO_COUNT)
    {
        return 1;
    }

    snprintf(hello_data->rhd_buf, "Client Hello World %d\n", i);

    send_hello(app, rq);

    return 0;
}

int run_client(struct rdmt_app_info *app, char *addr, char *port)
{
    struct rdmt_hello_data hello_data;
    struct rdmt_peer_rq rq;

    struct rdmt_peer *peer;

    int rc = add_peer(app, &peer, addr, port);
    rc = new_hello_buf(app, &hello_data.rhd_buf);

    rq.next_data = &hello_data;
    rq.next_func = hello_sender;
    rq.peer = peer;

    run_app(app);

    free_hello_buf(app, hello_data.rhd_buf);
    free_peer(app, peer);
}