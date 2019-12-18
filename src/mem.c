#include "mem.h"
#include "log.h"
#include "network.h"
#include <rdma/fi_domain.h>
#include <stdlib.h>
#include <string.h>

// fi_mr_reg require that requested key be different for each region
#define BULK_KEY 0
#define CMD_KEY 1

#define GET_BIT(bmap, pos) ((bmap) & (1 << ((pos) % 8)))
#define SET_BIT(bmap, pos) ((bmap) | (1 << ((pos) % 8)))
#define CLR_BIT(bmap, pos) ((bmap) & ~(1 << ((pos) % 8)))

uint8_t bulk_free_bitmap[MAX_CONNECTIONS];
uint8_t cmd_free_bitmap[MAX_CONNECTIONS];

void *bulk_bufs = NULL;
void *cmd_bufs = NULL;

struct fid_mr *bulk_mr;
struct fid_mr *cmd_mr;

int init_memory(struct net_info *ni)
{
    int rc;

    size_t cmd_buf_size = (sizeof(struct network_cmd) * MAX_CONNECTIONS);

    bulk_bufs = calloc(MAX_CONNECTIONS, BULK_SIZE);
    cmd_bufs = calloc(MAX_CONNECTIONS, sizeof(struct network_cmd));

    memset(bulk_free_bitmap, 0, MAX_CONNECTIONS);
    memset(cmd_free_bitmap, 0, MAX_CONNECTIONS);

    rc = fi_mr_reg(ni->domain, bulk_bufs, BULK_SIZE * MAX_CONNECTIONS,
                   FI_SEND | FI_RECV | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE, 0, 0,
                   0, &bulk_mr, NULL);
    if (rc < 0)
    {
        FI_GOTO(err, "fi_mr_reg");
    }

    rc = fi_mr_reg(ni->domain, cmd_bufs, cmd_buf_size,
                   FI_SEND | FI_RECV | FI_READ | FI_WRITE | FI_REMOTE_READ | FI_REMOTE_WRITE, 0, 1,
                   0, &cmd_mr, NULL);
    if (rc < 0)
    {
        FI_GOTO(err, "fi_mr_reg");
    }

    ni->local_keys.magic = MAGIC;
    ni->local_keys.bulk_key = fi_mr_key(bulk_mr);
    ni->local_keys.cmd_key = fi_mr_key(cmd_mr);

    return 0;

err:
    free(bulk_bufs);
    free(cmd_bufs);

    return rc;
}

int close_memory(struct net_info *ni)
{
    fi_close((fid_t)bulk_mr);
    fi_close((fid_t)cmd_mr);
    free(bulk_bufs);
    free(cmd_bufs);
}

static int get_free_index(uint8_t *bitmap)
{
    for (int i = 0; i < MAX_CONNECTIONS / 8; i++)
    {
        if (!~bitmap[0])
        {
            continue;
        }
        for (int j = 0; j < 8; j++)
        {
            if (GET_BIT(bitmap[i], j) == 0)
            {
                SET_BIT(bitmap[i], j);

                return i * 8 + j;
            }
        }
    }

    return -1;
}

void *alloc_bulk_buf()
{
    int index = get_free_index(bulk_free_bitmap);
    if (index == -1)
    {
        return NULL;
    }

    return bulk_bufs + index * BULK_SIZE;
}

void *alloc_cmd_buf()
{
    int index = get_free_index(cmd_free_bitmap);
    if (index == -1)
    {
        return NULL;
    }

    return cmd_bufs + index * sizeof(struct network_cmd);
}

void free_cmd_buf(void *buf)
{
    int index = (buf - cmd_bufs) / sizeof(struct network_cmd);

    CLR_BIT(cmd_free_bitmap[index / 8], index % 8);
}

void free_bulk_buf(void *buf)
{
    int index = (buf - bulk_bufs) / BULK_SIZE;

    CLR_BIT(bulk_free_bitmap[index / 8], index % 8);
}

bool is_bulk_buf(void *buf)
{
    if (buf < bulk_bufs)
    {
        return false;
    }

    return (buf - bulk_bufs) < BULK_SIZE * MAX_CONNECTIONS;
}

bool is_cmd_buf(void *buf)
{
    if (buf < cmd_bufs)
    {
        return false;
    }

    return (buf - cmd_bufs) < sizeof(struct network_cmd) * MAX_CONNECTIONS;
}

struct fid_mr *get_bulk_mr()
{
    return bulk_mr;
}

struct fid_mr *get_cmd_mr()
{
    return cmd_mr;
}