#include "task.h";

// tasks that have submit their requests
struct task *submitted_tasks;

// tasks that are waiting to submit
struct task *queued_tasks;

void task_poll_network()
{
}

bool is_task_done(struct task *task)
{
	return task->task_req_count == task->task_req_completed;
}

int task_mgr_start(struct system_context *ctx, struct task *task)
{
	int rc;
	do
	{
		if (is_task_done(task))
		{
			rc = task->task_cb(task);
		}
		else
		{
			rc = submit_requests(task->task_requests, task->task_req_count);
		}

		task_poll_network();
		task_poll_timeout();
		task_poll_mem();

		task = get_next_task();
	} while (rc == 0);
}
