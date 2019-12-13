int main() {
	// ... set up ndev ...

	/**
	 * Create the first task
	 */
	struct niova_task *mount_task = new_task(TCT_MOUNT, 0);

	mount_task->task_cb = superblock_read_launch;

	/**
	 * Should only return on system end.
	 */
	return task_mgr_start(mount_task);
}

int superblock_read_launch(struct niova_task *task) {
	struct niosd_device *ndev = mount_task->task_data;

	/*
	 * Describe how many requests the task will use, resets request buffers
	 */
	reset_task(task, 1);
	struct niosd_io_request *niorq = task_request_io(task, 0);
	if (!niorq) {
		return -ENOMEM;
	}

	/**
	 * Requests should just describe what is needed, the IO system can allocate resources.
	 */
	rc = niosd_io_request_read(niorq,
			ndev,
			SB_PRIMARY_PBLK_ID, sizeof(struct sb_header_persistent),
			superblock_read_continue_cb);
    if (rc) {
		return rc;
	}

	task->task_cb = superblock_read_continue;

	/**
	 * Task manager will abort task if non-zero
	 */
	return 0;
}


int superblock_read_continue(struct niova_task *task) {
	struct niosd_device *ndev = mount_task->task_data;
	struct sb_header_data *sb = ndev->ndev_sb;

	/**
	 * Request memory management is handled by task system
	 */
	if (task->task_res) {
        superblock_read_error(ndev);

        return task->task_res;
    }

	sb_replica_t sb_replica = sb_2_current_replica_num(sb);

	/**
	 * Request results exist until reset_task is called
	 */
	struct niosd_io_request *niorq = task_request_io(task, 0);
    if (!sb_replica)
        superblock_initialize_from_ndev_pblk_data(ndev,
                                                  niorq->niorq_sink_buf);

    DBG_NIOSD_REQ(LL_DEBUG, niorq, "current-replica=%hhu", sb_replica);

    sb_set_current_replica_num(sb, sb_replica + 1);

	return 0;
}
