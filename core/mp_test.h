/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __MP_TEST_H
#define __MP_TEST_H

#define MP_PASS      1
#define MP_FAIL     -1
#define BENCHMARK 1
#define NODETYPE	 	1
#define RAWDATA_NO_BK_DATA_SHIFT_9881H 8192
#define RAWDATA_NO_BK_DATA_SHIFT_9881F 4096
#define TYPE_BENCHMARK 0
#define TYPE_NO_JUGE 1
#define TYPE_JUGE 2

struct mp_test_P540_open {
	int32_t *tdf_700;
	int32_t *tdf_250;
	int32_t *tdf_200;
	int32_t *cbk_700;
	int32_t *cbk_250;
	int32_t *cbk_200;
	int32_t *charg_rate;
	int32_t *full_Open;
	int32_t *dac;
};

struct mp_test_open_c {
	int32_t *cap1_dac;
	int32_t *cap2_dac;
	int32_t *cap1_raw;
	int32_t *cap2_raw;
	int32_t *dcl_cap;
	int32_t *lfd_cap;
};

struct mp_test_items {
	int id;
	char *name;
	/* The description must be the same as ini's section name */
	char *desp;
	char *result;
	int catalog;
	uint8_t cmd;
	uint8_t spec_option;
	uint8_t type_option;
	bool run;
	int max;
	int max_res;
	int item_result;
	int min;
	int min_res;
	int frame_count;
	int trimmed_mean;
	int lowest_percentage;
	int highest_percentage;
	int v_tdf_1;
	int v_tdf_2;
	int h_tdf_1;
	int h_tdf_2;
	int32_t *result_buf;
	int32_t *buf;
	int32_t *max_buf;
	int32_t *min_buf;
	int32_t *bench_mark_max;
	int32_t *bench_mark_min;
	int32_t *node_type;
	int (*do_test)(int index);
};

struct core_mp_test_data {
	/* A flag shows a test run in particular */
	bool retry;
	bool m_signal;
	bool m_dac;
	bool s_signal;
	bool s_dac;
	bool key_dac;
	bool st_dac;
	bool p_no_bk;
	bool p_has_bk;
	bool open_integ;
	bool open_cap;
	bool isLongV;

	int xch_len;
	int ych_len;
	int stx_len;
	int srx_len;
	int key_len;
	int st_len;
	int frame_len;
	int mp_items;
	int final_result;

	/* Tx/Rx threshold & buffer */
	int TxDeltaMax;
	int TxDeltaMin;
	int RxDeltaMax;
	int RxDeltaMin;
	int32_t *tx_delta_buf;
	int32_t *rx_delta_buf;
	int32_t *tx_max_buf;
	int32_t *tx_min_buf;
	int32_t *rx_max_buf;
	int32_t *rx_min_buf;

	int tdf;
	bool busy_cdc;
	bool ctrl_lcm;
};

extern struct core_mp_test_data *core_mp;
extern struct mp_test_items tItems[];

extern void dump_data(void *data, int type, int len, int row_len, const char *name);
extern void core_mp_copy_reseult(char *buf, size_t size);
extern void core_mp_test_free(void);
extern int core_mp_move_code(void);
extern int core_mp_start_test(bool lcm_on);
#endif
