// SPDX-License-Identifier: MIT
/* Copyright 2022 Eileen Yoon <eyn@gmx.com> */

#include "ane_bo.h"
#include "ane_priv.h"

#define FIFO_WIDTH 0x400 // nxtpow2(0x274)
#define FIFO_COUNT 0x20

static inline void set_nid(void *td, int nid)
{
	uint32_t hdr0 = *(uint32_t *)td;
	hdr0 = (hdr0 & 0xf00ffff) | ((nid & 0xff) << 16);
	memcpy(td, &hdr0, sizeof(uint32_t));
}

// clang-format off
static inline void load_anec(struct ane_nn *nn)
{
	const struct anec *anec = to_anec(nn);
	const void *anec_data = nn->model->data;

	memcpy(nn->chans[0]->map, anec_data, anec->size);

	/* do not fucking overflow */
	memcpy(nn->fifo_chan->map, anec_data, anec->td_size);
	memcpy((char *)nn->fifo_chan->map + FIFO_WIDTH, anec_data, anec->td_size);

	set_nid(nn->fifo_chan->map, ANE_FIFO_NID);
	set_nid((char *)nn->fifo_chan->map + FIFO_WIDTH, ANE_FIFO_NID + FIFO_COUNT);
}
// clang-format on

int ane_chan_free(struct ane_device *ane, struct ane_nn *nn)
{
	if (nn->fifo_chan) {
		ane_bo_free(ane, nn->fifo_chan);
	}

	for (int bdx = 0; bdx < ANE_TILE_COUNT; bdx++) {
		if (nn->chans[bdx]) {
			ane_bo_free(ane, nn->chans[bdx]);
		}
	}

	return 0;
}

int ane_chan_init(struct ane_device *ane, struct ane_nn *nn)
{
	const struct anec *anec = to_anec(nn);

	int ic = 0, oc = 0;
	for (int bdx = 0; bdx < ANE_TILE_COUNT; bdx++) {
		if (anec->types[bdx] == ANE_TILE_SRC) {
			nn->src_bdx[ic] = bdx;
			ic++;
		} else if (anec->types[bdx] == ANE_TILE_DST) {
			nn->dst_bdx[oc] = bdx;
			oc++;
		}
	}

	if (ic != input_count(nn) || oc != output_count(nn)) {
		fprintf(stderr, "LIBANE: invalid src/dst setup\n");
		return -EINVAL;
	}

	for (int bdx = 0; bdx < ANE_TILE_COUNT; bdx++) {
		if (anec->tiles[bdx]) {
			struct ane_bo *bo = NULL;
			bo = ane_bo_init(ane, tile_size(nn, bdx));
			if (!bo)
				goto error;
			nn->chans[bdx] = bo;
		}
	}

	nn->fifo_chan = ane_bo_init(ane, tile_align(FIFO_WIDTH * 2));
	if (!nn->fifo_chan)
		goto error;

	load_anec(nn);

	return 0;

error:
	fprintf(stderr, "LIBANE: out of memory for chans\n");
	ane_chan_free(ane, nn);
	return -ENOMEM;
}
