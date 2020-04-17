// SPDX-License-Identifier: GPL-2.0+
/*
 * Hardware monitoring driver for Renesas Digital Multiphase Voltage Regulators
 *
 * Copyright (c) 2017 Google Inc
 * Copyright (c) 2020 Renesas Electronics America
 *
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "pmbus.h"

#define ISL68137_VOUT_AVS		0x30
#define RAA_DMPVR2_DMA_FIX		0xc5
#define RAA_DMPVR2_DMA_SEQ		0xc6
#define RAA_DMPVR2_DMA_ADDR		0xc7
#define RAA_DMPVR2_READ_VMON		0xc8
#define RAA_DMPVR2_BB_BASE_ADDR 	0xef80
#define RAA_DMPVR2_BB_WSIZE		4
#define RAA_DMPVR2_BB_WCNT		32
#define RAA_DMPVR2_BB_BUF_SIZE  	288
#define RAA_DMPVR2_NVM_CNT_ADDR 	0x00c2
#define RAA_DMPVR2_PRGM_STATUS_ADDR	0x0707
#define RAA_DMPVR2_BANK0_STATUS_ADDR	0x0709
#define RAA_DMPVR2_BANK1_STATUS_ADDR	0x070a
#define RAA_DMPVR2_CFG_MAX_SLOT 	16
#define RAA_DMPVR2_CFG_HEAD_LEN 	290
#define RAA_DMPVR2_CFG_SLOT_LEN 	358

enum chips {
	isl68137,
	isl68220,
	isl68221,
	isl68222,
	isl68223,
	isl68224,
	isl68225,
	isl68226,
	isl68227,
	isl68229,
	isl68233,
	isl68239,
	isl69222,
	isl69223,
	isl69224,
	isl69225,
	isl69227,
	isl69228,
	isl69234,
	isl69236,
	isl69239,
	isl69242,
	isl69243,
	isl69247,
	isl69248,
	isl69254,
	isl69255,
	isl69256,
	isl69259,
	isl69260,
	isl69268,
	isl69269,
	isl69298,
	raa228000,
	raa228004,
	raa228006,
	raa228228,
	raa229001,
	raa229004,
};

enum variants {
	raa_dmpvr1_2rail,
	raa_dmpvr2_1rail,
	raa_dmpvr2_2rail,
	raa_dmpvr2_3rail,
	raa_dmpvr2_hv,
};

enum {
	RAA_DMPVR2_DEBUGFS_CFG_W = 0,
	RAA_DMPVR2_DEBUGFS_BB_R,
	RAA_DMPVR2_DEBUGFS_NUM_ENTRIES,
};

struct raa_dmpvr2_ctrl {
	enum chips part;
	struct i2c_client *client;
	int debugfs_entries[RAA_DMPVR2_DEBUGFS_NUM_ENTRIES];
};

#define to_ctrl(x, y) container_of((x), struct raa_dmpvr2_ctrl, \
				   debugfs_entries[(y)])

static ssize_t isl68137_avs_enable_show_page(struct i2c_client *client,
					     int page,
					     char *buf)
{
	int val = pmbus_read_byte_data(client, page, PMBUS_OPERATION);

	return sprintf(buf, "%d\n",
		       (val & ISL68137_VOUT_AVS) == ISL68137_VOUT_AVS ? 1 : 0);
}

static ssize_t isl68137_avs_enable_store_page(struct i2c_client *client,
					      int page,
					      const char *buf, size_t count)
{
	int rc, op_val;
	bool result;

	rc = kstrtobool(buf, &result);
	if (rc)
		return rc;

	op_val = result ? ISL68137_VOUT_AVS : 0;

	/*
	 * Writes to VOUT setpoint over AVSBus will persist after the VRM is
	 * switched to PMBus control. Switching back to AVSBus control
	 * restores this persisted setpoint rather than re-initializing to
	 * PMBus VOUT_COMMAND. Writing VOUT_COMMAND first over PMBus before
	 * enabling AVS control is the workaround.
	 */
	if (op_val == ISL68137_VOUT_AVS) {
		rc = pmbus_read_word_data(client, page, 0xff,
					  PMBUS_VOUT_COMMAND);
		if (rc < 0)
			return rc;

		rc = pmbus_write_word_data(client, page, PMBUS_VOUT_COMMAND,
					   rc);
		if (rc < 0)
			return rc;
	}

	rc = pmbus_update_byte_data(client, page, PMBUS_OPERATION,
				    ISL68137_VOUT_AVS, op_val);

	return (rc < 0) ? rc : count;
}

