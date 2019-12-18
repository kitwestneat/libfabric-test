#ifndef MEM_H
#define MEM_H

#include "network.h"
#include <stdint.h>

#define MAX_CONNECTIONS 16
#define BULK_SIZE 4096

int init_memory(struct net_info *ni);

void *alloc_bulk_buf();
void *alloc_cmd_buf();

void free_bulk_buf(void *buf);
void free_cmd_buf(void *buf);

int close_memory(struct net_info *ni);

struct fid_mr *get_bulk_mr();
struct fid_mr *get_cmd_mr();
#endif