// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for handling externally connected matrix keyboard.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/bcd.h>

#define REG_SECS		0x00	/* 00-59 */
#define REG_MIN		0x01	/* 00-59 */
#define REG_HOUR		0x02	/* 00-23, or 1-12{am,pm} */
#define REG_WDAY		0x03	/* 01-07 */
#define REG_MDAY		0x04	/* 01-31 */
#define REG_MONTH	0x05	/* 01-12 */
#define REG_YEAR		0x06	/* 00-99 */

struct ds1307x {
	struct i2c_client *i2c_rtc;
	struct rtc_device *rtc_ds1307;
};

static int ds1307_read_time(struct device *dev, struct rtc_time *tm)
{
	u8 ret;
	u8 reg = 0x00;		/* I2C register */
	u8 buffer[7];			/* where to read */
	struct ds1307x *ds1307x = dev_get_drvdata(dev);

	struct i2c_msg msg[2] = {
		{
			.addr = ds1307x->i2c_rtc->addr,
			.flags = 0,		/* write */
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = ds1307x->i2c_rtc->addr,
			.flags = I2C_M_RD,	/* read */
			.len = 7,		/* 7 time regs in device */
			.buf = buffer,
		}
	};

	ret = i2c_transfer(ds1307x->i2c_rtc->adapter, msg, 2);

	tm->tm_sec = bcd2bin(buffer[REG_SECS] & 0x7f);
	tm->tm_min = bcd2bin(buffer[REG_MIN] & 0x7f);
	tm->tm_hour = bcd2bin(buffer[REG_HOUR] & 0x3f);
	tm->tm_wday = bcd2bin(buffer[REG_WDAY] & 0x07) - 1;
	tm->tm_mday = bcd2bin(buffer[REG_MDAY] & 0x3f);
	tm->tm_mon = bcd2bin(buffer[REG_MONTH] & 0x1f) - 1;
	tm->tm_year = bcd2bin(buffer[REG_YEAR]) + 100;

	return 0;
}

static int ds1307_set_time(struct device *dev, struct rtc_time *tm)
{
	u8 ret;
	u8 reg = 0x00;		/* I2C register */
	u8 buffer[7];			/* where to read */
	struct ds1307x *ds1307x = dev_get_drvdata(dev);

	struct i2c_msg msg[2] = {
		{
			.addr = ds1307x->i2c_rtc->addr,
			.flags = 0,		/* write */
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = ds1307x->i2c_rtc->addr,
			.flags = 0,		/* write */
			.len = 7,		/* 7 time regs in device */
			.buf = buffer,
		}
	};

	buffer[REG_SECS] = bin2bcd(tm->tm_sec);
	buffer[REG_MIN] = bin2bcd(tm->tm_min);
	buffer[REG_HOUR] = bin2bcd(tm->tm_hour);
	buffer[REG_WDAY] = bin2bcd(tm->tm_wday + 1);
	buffer[REG_MDAY] = bin2bcd(tm->tm_mday);
	buffer[REG_MONTH] = bin2bcd(tm->tm_mon + 1);
	buffer[REG_YEAR] = bin2bcd(tm->tm_year - 100);
	ret = i2c_transfer(ds1307x->i2c_rtc->adapter, msg, 2);

	return 0;
}

static const struct rtc_class_ops ds1307_rtc_ops = {
	.read_time = ds1307_read_time,
	.set_time = ds1307_set_time,
//	int (* read_alarm)(struct device *, struct rtc_wkalrm *);
//	int (* set_alarm)(struct device *, struct rtc_wkalrm *);
//	int (* alarm_irq_enable)(struct device *, unsigned int enabled);
};


static int ds1307x_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int err;
	struct ds1307x *ds1307x;

	ds1307x = devm_kzalloc(&client->dev, sizeof(struct ds1307x), GFP_KERNEL);
	if (!ds1307x)
		return -ENOMEM;

	ds1307x->i2c_rtc = client;
	ds1307x->rtc_ds1307 = devm_rtc_allocate_device(&client->dev);
	if (IS_ERR(ds1307x->rtc_ds1307))
		return PTR_ERR(ds1307x->rtc_ds1307);

	ds1307x->rtc_ds1307->uie_unsupported = 1;	/* no IRQ line = no Update Interrupt Enable */
	ds1307x->rtc_ds1307->ops = &ds1307_rtc_ops;

	dev_set_drvdata(&client->dev, ds1307x);

	err = rtc_register_device(ds1307x->rtc_ds1307);
	if (err)
		return err;

	return 0;
}

static int ds1307x_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ds1307x_id[] = {
	{ "ds1307x" },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ds1307x_id);

static const struct of_device_id ds1307x_of_match[] = {
	{ .compatible = "globallogic ,ds1307x" },	/* ds1307 already exists */
	{ }
};

MODULE_DEVICE_TABLE(of, ds1307x_of_match);

static struct i2c_driver ds1307x_driver = {
	.driver = {
		.name = "ds1307x",
		.of_match_table = ds1307x_of_match,
	},
	.probe = ds1307x_probe,
	.remove = ds1307x_remove,
	.id_table = ds1307x_id,
};

module_i2c_driver(ds1307x_driver);

MODULE_ALIAS("rtc");
MODULE_AUTHOR("Max Kokhan");
MODULE_DESCRIPTION("Homework: Real Time Clock");
MODULE_LICENSE("GPL");