static ssize_t isl68137_avs_enable_show(struct device *dev,
					struct device_attribute *devattr,
					char *buf)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);

	return isl68137_avs_enable_show_page(client, attr->index, buf);
}

static ssize_t isl68137_avs_enable_store(struct device *dev,
				struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);

	return isl68137_avs_enable_store_page(client, attr->index, buf, count);
}

static SENSOR_DEVICE_ATTR_RW(avs0_enable, isl68137_avs_enable, 0);
static SENSOR_DEVICE_ATTR_RW(avs1_enable, isl68137_avs_enable, 1);

static struct attribute *enable_attrs[] = {
	&sensor_dev_attr_avs0_enable.dev_attr.attr,
	&sensor_dev_attr_avs1_enable.dev_attr.attr,
	NULL,
};

static const struct attribute_group enable_group = {
	.attrs = enable_attrs,
};

static const struct attribute_group *isl68137_attribute_groups[] = {
	&enable_group,
	NULL,
};

/**
 * Non-standard SMBus read function to account for I2C controllers that
 * do not support SMBus block reads. Reads 5 bytes from client (length + 4 data
 * bytes)
 */
static s32 raa_smbus_read40(const struct i2c_client *client, u8 command,
			    unsigned char *data)
{
	int status;
	unsigned char msgbuf[1];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = msgbuf,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = 5,
			.buf = data,
		},
	};

	msgbuf[0] = command;
	status = i2c_transfer(client->adapter, msg, 2);
	if (status != 2)
		return status;
	return 0;
}

/**
 * Helper function required since linux SMBus implementation does not currently
 * (v5.6) support the SMBus 3.0 "Read 32" protocol
 */
static s32 raa_dmpvr2_smbus_read32(const struct i2c_client *client, u8 command,
				   unsigned char *data)
{
	int status;
	unsigned char msgbuf[1];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = msgbuf,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = 4,
			.buf = data,
		},
	};

	msgbuf[0] = command;
	status = i2c_transfer(client->adapter, msg, 2);
	if (status != 2)
		return status;
	return 0;
}

/**
 * Helper function required since linux SMBus implementation does not currently
 * (v5.6) support the SMBus 3.0 "Write 32" protocol
 */
static s32 raa_dmpvr2_smbus_write32(const struct i2c_client *client,
				    u8 command, u32 value)
{
	int status;
	unsigned char msgbuf[5];
	struct i2c_msg msg[1] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 5,
			.buf = msgbuf,
		},
	};
	msgbuf[0] = command;
	msgbuf[1] = value & 0x0FF;
	msgbuf[2] = (value >> 8) & 0x0FF;
	msgbuf[3] = (value >> 16) & 0x0FF;
	msgbuf[4] = (value >> 24) &  0x0FF;
	status = i2c_transfer(client->adapter, msg, 1);
	if (status != 1)
		return status;
	return 0;
}

