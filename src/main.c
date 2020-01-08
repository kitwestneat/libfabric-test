#include <stdbool.h>
#include <stdio.h>

#include "network.h"
#include "rdmt.h"

int run_app(struct rdmt_app_info *app)
{
    struct rdmt_peer_rq *rq;

    while (rq = net_wait_for_rq(app->net))
    {
        rq->next_func(app, rq);
    }
}

int main(int argc, const char **argv)
{
    struct net_info net;
    bool is_server = argc > 1;
    int rc;

    rc = init_network(&net, is_server);
    if (rc < 0)
    {
        fprintf(stderr, "Unable to initialize fabric: rc=%d\n", rc);

        return rc;
    }

    if (is_server)
    {
        rc = init_server(&net);
        if (rc < 0)
        {
            fprintf(stderr, "Unable to initialize server: rc=%d\n", rc);

            return rc;
        }

        run_server(&net);
        fprintf(stderr, "Closing server...\n");
        close_server(&net);
    }
    else
    {
        rc = init_client(&net);
        if (rc < 0)
        {
            fprintf(stderr, "Unable to initialize client: rc=%d\n", rc);

            return rc;
        }

        run_client(&net, "127.0.0.1", 1701);
        close_client(&net);
    }

    close_network(&net);

    return 0;
}
