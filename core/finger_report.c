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
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/list.h>

#include "../common.h"
#include "../platform.h"
#include "config.h"
#include "i2c.h"
#include "finger_report.h"
#include "mp_test.h"
#include "protocol.h"

/* An id with position in each fingers */
struct mutual_touch_point
{
	uint16_t id;
	uint16_t x;
	uint16_t y;
	uint16_t pressure;
};

/* Keys and code with each fingers */
struct mutual_touch_info
{
	uint8_t touch_num;
	uint8_t key_code;
	struct mutual_touch_point mtp[10];
};

/* Store the packet of finger report */
struct fr_data_node
{
	uint8_t *data;
	uint16_t len;
};

/* record the status of touch being pressed or released currently and previosuly */
uint8_t CurrentTouch[MAX_TOUCH_NUM];
uint8_t PreviousTouch[MAX_TOUCH_NUM];

/* the total length of finger report packet */
uint16_t tlen = 0;

struct mutual_touch_info mti;
struct fr_data_node *fnode = NULL, *fuart = NULL;
struct core_fr_data *core_fr = NULL;

/**
 * Calculate the check sum of each packet reported by firmware 
 *
 * @pMsg: packet come from firmware
 * @nLength : the length of its packet
 */
static uint8_t cal_fr_checksum(uint8_t *pMsg, uint32_t nLength)
{
	int i;
	int32_t nCheckSum = 0;

	for (i = 0; i < nLength; i++)
	{
		nCheckSum += pMsg[i];
	}

	return (uint8_t)((-nCheckSum) & 0xFF);
}

/**
 *  Receive data when fw mode stays at i2cuart mode.
 *
 *  the first is to receive N bytes depending on the mode that firmware stays 
 *  before going in this function, and it would check with i2c buffer if it
 *  remains the rest of data.
 */
static void i2cuart_recv_packet(void)
{
	int res = 0, need_read_len = 0, one_data_bytes = 0;
	int type = fnode->data[3] & 0x0F;
	int actual_len = fnode->len - 5;

	DBG(DEBUG_FINGER_REPORT, "pid = %x, data[3] = %x, type = %x, actual_len = %d", 
			fnode->data[0], fnode->data[3], type, actual_len);

	need_read_len = fnode->data[1] * fnode->data[2];

	if (type == 0 || type == 1 || type == 6)
	{
		one_data_bytes = 1;
	}
	else if (type == 2 || type == 3)
	{
		one_data_bytes = 2;
	}
	else if (type == 4 || type == 5)
	{
		one_data_bytes = 4;
	}

	DBG(DEBUG_FINGER_REPORT, "need_read_len = %d  one_data_bytes = %d", 
			need_read_len, one_data_bytes);
	
	need_read_len = need_read_len * one_data_bytes + 1;

	if (need_read_len > actual_len)
	{
		fuart = kmalloc(sizeof(*fuart), GFP_ATOMIC);
		if(ERR_ALLOC_MEM(fuart))
		{
			DBG_ERR("Failed to allocate fuart memory %ld", PTR_ERR(fuart));
			return;
		}

		fuart->len = need_read_len - actual_len;
		fuart->data = kzalloc(fuart->len, GFP_ATOMIC);
		if(ERR_ALLOC_MEM(fuart->data))
		{
			DBG_ERR("Failed to allocate fuart memory %ld", PTR_ERR(fuart->data));
			return;
		}

		tlen += fuart->len;
		res = core_i2c_read(core_config->slave_i2c_addr, fuart->data, fuart->len);
		if (res < 0)
			DBG_ERR("Failed to read finger report packet");

	}
}

/*
 * It'd be called when a finger's touching down a screen. It'll notify the event
 * to the uplayer from input device.
 *
 * @x: the axis of X
 * @y: the axis of Y
 * @pressure: the value of pressue on a screen
 * @id: an id represents a finger pressing on a screen
 */
