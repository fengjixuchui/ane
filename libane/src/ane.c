// SPDX-License-Identifier: MIT
/* Copyright 2022 Eileen Yoon <eyn@gmx.com> */

#include <drm.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ane_chan.h"
#include "ane_priv.h"

#define ANE_SYSFS_PATH "/dev/dri/renderD129"

static struct ane_device *device_new(void)
{
	struct ane_device *ane = ane_zmalloc(sizeof(struct ane_device));
	if (!ane)
		return NULL;

	ane->fd = open(ANE_SYSFS_PATH, O_RDWR, S_IRUSR | S_IWUSR);
	if (ane->fd < 0) {
		fprintf(stderr, "LIBANE: failed to open sysfs %s\n",
			ANE_SYSFS_PATH);
		free(ane);
		return NULL;
	}

	return ane;
}

static void device_del(struct ane_device *ane)
{
	close(ane->fd);
	free(ane);
}

struct ane_nn *ane_init(const struct ane_model *model)
{
	int err;

	struct ane_nn *nn = ane_zmalloc(sizeof(struct ane_nn));
	if (!nn)
		return NULL;

	nn->model = model;

	nn->ane = device_new();
	if (!nn->ane)
		goto free;

	err = ane_chan_init(nn->ane, nn);
	if (err) {
		fprintf(stderr, "LIBANE: ane_chan_init failed with 0x%x\n",
			err);
		goto del;
	}

	printf("LIBANE: initialized nn %p\n", (void *)nn);

	return nn;

del:
	device_del(nn->ane);
free:
	free(nn);
	return NULL;
}

void ane_free(struct ane_nn *nn)
{
	printf("LIBANE: freeing nn %p\n", (void *)nn);
	ane_chan_free(nn->ane, nn);
	device_del(nn->ane);
	free(nn);
}

int ane_exec(struct ane_nn *nn)
{
	const struct anec *anec = to_anec(nn);

	struct drm_ane_submit args;
	memset(&args, 0, sizeof(args));

	args.tsk_size = anec->tsk_size;
	args.td_count = anec->td_count;
	args.td_size = anec->td_size;

	for (int bdx = 0; bdx < ANE_TILE_COUNT; bdx++) {
		if (nn->chans[bdx]) {
			args.tile_handle[bdx] = nn->chans[bdx]->handle;
		}
	}
	args.fifo_handle = nn->fifo_chan->handle;

	return ioctl(nn->ane->fd, DRM_IOCTL_ANE_SUBMIT, &args);
}

int ane_send_raw(struct ane_nn *nn, void *from, const int idx)
{
	if (idx >= input_count(nn))
		return -EINVAL;
	memcpy(nn->chans[nn->src_bdx[idx]]->map, from,
	       tile_size(nn, nn->src_bdx[idx]));
	return 0;
}

int ane_read_raw(struct ane_nn *nn, void *to, const int idx)
{
	if (idx >= output_count(nn))
		return -EINVAL;
	memcpy(to, nn->chans[nn->dst_bdx[idx]]->map,
	       tile_size(nn, nn->dst_bdx[idx]));
	return 0;
}
