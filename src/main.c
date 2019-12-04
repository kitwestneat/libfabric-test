#include <stdbool.h>

#include "network.h"

int main(int argc, const char **argv) {
    struct net_info net;
    bool is_server = argc > 1;

    init_network(&net, is_server);

    if (is_server) {
        init_server(&net);
        run_server(&net);
        close_server(&net);
    } else {
        init_client(&net);
        run_client(&net, "127.0.0.1", 1701);
        close_client(&net);
    }

    close_network(&net);
}