void core_fr_touch_press(int32_t x, int32_t y, uint32_t pressure, int32_t id)
{
	DBG(DEBUG_FINGER_REPORT, "DOWN: id = %d, x = %d, y = %d", id, x, y);

#ifdef MT_B_TYPE
	input_mt_slot(core_fr->input_device, id);
	input_mt_report_slot_state(core_fr->input_device, MT_TOOL_FINGER, true);
	input_report_abs(core_fr->input_device, ABS_MT_POSITION_X, x);
	input_report_abs(core_fr->input_device, ABS_MT_POSITION_Y, y);

	if(core_fr->isEnablePressure)
		input_report_abs(core_fr->input_device, ABS_MT_PRESSURE, pressure);
#else
	input_report_key(core_fr->input_device, BTN_TOUCH, 1);

	input_report_abs(core_fr->input_device, ABS_MT_TRACKING_ID, id);
	input_report_abs(core_fr->input_device, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(core_fr->input_device, ABS_MT_WIDTH_MAJOR, 1);
	input_report_abs(core_fr->input_device, ABS_MT_POSITION_X, x);
	input_report_abs(core_fr->input_device, ABS_MT_POSITION_Y, y);

	if(core_fr->isEnablePressure)
		input_report_abs(core_fr->input_device, ABS_MT_PRESSURE, pressure);

	input_mt_sync(core_fr->input_device);
#endif
}
EXPORT_SYMBOL(core_fr_touch_press);

/*
 * It'd be called when a finger's touched up from a screen. It'll notify
 * the event to the uplayer from input device.
 *
 * @x: the axis of X
 * @y: the axis of Y
 * @id: an id represents a finger leaving from a screen.
 */
void core_fr_touch_release(int32_t x, int32_t y, int32_t id)
{
	DBG(DEBUG_FINGER_REPORT, "UP: id = %d, x = %d, y = %d", id, x, y);

#ifdef MT_B_TYPE
	input_mt_slot(core_fr->input_device, id);
	input_mt_report_slot_state(core_fr->input_device, MT_TOOL_FINGER, false);	
#else
	input_report_key(core_fr->input_device, BTN_TOUCH, 0);
	input_mt_sync(core_fr->input_device);	
#endif
}
EXPORT_SYMBOL(core_fr_touch_release);

static int parse_touch_package_v3_2(void)
{
	DBG_INFO("Not implemented yet");
	return 0;
}

static int finger_report_ver_3_2(void)
{
	DBG_INFO("Not implemented yet");
	parse_touch_package_v3_2();
	return 0;
}

/*
 * It mainly parses the packet assembled by protocol v5.0
 */
static int parse_touch_package_v5_0(uint8_t pid)
{
	int i, res = 0;
	uint8_t check_sum = 0;
	uint32_t nX = 0, nY = 0;

	for (i = 0; i < 9; i++)
	 	DBG(DEBUG_FINGER_REPORT, "data[%d] = %x", i, fnode->data[i]);

	check_sum = cal_fr_checksum(&fnode->data[0], (fnode->len - 1));
	DBG(DEBUG_FINGER_REPORT, "data = %x  ;  check_sum : %x ", fnode->data[fnode->len - 1], check_sum);

	if (fnode->data[fnode->len - 1] != check_sum)
	{
		DBG_ERR("Wrong checksum");
		res = -1;
		goto out;
	}

	/* start to parsing the packet of finger report */
	if (pid == protocol->demo_pid)
	{
		DBG(DEBUG_FINGER_REPORT, " **** Parsing DEMO packets : 0x%x ****", pid);

		for (i = 0; i < MAX_TOUCH_NUM; i++)
		{
			if ((fnode->data[(4 * i) + 1] == 0xFF) && (fnode->data[(4 * i) + 2] && 0xFF) && (fnode->data[(4 * i) + 3] == 0xFF))
			{
				#ifdef MT_B_TYPE
					CurrentTouch[i] = 0;
				#endif
				continue;
			}

			nX = (((fnode->data[(4 * i) + 1] & 0xF0) << 4) | (fnode->data[(4 * i) + 2]));
			nY = (((fnode->data[(4 * i) + 1] & 0x0F) << 8) | (fnode->data[(4 * i) + 3]));

			if(!core_fr->isSetResolution)
			{
				mti.mtp[mti.touch_num].x = nX * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
				mti.mtp[mti.touch_num].y = nY * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
				mti.mtp[mti.touch_num].id = i;
			}
			else
			{
				mti.mtp[mti.touch_num].x = nX;
				mti.mtp[mti.touch_num].y = nY;
				mti.mtp[mti.touch_num].id = i;
			}

			if(core_fr->isEnablePressure)
				mti.mtp[mti.touch_num].pressure = fnode->data[(4 * i) + 4];
			else
				mti.mtp[mti.touch_num].pressure = 1;

			DBG(DEBUG_FINGER_REPORT, "[x,y]=[%d,%d]", nX, nY);
			DBG(DEBUG_FINGER_REPORT, "point[%d] : (%d,%d) = %d",
				mti.mtp[mti.touch_num].id,
				mti.mtp[mti.touch_num].x,
				mti.mtp[mti.touch_num].y,
				mti.mtp[mti.touch_num].pressure);

			mti.touch_num++;

			#ifdef MT_B_TYPE
				CurrentTouch[i] = 1;
			#endif
		}
	}
	else if (pid == protocol->debug_pid)
	{
		DBG(DEBUG_FINGER_REPORT, " **** Parsing DEBUG packets : 0x%x ****", pid);
		DBG(DEBUG_FINGER_REPORT, "Length = %d", (fnode->data[1] << 8 | fnode->data[2]));

		for (i = 0; i < MAX_TOUCH_NUM; i++)
		{
			if ((fnode->data[(3 * i) + 5] == 0xFF) && (fnode->data[(3 * i) + 6] && 0xFF) && (fnode->data[(3 * i) + 7] == 0xFF))
			{
				#ifdef MT_B_TYPE
					CurrentTouch[i] = 0;
				#endif
				continue;
			}

			nX = (((fnode->data[(3 * i) + 5] & 0xF0) << 4) | (fnode->data[(3 * i) + 6]));
			nY = (((fnode->data[(3 * i) + 5] & 0x0F) << 8) | (fnode->data[(3 * i) + 7]));

			if(!core_fr->isSetResolution)
			{
				mti.mtp[mti.touch_num].x = nX * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
				mti.mtp[mti.touch_num].y = nY * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
				mti.mtp[mti.touch_num].id = i;
			}
			else
			{
				mti.mtp[mti.touch_num].x = nX;
				mti.mtp[mti.touch_num].y = nY;
				mti.mtp[mti.touch_num].id = i;
			}

			if(core_fr->isEnablePressure)
				mti.mtp[mti.touch_num].pressure = fnode->data[(4 * i) + 4];
			else
				mti.mtp[mti.touch_num].pressure = 1;

			DBG(DEBUG_FINGER_REPORT, "[x,y]=[%d,%d]", nX, nY);
			DBG(DEBUG_FINGER_REPORT, "point[%d] : (%d,%d) = %d",
				mti.mtp[mti.touch_num].id,
				mti.mtp[mti.touch_num].x,
				mti.mtp[mti.touch_num].y,
				mti.mtp[mti.touch_num].pressure);

			mti.touch_num++;

			#ifdef MT_B_TYPE
				CurrentTouch[i] = 1;
			#endif
		}
	}
	else
	{
		if(pid == 0)
		{
			/* ignore the pid with 0x0 after enable irq at once */
			goto out;
		}
		else
		{
			DBG_ERR(" **** Unkown PID : 0x%x ****", pid);
			res = -1;
			goto out;
		}
	}

out:
	return res;
}

/*
 * The function is called by an interrupt and used to handle packet of finger 
 * touch from firmware. A differnece in the process of the data is acorrding to the protocol
 */
static int finger_report_ver_5_0(void)
{
	int i, res = 0;
	static int last_touch = 0;
	uint8_t pid = 0x0;

	memset(&mti, 0x0, sizeof(struct mutual_touch_info));

#ifdef I2C_SEGMENT
	res = core_i2c_segmental_read(core_config->slave_i2c_addr, fnode->data, fnode->len);
#else
	res = core_i2c_read(core_config->slave_i2c_addr, fnode->data, fnode->len);
#endif
	if (res < 0)
	{
		DBG_ERR("Failed to read finger report packet");
		goto out;
	}

	pid = fnode->data[0];
	DBG(DEBUG_FINGER_REPORT, "PID = 0x%x", pid);
	
	if(pid == protocol->i2cuart_pid)
	{
		DBG(DEBUG_FINGER_REPORT, "I2CUART(0x%x): prepare to receive rest of data", pid);
		i2cuart_recv_packet();
		goto out;
	}

	if(pid == protocol->ges_pid && core_config->isEnableGesture)
	{
		DBG(DEBUG_FINGER_REPORT, "pid = 0x%x, code = %x", pid, fnode->data[1]);
		input_report_key(core_fr->input_device, KEY_POWER, 1);
		input_sync(core_fr->input_device);
		input_report_key(core_fr->input_device, KEY_POWER, 0);
		input_sync(core_fr->input_device);
		goto out;
	}

	res = parse_touch_package_v5_0(pid);
	if (res < 0)
	{
		DBG_ERR("Failed to parse packet of finger touch");
		goto out;
	}

	DBG(DEBUG_FINGER_REPORT, "Touch Num = %d, LastTouch = %d", mti.touch_num, last_touch);

	/* interpret parsed packat and send input events to system */
	if (mti.touch_num > 0)
	{
		#ifdef MT_B_TYPE
			for (i = 0; i < mti.touch_num; i++)
			{
				input_report_key(core_fr->input_device, BTN_TOUCH, 1);
				core_fr_touch_press(mti.mtp[i].x, mti.mtp[i].y, mti.mtp[i].pressure, mti.mtp[i].id);

				input_report_key(core_fr->input_device, BTN_TOOL_FINGER, 1);
			}

			for (i = 0; i < MAX_TOUCH_NUM; i++)
			{
				DBG(DEBUG_FINGER_REPORT, "PreviousTouch[%d]=%d, CurrentTouch[%d]=%d", i, PreviousTouch[i], i, CurrentTouch[i]);

				if (CurrentTouch[i] == 0 && PreviousTouch[i] == 1)
				{
					core_fr_touch_release(0, 0, i);
				}

				PreviousTouch[i] = CurrentTouch[i];
			}
		#else
			for (i = 0; i < mti.touch_num; i++)
			{
				core_fr_touch_press(mti.mtp[i].x, mti.mtp[i].y, mti.mtp[i].pressure, mti.mtp[i].id);
			}
		#endif
		input_sync(core_fr->input_device);

		last_touch = mti.touch_num;
	}
	else
	{
		if (last_touch > 0)
		{
			#ifdef MT_B_TYPE
				for (i = 0; i < MAX_TOUCH_NUM; i++)
				{
					DBG(DEBUG_FINGER_REPORT, "PreviousTouch[%d]=%d, CurrentTouch[%d]=%d", i, PreviousTouch[i], i, CurrentTouch[i]);

					if (CurrentTouch[i] == 0 && PreviousTouch[i] == 1)
					{
						core_fr_touch_release(0, 0, i);
					}
					PreviousTouch[i] = CurrentTouch[i];
				}
				input_report_key(core_fr->input_device, BTN_TOUCH, 0);
				input_report_key(core_fr->input_device, BTN_TOOL_FINGER, 0);
			#else
				core_fr_touch_release(0, 0, 0);
			#endif

			input_sync(core_fr->input_device);

			last_touch = 0;
		}
	}

out:
	return res;
}

void core_fr_mode_control(uint8_t *from_user)
{
	int mode, res = 0;
	uint8_t cmd[4] = {0};

	ilitek_platform_disable_irq();

	if (from_user == NULL)
	{
		DBG_ERR("Arguments from user space are invaild");
		goto out;
	}

	DBG(DEBUG_FINGER_REPORT, "mode = %x, b1 = %x, b2 = %x, b3 = %x",
			from_user[0], from_user[1], from_user[2], from_user[3]);

	mode = from_user[0];

	if(protocol->major == 0x5)
	{
		if(mode == protocol->i2cuart_mode)
		{
			cmd[0] = protocol->cmd_i2cuart;
			cmd[1] = *(from_user + 1);
			cmd[2] = *(from_user + 2);

			DBG_INFO("Switch to I2CUART mode, cmd = %x, b1 = %x, b2 = %x",
					 cmd[0], cmd[1], cmd[2]);

			res = core_i2c_write(core_config->slave_i2c_addr, cmd, 3);
			if (res < 0)
				DBG_ERR("Failed to switch I2CUART mode");
		}
		else if(mode == protocol->demo_mode || mode == protocol->debug_mode)
		{
			cmd[0] = protocol->cmd_mode_ctrl;
			cmd[1] = mode;

			DBG_INFO("Switch to Demo/Debug mode, cmd = 0x%x, b1 = 0x%x",
					 cmd[0], cmd[1]);

			res = core_i2c_write(core_config->slave_i2c_addr, cmd, 2);
			if (res < 0)
			{
				DBG_ERR("Failed to switch Demo/Debug mode");
			}
			else
			{
				core_fr->actual_fw_mode = mode;				
			}
		}
		else if(mode == protocol->test_mode)
		{
			cmd[0] = protocol->cmd_mode_ctrl;
			cmd[1] = mode;

			DBG_INFO("Switch to Test mode, cmd = 0x%x, b1 = 0x%x",
					 cmd[0], cmd[1]);

			res = core_i2c_write(core_config->slave_i2c_addr, cmd, 2);
			if (res < 0)
			{
				DBG_ERR("Failed to switch Test mode");
			}
			else
			{
				core_fr->actual_fw_mode = mode;				
			}

			// doing sensor test
			//core_mp_switch_mode();
		}
		else
		{
			DBG_ERR("Unknown firmware mode: %d", mode);
		}
	}
	else
	{
		DBG_ERR("Wrong the major version of protocol, 0x%x", protocol->major);
	}

out:
	ilitek_platform_enable_irq();
	return;
}
EXPORT_SYMBOL(core_fr_mode_control);

/**
 * Calculate the length with different modes according to the format of protocol 5.0
 *
 * We compute the length before receiving its packet. If the length is differnet between
 * firmware and the number we calculated, in this case I just print an error to inform users
 * and still send up to users.
 */
static uint16_t calc_packet_length(void)
{
	uint16_t xch = 0, ych = 0, stx = 0, srx = 0;
	//FIXME: self_key not defined by firmware yet
	uint16_t self_key = 2;
	uint16_t rlen = 0;

	if(protocol->major == 0x5)
	{
		if(!ERR_ALLOC_MEM(core_config->tp_info))
		{
			xch = core_config->tp_info->nXChannelNum;
			ych = core_config->tp_info->nYChannelNum;
			stx = core_config->tp_info->self_tx_channel_num;
			srx = core_config->tp_info->self_rx_channel_num;
		}

		DBG(DEBUG_FINGER_REPORT, "firmware mode : 0x%x", core_fr->actual_fw_mode);

		if (protocol->demo_mode == core_fr->actual_fw_mode)
		{
			rlen = protocol->demo_len;
		}
		else if (protocol->test_mode == core_fr->actual_fw_mode)
		{
			if(ERR_ALLOC_MEM(core_config->tp_info))
			{
				rlen = protocol->test_len;
			}
			else
			{
				rlen = (2 * xch * ych) + (stx * 2) + (srx * 2) + 2 * self_key + 1;
				rlen += 1;
			}
		}
		else if (protocol->debug_mode == core_fr->actual_fw_mode)
		{
			if(ERR_ALLOC_MEM(core_config->tp_info))
			{
				rlen = protocol->debug_len;	
			}
			else
			{
				rlen = (2 * xch * ych) + (stx * 2) + (srx * 2) + 2 * self_key + (8 * 2) + 1;
				rlen += 35;
			}
		}
		else
		{
			DBG_ERR("Unknow firmware mode : %d", core_fr->actual_fw_mode);
			rlen = 0;
		}
	}
	else
	{
		DBG_ERR("Wrong the major version of protocol, 0x%x", protocol->major);
		return -1;
	}

	DBG(DEBUG_FINGER_REPORT, "rlen = %d", rlen);
	return rlen;
}

/**
 * The table is used to handle calling functions that deal with packets of finger report.
 * The callback function might be different of what a protocol is used on a chip.
 *
 * It's possible to have the different protocol according to customer's requirement on the same
 * touch ic with customised firmware, so I don't have to identify which of the ic has been used; instead,
 * the version of protocol should match its parsing pattern.
 */
typedef struct
{
	uint8_t protocol_marjor_ver;
	uint8_t protocol_minor_ver;
	int (*finger_report)(void);
} fr_hashtable;

fr_hashtable fr_t[] = {
	{0x3, 0x2, finger_report_ver_3_2},
	{0x5, 0x0, finger_report_ver_5_0},
	{0x5, 0x1, finger_report_ver_5_0},
};

/**
 * The function is an entry for the work queue registered by ISR activates.
 *
 * Here will allocate the size of packet depending on what the current protocol
 * is used on its firmware.
 */
void core_fr_handler(void)
{
	int i = 0;
	uint8_t *tdata = NULL;

	if(core_fr->isEnableFR)
	{
		tlen = calc_packet_length();
		if(tlen)
		{
			fnode = kmalloc(sizeof(*fnode), GFP_ATOMIC);	
			if(ERR_ALLOC_MEM(fnode))
			{
				DBG_ERR("Failed to allocate fnode memory %ld", PTR_ERR(fnode));
				goto out;
			}

			fnode->data = kmalloc(sizeof(uint8_t) * tlen, GFP_ATOMIC);
			if(ERR_ALLOC_MEM(fnode->data))
			{
				DBG_ERR("Failed to allocate fnode memory %ld", PTR_ERR(fnode->data));
				goto out;
			}

			fnode->len = tlen;
			memset(fnode->data, 0xFF, (uint8_t)sizeof(uint8_t) * tlen);

			while(i < ARRAY_SIZE(fr_t))
			{
				if(protocol->major == fr_t[i].protocol_marjor_ver)
				{
					mutex_lock(&ipd->MUTEX);
					fr_t[i].finger_report();
					mutex_unlock(&ipd->MUTEX);

					if (core_fr->isEnableNetlink)
					{
						/* 2048 is referred to the defination by user */
						if(tlen < 2048)
						{
							tdata = kmalloc(tlen, GFP_ATOMIC);
							if(ERR_ALLOC_MEM(tdata))
							{
								DBG_ERR("Failed to allocate fnode memory %ld", PTR_ERR(tdata));
								goto out;
							}

							memcpy(tdata, fnode->data, fnode->len);
							/* merge uart data if it's at i2cuart mode */
							if(fuart != NULL)
								memcpy(tdata+fnode->len, fuart->data, fuart->len);
						}
						else
						{
							DBG_ERR("total lenght (%d) is too long than user can handle", tlen);
							goto out;
						}

						netlink_reply_msg(tdata, tlen);
					}
					break;
				}
				i++;
			}

			if(i >= ARRAY_SIZE(fr_t))
				DBG_ERR("Can't find any callback functions to handle INT event");
		}
		else
		{
			DBG_ERR("Wrong the length of packet");
		}
	}
	else
	{
		DBG_ERR("The figner report was disabled");
	}

out:
	DBG(DEBUG_IRQ, "handle INT done \n");
	tlen = 0;
	if(tdata != NULL)
	{
		kfree(tdata);
		tdata = NULL;
	}
	if(fnode != NULL)
	{
		if(fnode->data != NULL)
		{
			kfree(fnode->data);
			fnode->data = NULL;
		}
		kfree(fnode);
		fnode = NULL;
	}
	if(fuart != NULL)
	{
		if(fuart->data != NULL)
		{
			kfree(fuart->data);
			fuart->data = NULL;
		}
		kfree(fuart);
		fuart = NULL;
	}
	return;
}
EXPORT_SYMBOL(core_fr_handler);

void core_fr_input_set_param(struct input_dev *input_device)
{
	int max_x = 0, max_y = 0, min_x = 0, min_y = 0;
	int max_tp = 0;

	core_fr->input_device = input_device;

	/* set the supported event type for input device */
	set_bit(EV_ABS, core_fr->input_device->evbit);
	set_bit(EV_SYN, core_fr->input_device->evbit);
	set_bit(EV_KEY, core_fr->input_device->evbit);
	set_bit(BTN_TOUCH, core_fr->input_device->keybit);
	set_bit(BTN_TOOL_FINGER, core_fr->input_device->keybit);
	set_bit(INPUT_PROP_DIRECT, core_fr->input_device->propbit);

	if (core_fr->isSetResolution)
	{
		max_x = core_config->tp_info->nMaxX;
		max_y = core_config->tp_info->nMaxY;
		min_x = core_config->tp_info->nMinX;
		min_y = core_config->tp_info->nMinY;
		max_tp = core_config->tp_info->nMaxTouchNum;
	}
	else
	{
		max_x = TOUCH_SCREEN_X_MAX;
		max_y = TOUCH_SCREEN_Y_MAX;
		min_x = TOUCH_SCREEN_X_MIN;
		min_y = TOUCH_SCREEN_Y_MIN;
		max_tp = MAX_TOUCH_NUM;
	}

	DBG_INFO("input resolution : max_x = %d, max_y = %d, min_x = %d, min_y = %d",
		max_x, max_y, min_x, min_y);
	DBG_INFO("input touch number: max_tp = %d", max_tp);

#ifndef PLATFORM_MTK
	input_set_abs_params(core_fr->input_device, ABS_MT_POSITION_X, min_x, max_x - 1, 0, 0);
	input_set_abs_params(core_fr->input_device, ABS_MT_POSITION_Y, min_y, max_y - 1, 0, 0);

	input_set_abs_params(core_fr->input_device, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(core_fr->input_device, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
#endif

	if(core_fr->isEnablePressure)
		input_set_abs_params(core_fr->input_device, ABS_MT_PRESSURE, 0, 255, 0, 0);

	#ifdef MT_B_TYPE
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
			input_mt_init_slots(core_fr->input_device, max_tp, INPUT_MT_DIRECT);
		#else
			input_mt_init_slots(core_fr->input_device, max_tp);
		#endif
	#else
		input_set_abs_params(core_fr->input_device, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
	#endif /* MT_B_TYPE */

	/* Set up virtual key with gesture code */
	input_set_capability(core_fr->input_device, EV_KEY, KEY_POWER);
	input_set_capability(core_fr->input_device, EV_KEY, KEY_UP);
	input_set_capability(core_fr->input_device, EV_KEY, KEY_DOWN);
	input_set_capability(core_fr->input_device, EV_KEY, KEY_LEFT);
	input_set_capability(core_fr->input_device, EV_KEY, KEY_RIGHT);

	if(core_fr->isSetPhoneCover)
		core_config_set_phone_cover(NULL);

	return;
}
EXPORT_SYMBOL(core_fr_input_set_param);

int core_fr_init(struct i2c_client *pClient)
{
	int i = 0, res = -1;

	for (; i < ARRAY_SIZE(ipio_chip_list); i++)
	{
		if (ipio_chip_list[i] == ON_BOARD_IC)
		{
			core_fr = kzalloc(sizeof(*core_fr), GFP_KERNEL);
			if (ERR_ALLOC_MEM(core_fr))
			{
				DBG_ERR("Failed to init core_fr, %ld", PTR_ERR(core_fr));
				res = -ENOMEM;
				goto out;
			}

			core_fr->isEnableFR = true;
			core_fr->isEnableNetlink = false;
			core_fr->isEnablePressure = false;
			core_fr->isSetResolution = false;
			core_fr->isSetPhoneCover = false;
			core_fr->actual_fw_mode = protocol->demo_mode;

			res = 0;
			return res;			
		}
	}

	DBG_ERR("Can't find this chip in support list");
out:
	core_fr_remove();
	return res;
}
EXPORT_SYMBOL(core_fr_init);

void core_fr_remove(void)
{
	DBG_INFO("Remove core-FingerReport members");

	if(core_fr != NULL)
		kfree(core_fr);
}
EXPORT_SYMBOL(core_fr_remove);
