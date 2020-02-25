// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hardware monitoring driver for Renesas Gen 2 Digital Multiphase Devices
 *
 * Copyright (c) 2020 Renesas Electronics America
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>

#include "pmbus.h"

#define ISL692XX_READ_VMON        0xc8

enum parts {
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

enum rail_configs { high_voltage, one_rail, two_rail, three_rail };

static const struct  i2c_device_id isl692xx_id[] = {
        { "isl68220", isl68220 },
        { "isl68221", isl68221 },
        { "isl68222", isl68222 },
        { "isl68223", isl68223 },
        { "isl68224", isl68224 },
        { "isl68225", isl68225 },
        { "isl68226", isl68226 },
        { "isl68227", isl68227 },
        { "isl68229", isl68229 },
        { "isl68233", isl68233 },
        { "isl68239", isl68239 },
        { "isl69222", isl69222 },
        { "isl69223", isl69223 },
        { "isl69224", isl69224 },
        { "isl69225", isl69225 },
        { "isl69227", isl69227 },
        { "isl69228", isl69228 },
        { "isl69234", isl69234 },
        { "isl69236", isl69236 },
        { "isl69239", isl69239 },
        { "isl69242", isl69242 },
        { "isl69243", isl69243 },
        { "isl69247", isl69247 },
        { "isl69248", isl69248 },
        { "isl69254", isl69254 },
        { "isl69255", isl69255 },
        { "isl69256", isl69256 },
        { "isl69259", isl69259 },
        { "isl69260", isl69260 },
        { "isl69268", isl69268 },
        { "isl69269", isl69269 },
        { "isl69298", isl69298 },
        { "raa228000", raa228000 },
        { "raa228004", raa228004 },
        { "raa228006", raa228006 },
        { "raa228228", raa228228 },
        { "raa229001", raa229001 },
        { "raa229004", raa229004 },
        { },
};

MODULE_DEVICE_TABLE(i2c, isl692xx_id);

static int isl692xx_read_word_data(struct i2c_client *client, int page, int reg)
{
        int ret;

        switch (reg) 
        {
        case PMBUS_VIRT_READ_VMON:
                ret = pmbus_read_word_data(client, page, ISL692XX_READ_VMON);
                break;
        default:
                ret = -ENODATA;
                break;
        }
        
        return ret;
}

static struct pmbus_driver_info isl692xx_info[] = {
        [high_voltage] = {
                .pages = 1,
                .format[PSC_VOLTAGE_IN] = direct,
                .format[PSC_VOLTAGE_OUT] = direct,
                .format[PSC_CURRENT_IN] = direct,
                .format[PSC_CURRENT_OUT] = direct,
                .format[PSC_POWER] = direct,
                .format[PSC_TEMPERATURE] = direct,
                .m[PSC_VOLTAGE_IN] = 1,
                .b[PSC_VOLTAGE_IN] = 0,
                .R[PSC_VOLTAGE_IN] = 1,
                .m[PSC_VOLTAGE_OUT] = 2,
                .b[PSC_VOLTAGE_OUT] = 0,
                .R[PSC_VOLTAGE_OUT] = 2,
                .m[PSC_CURRENT_IN] = 2,
                .b[PSC_CURRENT_IN] = 0,
                .R[PSC_CURRENT_IN] = 2,
                .m[PSC_CURRENT_OUT] = 1,
                .b[PSC_CURRENT_OUT] = 0,
                .R[PSC_CURRENT_OUT] = 1,
                .m[PSC_POWER] = 2,
                .b[PSC_POWER] = 0,
                .R[PSC_POWER] = -1,
                .m[PSC_TEMPERATURE] = 1,
                .b[PSC_TEMPERATURE] = 0,
                .R[PSC_TEMPERATURE] = 0,
                .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .read_word_data = isl692xx_read_word_data,
        },
        [one_rail] = {
                .pages = 1,
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
                .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .read_word_data = isl692xx_read_word_data,
        },
        [two_rail] = {
                .pages = 2,
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
                .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .func[1] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .read_word_data = isl692xx_read_word_data,
        },
        [three_rail] = {
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
                .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .func[1] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .func[2] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_IIN |
                        PMBUS_HAVE_IOUT | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT |
                        PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3 |
                        PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT |
                        PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP |
                        PMBUS_HAVE_VMON,
                .read_word_data = isl692xx_read_word_data,
        },
};

static int isl692xx_probe(struct i2c_client *client,
                          const struct i2c_device_id *id)
{
        int ret;

        switch (id->driver_data)
        {
        case raa228000:
        case raa228004:
        case raa228006:
                ret = pmbus_do_probe(client, id, &isl692xx_info[high_voltage]);
                break;
        case isl68227:
        case isl69243:
                ret = pmbus_do_probe(client, id, &isl692xx_info[one_rail]);
                break;
        case isl68220:
        case isl68222:
        case isl68223:
        case isl68225:
        case isl68233:
        case isl69222:
        case isl69224:
        case isl69225:
        case isl69234:
        case isl69236:
        case isl69242:
        case isl69247:
        case isl69248:
        case isl69254:
        case isl69255:
        case isl69256:
        case isl69259:
        case isl69260:
        case isl69268:
        case isl69298:
        case raa228228:
        case raa229001:
        case raa229004:
                ret = pmbus_do_probe(client, id, &isl692xx_info[two_rail]);
                break;
        case isl68221:
        case isl68224:
        case isl68226:
        case isl68229:
        case isl68239:
        case isl69223:
        case isl69227:
        case isl69228:
        case isl69239:
        case isl69269:
                ret = pmbus_do_probe(client, id, &isl692xx_info[three_rail]);
                break;
        default:
                ret = -ENODEV;
        }

        return ret;
}

static struct i2c_driver isl692xx_driver = {
        .driver = {
                .name = "isl692xx",
        },
        .probe = isl692xx_probe,
        .remove = pmbus_do_remove,
        .id_table = isl692xx_id,
};

module_i2c_driver(isl692xx_driver);

MODULE_AUTHOR("Grant Peltier");
MODULE_DESCRIPTION("PMBus driver for 2nd Gen Renesas digital multiphase "
                   "devices");
MODULE_LICENSE("GPL");

