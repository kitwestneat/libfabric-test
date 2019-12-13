#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <time.h>
#include "network.h";

struct system_context
{
	struct net_info *net_ctx;
	// struct memory_info *mem_ctx;
	// struct task_info *task_ctx;
};

/*
struct task_info
{
	// tasks that have submit their requests
	struct task *submitted_tasks;

	// tasks that are waiting to submit
	struct task *queued_tasks;
};
*/

struct task
{
	enum task_context_type task_context_type;
	void *task_data;

	int task_req_count;
	int task_req_submitted;
	int task_req_completed;
	struct task_request **task_requests; // (or could be *task_requests if they are allocated in a block);

	bool task_queued; // not sure if this is necessary

	int task_res; // non-zero if there was an error with one of the requests
	int (*task_cb)(struct *task);
	// int (*task_catch)(struct *task); // or should errors always be handled in task_cb?

	struct task *task_next;
};

struct task_request
{
	enum task_request_type tr_type;
	int tr_status;
	struct task *tr_parent;

	union {
		struct net_request tr_net_req;
		struct timespec tr_timeout_req;
	};
};

enum task_request_type
{
	TRT_EMPTY,
	TRT_NET,
	TRT_TIMEOUT
	/*  ...etc...  */
};

enum task_request_status
{
	TRS_PENDING,
	TRS_SUBMITTED,
	TRS_COMPLETED
};

enum task_context_type
{
	TCT_MOUNT,
	TCT_UMOUNT,
	TCT_CLIENT,
	TCT_COMPACTOR,
	TCT_REBUILD
	/*  ...etc...  */
};

int task_mgr_start(struct system_context *ctx, struct task *task);

struct task *new_task(enum task_context_type task_context_type, int req_count);
void reset_task(struct task *task, int req_count);

struct niosd_net_request *task_request_net(struct task *task, int req_slot);
struct timespec *task_request_timeout(struct task *task, int req_slot);

#endif