static ssize_t raa_dmpvr2_read_black_box(struct raa_dmpvr2_ctrl *ctrl,
					 char __user *buf, size_t len,
					 loff_t *ppos)
{
	int i, j;
	u16 addr = RAA_DMPVR2_BB_BASE_ADDR;
	unsigned char data[RAA_DMPVR2_BB_WSIZE] = { 0 };
	char out[RAA_DMPVR2_BB_BUF_SIZE] = { 0 };
	char *optr = out;

	i2c_smbus_write_word_data(ctrl->client, RAA_DMPVR2_DMA_ADDR, addr);
	for (i = 0; i < RAA_DMPVR2_BB_WCNT; i++) {
		raa_dmpvr2_smbus_read32(ctrl->client, RAA_DMPVR2_DMA_SEQ,
					data);
		for (j = 0; j < RAA_DMPVR2_BB_WSIZE; j++) {
			optr += snprintf(optr, sizeof(out), "%02X", data[j]);
		}
		*optr = '\n';
		optr++;
	}

	return simple_read_from_buffer(buf, len, ppos, &out, sizeof(out));
}

struct raa_dmpvr_cfg_cmd {
	u8 cmd;
	u8 len;
	u8 data[4];
};

struct raa_dmpvr2_cfg {
	u8 dev_id[4];
	u8 dev_rev[4];
	int slot_cnt;
	int cmd_cnt;
	struct raa_dmpvr_cfg_cmd *cmds;
	u8 crc[4];
};

/**
 * Helper function to handle hex string to byte conversions
 */
static int raa_dmpvr2_hextou8(char *buf, u8 *res)
{
	char s[3];
	s[0] = buf[0];
	s[1] = buf[1];
	s[2] = '\0';
	return kstrtou8(s, 16, res);
}

static int raa_dmpvr2_parse_cfg(char *buf, struct raa_dmpvr2_cfg *cfg)
{
	const int lsta = 2;
	const int csta = 6;
	const int dsta = 8;
	char *cptr, *line;
	int lcnt = 1;
	int ccnt = 0;
	int i, j, ret;
	u8 b;

	// Ensure there is enough memory for the file
	for (i = 0; i < strlen(buf); i++) {
		if (buf[i] == '\n' && i < strlen(buf)-2) {
			lcnt++;
			if (buf[i+1] != '4' || buf[i+2] != '9')
				ccnt++;
		}
	}
	cfg->slot_cnt = (lcnt-RAA_DMPVR2_CFG_HEAD_LEN)/RAA_DMPVR2_CFG_SLOT_LEN;
	if (cfg->slot_cnt < 1 || cfg->slot_cnt > RAA_DMPVR2_CFG_MAX_SLOT)
		return -EINVAL;
	cfg->cmd_cnt = ccnt;
	cfg->cmds = kmalloc(ccnt*sizeof(struct raa_dmpvr_cfg_cmd), GFP_KERNEL);
	if (!cfg->cmds)
		return -ENOMEM;

	// Parse header
	for (i = 0; i < 2; i++) {
		line = strsep(&buf, "\n");
		cptr = line + dsta;
		for (j = 3; j >= 0; j--) {
			ret = raa_dmpvr2_hextou8(cptr, &b);
			if (ret)
				goto parse_err;
			if (i == 0) {
				cfg->dev_id[j] = b;
			} else {
				cfg->dev_rev[j] = b;
			}
			cptr += 2;
		}
	}

	// Parse cmds
	i = 0;
	while (line != NULL && strlen(line) > (dsta + 2)) {
		if (strncmp(line, "49", 2) != 0) {
			ret = raa_dmpvr2_hextou8(line+lsta, &b);
			if (ret)
				goto parse_err;
			cfg->cmds[i].len = b - 3;
			ret = raa_dmpvr2_hextou8(line+csta, &b);
			if (ret)
				goto parse_err;
			cfg->cmds[i].cmd = b;
			for (j = 0; j < cfg->cmds[i].len; j++) {
				cptr = line + dsta + (2 * j);
				ret = raa_dmpvr2_hextou8(cptr, &b);
				if (ret)
					goto parse_err;
				cfg->cmds[i].data[j] = b;
			}
			i++;
		}
		line = strsep(&buf, "\n");
	}
	return 0;

parse_err:
	kfree(cfg->cmds);
	return ret;
}

