#include <stdbool.h>
#include <stdio.h>

#include "mem.h"
#include "network.h"

int main(int argc, const char **argv)
{
    struct net_info net;
    bool is_server = argc > 1;
    int rc;

    rc = init_network(&net, is_server);
    if (rc < 0)
    {
        fprintf(stderr, "Unable to initialize fabric");

        return rc;
    }

    if (is_server)
    {
        rc = init_server(&net);
        if (rc < 0)
        {
            fprintf(stderr, "Unable to initialize server");

            return rc;
        }

        run_server(&net);
        fprintf(stderr, "Closing server...\n");
        close_server(&net);
    }
    else
    {
        init_client(&net);
        if (rc < 0)
        {
            fprintf(stderr, "Unable to initialize client");

            return rc;
        }

        run_client(&net, "127.0.0.1", 1701);
        close_client(&net);
    }

    close_network(&net);

    return 0;
}