static int raa_dmpvr2_verify_device(struct raa_dmpvr2_ctrl *ctrl,
				    struct raa_dmpvr2_cfg *cfg)
{
	u8 dev_id[5];
	u8 dev_rev[5];
	int status;

	status = raa_smbus_read40(ctrl->client, PMBUS_IC_DEVICE_ID, dev_id);
	if (status)
		return status;

	status = raa_smbus_read40(ctrl->client, PMBUS_IC_DEVICE_REV, dev_rev);
	if (status)
		return status;

	return memcmp(cfg->dev_id, dev_id+1, 4)
		| memcmp(cfg->dev_rev+3, dev_rev+4, 1);
}

static int raa_dmpvr2_check_cfg(struct raa_dmpvr2_ctrl *ctrl,
				struct raa_dmpvr2_cfg *cfg)
{
	u8 data[4];

	i2c_smbus_write_word_data(ctrl->client, RAA_DMPVR2_DMA_ADDR,
				  RAA_DMPVR2_NVM_CNT_ADDR);
	raa_dmpvr2_smbus_read32(ctrl->client, RAA_DMPVR2_DMA_SEQ, data);
	if (cfg->slot_cnt > data[0])
		return -EINVAL;
	return 0;
}

static int raa_dmpvr2_send_cfg(struct raa_dmpvr2_ctrl *ctrl,
			       struct raa_dmpvr2_cfg *cfg)
{
	int i, status;
	u16 word;
	u32 dbl_word;

	for (i = 0; i < cfg->cmd_cnt; i++) {
		if (cfg->cmds[i].len == 2) {
			word = cfg->cmds[i].data[0]
				| (cfg->cmds[i].data[1] << 8);
			status = i2c_smbus_write_word_data(ctrl->client,
							   cfg->cmds[i].cmd,
							   word);
			if (status < 0)
				return status;
		}
		else if (cfg->cmds[i].len == 4) {
			dbl_word = cfg->cmds[i].data[0]
				| (cfg->cmds[i].data[1] << 8)
				| (cfg->cmds[i].data[2] << 16)
				| (cfg->cmds[i].data[3] << 24);
			status = raa_dmpvr2_smbus_write32(ctrl->client,
							  cfg->cmds[i].cmd,
							  dbl_word);
			if (status < 0)
				return status;
		} else {
			return -EINVAL;
		}
	}

	return 0;
}

static int raa_dmpvr2_cfg_write_result(struct raa_dmpvr2_ctrl *ctrl,
				       struct raa_dmpvr2_cfg *cfg)
{
	u8 data[4] = {0};
	u8 data1[4];
	u8 *dptr;
	unsigned long start;
	int i, j, status;

	// Check programmer status
	start = jiffies;
	i2c_smbus_write_word_data(ctrl->client, RAA_DMPVR2_DMA_ADDR,
				  RAA_DMPVR2_PRGM_STATUS_ADDR);
	while (data[0] == 0 && !time_after(jiffies, start + HZ + HZ)) {
		raa_dmpvr2_smbus_read32(ctrl->client, RAA_DMPVR2_DMA_FIX,
					data);
	}
	if (data[0] != 1)
		return -ETIME;

	// Check bank statuses
	i2c_smbus_write_word_data(ctrl->client, RAA_DMPVR2_DMA_ADDR,
				  RAA_DMPVR2_BANK0_STATUS_ADDR);
	raa_dmpvr2_smbus_read32(ctrl->client, RAA_DMPVR2_DMA_FIX, data);
	i2c_smbus_write_word_data(ctrl->client, RAA_DMPVR2_DMA_ADDR,
				  RAA_DMPVR2_BANK1_STATUS_ADDR);
	raa_dmpvr2_smbus_read32(ctrl->client, RAA_DMPVR2_DMA_FIX, data1);

	for (i = 0; i < cfg->slot_cnt; i++) {
		if (i < 8) {
			j = i;
			dptr = data;
		} else {
			j = i - 8;
			dptr = data1;
		}
		status = (dptr[j/2] >> (4 * (j % 2))) & 0x0F;
		if (status != 1)
			return -EIO;
	}

	return 0;
}

static ssize_t raa_dmpvr2_write_cfg(struct raa_dmpvr2_ctrl *ctrl,
				    const char __user *buf, size_t len,
				    loff_t *ppos)
{
	char *cbuf;
	int ret, status;
	struct raa_dmpvr2_cfg *cfg;

	cfg = kmalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cbuf = kmalloc(len, GFP_KERNEL);
	if (!cbuf) {
		status = -ENOMEM;
		goto buf_err;
	}
	ret = simple_write_to_buffer(cbuf, len, ppos, buf, len);

	// Parse file
	status = raa_dmpvr2_parse_cfg(cbuf, cfg);
	if (status)
		goto parse_err;
	// Verify device and file IDs/revs match
	status = raa_dmpvr2_verify_device(ctrl, cfg);
	if (status)
		goto dev_err;
	// Verify enough of NVM slots available
	status = raa_dmpvr2_check_cfg(ctrl, cfg);
	if (status)
		goto dev_err;
	// Write CFG to device
	status = raa_dmpvr2_send_cfg(ctrl, cfg);
	if (status)
		goto dev_err;
	// Verify programming success
	status = raa_dmpvr2_cfg_write_result(ctrl, cfg);
	if (status)
		goto dev_err;
	// Free memory
	kfree(cbuf);
	kfree(cfg->cmds);
	kfree(cfg);
	return ret;

	// Handle Errors
dev_err:
	kfree(cfg->cmds);
parse_err:
	kfree(cbuf);
buf_err:
	kfree(cfg);
	return status;
}

static ssize_t raa_dmpvr2_debugfs_read(struct file *file, char __user *buf,
				       size_t len, loff_t *ppos)
{
	int *idxp = file->private_data;
	int idx = *idxp;
	struct raa_dmpvr2_ctrl *ctrl = to_ctrl(idxp, idx);
	switch (idx)
	{
	case RAA_DMPVR2_DEBUGFS_BB_R:
		return raa_dmpvr2_read_black_box(ctrl, buf, len, ppos);
	default:
		return -EINVAL;
	}
}

static ssize_t raa_dmpvr2_debugfs_write(struct file *file,
					const char __user *buf, size_t len,
					loff_t *ppos)
{
	int *idxp = file->private_data;
	int idx = *idxp;
	struct raa_dmpvr2_ctrl *ctrl = to_ctrl(idxp, idx);
	switch (idx)
	{
	case RAA_DMPVR2_DEBUGFS_CFG_W:
		return raa_dmpvr2_write_cfg(ctrl, buf, len, ppos);
	default:
		return -EINVAL;
	}
}

static const struct file_operations raa_dmpvr2_debugfs_fops = {
	.llseek = noop_llseek,
	.read = raa_dmpvr2_debugfs_read,
	.write = raa_dmpvr2_debugfs_write,
	.open = simple_open,
};

static int raa_dmpvr2_read_word_data(struct i2c_client *client, int page,
				     int phase, int reg)
{
	int ret;

	switch (reg) {
	case PMBUS_VIRT_READ_VMON:
		ret = pmbus_read_word_data(client, page, phase,
					   RAA_DMPVR2_READ_VMON);
		break;
	default:
		ret = -ENODATA;
		break;
	}

	return ret;
}

static struct pmbus_driver_info raa_dmpvr_info = {
	.pages = 3,
	.format[PSC_VOLTAGE_IN] = direct,
	.format[PSC_VOLTAGE_OUT] = direct,
	.format[PSC_CURRENT_IN] = direct,
	.format[PSC_CURRENT_OUT] = direct,
	.format[PSC_POWER] = direct,
	.format[PSC_TEMPERATURE] = direct,
	.m[PSC_VOLTAGE_IN] = 1,
	.b[PSC_VOLTAGE_IN] = 0,
	.R[PSC_VOLTAGE_IN] = 2,
	.m[PSC_VOLTAGE_OUT] = 1,
	.b[PSC_VOLTAGE_OUT] = 0,
	.R[PSC_VOLTAGE_OUT] = 3,
	.m[PSC_CURRENT_IN] = 1,
	.b[PSC_CURRENT_IN] = 0,
	.R[PSC_CURRENT_IN] = 2,
	.m[PSC_CURRENT_OUT] = 1,
	.b[PSC_CURRENT_OUT] = 0,
	.R[PSC_CURRENT_OUT] = 1,
	.m[PSC_POWER] = 1,
	.b[PSC_POWER] = 0,
	.R[PSC_POWER] = 0,
	.m[PSC_TEMPERATURE] = 1,
	.b[PSC_TEMPERATURE] = 0,
	.R[PSC_TEMPERATURE] = 0,
	.func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_IIN | PMBUS_HAVE_PIN
	    | PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2
	    | PMBUS_HAVE_TEMP3 | PMBUS_HAVE_STATUS_TEMP
	    | PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT
	    | PMBUS_HAVE_IOUT | PMBUS_HAVE_STATUS_IOUT | PMBUS_HAVE_POUT
		| PMBUS_HAVE_VMON,
	.func[1] = PMBUS_HAVE_IIN | PMBUS_HAVE_PIN | PMBUS_HAVE_STATUS_INPUT
	    | PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP3 | PMBUS_HAVE_STATUS_TEMP
	    | PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_IOUT
	    | PMBUS_HAVE_STATUS_IOUT | PMBUS_HAVE_POUT,
	.func[2] = PMBUS_HAVE_IIN | PMBUS_HAVE_PIN | PMBUS_HAVE_STATUS_INPUT
	    | PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP3 | PMBUS_HAVE_STATUS_TEMP
	    | PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_IOUT
	    | PMBUS_HAVE_STATUS_IOUT | PMBUS_HAVE_POUT,
};

static int isl68137_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int i, ret;
	struct pmbus_driver_info *info;
	struct dentry *debugfs;
	struct dentry *debug_dir;
	struct raa_dmpvr2_ctrl *ctrl;

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	memcpy(info, &raa_dmpvr_info, sizeof(*info));

	switch (id->driver_data) {
	case raa_dmpvr1_2rail:
		info->pages = 2;
		info->R[PSC_VOLTAGE_IN] = 3;
		info->func[0] &= ~PMBUS_HAVE_VMON;
		info->func[1] = PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT
		    | PMBUS_HAVE_IOUT | PMBUS_HAVE_STATUS_IOUT
		    | PMBUS_HAVE_POUT;
		info->groups = isl68137_attribute_groups;
		break;
	case raa_dmpvr2_1rail:
		info->pages = 1;
		info->read_word_data = raa_dmpvr2_read_word_data;
		break;
	case raa_dmpvr2_2rail:
		info->pages = 2;
		info->read_word_data = raa_dmpvr2_read_word_data;
		break;
	case raa_dmpvr2_3rail:
		info->read_word_data = raa_dmpvr2_read_word_data;
		break;
	case raa_dmpvr2_hv:
		info->pages = 1;
		info->R[PSC_VOLTAGE_IN] = 1;
		info->m[PSC_VOLTAGE_OUT] = 2;
		info->R[PSC_VOLTAGE_OUT] = 2;
		info->m[PSC_CURRENT_IN] = 2;
		info->m[PSC_POWER] = 2;
		info->R[PSC_POWER] = -1;
		info->read_word_data = raa_dmpvr2_read_word_data;
		break;
	default:
		return -ENODEV;
	}

	// No debugfs features for Gen 1
	if (id->driver_data == raa_dmpvr1_2rail)
		return pmbus_do_probe(client, id, info);

	ret = pmbus_do_probe(client, id, info);
	if (ret != 0)
		return ret;

	ctrl = devm_kzalloc(&client->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return 0;

	ctrl->client = client;
	debugfs = pmbus_get_debugfs_dir(client);
	if (!debugfs)
		return 0;

	debug_dir = debugfs_create_dir(client->name, debugfs);
	if (!debug_dir)
		return 0;

	for (i = 0; i < RAA_DMPVR2_DEBUGFS_NUM_ENTRIES; i++)
		ctrl->debugfs_entries[i] = i;

	debugfs_create_file("write_config", 0222, debug_dir,
			    &ctrl->debugfs_entries[RAA_DMPVR2_DEBUGFS_CFG_W],
			    &raa_dmpvr2_debugfs_fops);
	debugfs_create_file("read_black_box", 0444, debug_dir,
			    &ctrl->debugfs_entries[RAA_DMPVR2_DEBUGFS_BB_R],
			    &raa_dmpvr2_debugfs_fops);
	return 0;
}

static const struct i2c_device_id raa_dmpvr_id[] = {
	{"isl68137", raa_dmpvr1_2rail},
	{"isl68220", raa_dmpvr2_2rail},
	{"isl68221", raa_dmpvr2_3rail},
	{"isl68222", raa_dmpvr2_2rail},
	{"isl68223", raa_dmpvr2_2rail},
	{"isl68224", raa_dmpvr2_3rail},
	{"isl68225", raa_dmpvr2_2rail},
	{"isl68226", raa_dmpvr2_3rail},
	{"isl68227", raa_dmpvr2_1rail},
	{"isl68229", raa_dmpvr2_3rail},
	{"isl68233", raa_dmpvr2_2rail},
	{"isl68239", raa_dmpvr2_3rail},

	{"isl69222", raa_dmpvr2_2rail},
	{"isl69223", raa_dmpvr2_3rail},
	{"isl69224", raa_dmpvr2_2rail},
	{"isl69225", raa_dmpvr2_2rail},
	{"isl69227", raa_dmpvr2_3rail},
	{"isl69228", raa_dmpvr2_3rail},
	{"isl69234", raa_dmpvr2_2rail},
	{"isl69236", raa_dmpvr2_2rail},
	{"isl69239", raa_dmpvr2_3rail},
	{"isl69242", raa_dmpvr2_2rail},
	{"isl69243", raa_dmpvr2_1rail},
	{"isl69247", raa_dmpvr2_2rail},
	{"isl69248", raa_dmpvr2_2rail},
	{"isl69254", raa_dmpvr2_2rail},
	{"isl69255", raa_dmpvr2_2rail},
	{"isl69256", raa_dmpvr2_2rail},
	{"isl69259", raa_dmpvr2_2rail},
	{"isl69260", raa_dmpvr2_2rail},
	{"isl69268", raa_dmpvr2_2rail},
	{"isl69269", raa_dmpvr2_3rail},
	{"isl69298", raa_dmpvr2_2rail},

	{"raa228000", raa_dmpvr2_hv},
	{"raa228004", raa_dmpvr2_hv},
	{"raa228006", raa_dmpvr2_hv},
	{"raa228228", raa_dmpvr2_2rail},
	{"raa229001", raa_dmpvr2_2rail},
	{"raa229004", raa_dmpvr2_2rail},
	{}
};

MODULE_DEVICE_TABLE(i2c, raa_dmpvr_id);

/* This is the driver that will be inserted */
static struct i2c_driver isl68137_driver = {
	.driver = {
		   .name = "isl68137",
		   },
	.probe = isl68137_probe,
	.remove = pmbus_do_remove,
	.id_table = raa_dmpvr_id,
};

module_i2c_driver(isl68137_driver);

MODULE_AUTHOR("Maxim Sloyko <maxims@google.com>");
MODULE_DESCRIPTION("PMBus driver for Renesas digital multiphase voltage regulators");
MODULE_LICENSE("GPL");
