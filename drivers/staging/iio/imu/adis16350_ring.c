#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/list.h>

#include "../iio.h"
#include "../sysfs.h"
#include "../ring_sw.h"
#include "../accel/accel.h"
#include "../trigger.h"
#include "adis16350.h"

static IIO_SCAN_EL_C(in0_supply, ADIS16350_SCAN_SUPPLY,
		ADIS16350_SUPPLY_OUT, NULL);
static IIO_CONST_ATTR_SCAN_EL_TYPE(in0_supply, u, 12, 16);

static IIO_SCAN_EL_C(gyro_x, ADIS16350_SCAN_GYRO_X, ADIS16350_XGYRO_OUT, NULL);
static IIO_SCAN_EL_C(gyro_y, ADIS16350_SCAN_GYRO_Y, ADIS16350_YGYRO_OUT, NULL);
static IIO_SCAN_EL_C(gyro_z, ADIS16350_SCAN_GYRO_Z, ADIS16350_ZGYRO_OUT, NULL);
static IIO_CONST_ATTR_SCAN_EL_TYPE(gyro, s, 14, 16);

static IIO_SCAN_EL_C(accel_x, ADIS16350_SCAN_ACC_X, ADIS16350_XACCL_OUT, NULL);
static IIO_SCAN_EL_C(accel_y, ADIS16350_SCAN_ACC_Y, ADIS16350_YACCL_OUT, NULL);
static IIO_SCAN_EL_C(accel_z, ADIS16350_SCAN_ACC_Z, ADIS16350_ZACCL_OUT, NULL);
static IIO_CONST_ATTR_SCAN_EL_TYPE(accel, s, 14, 16);

static IIO_SCAN_EL_C(temp_x, ADIS16350_SCAN_TEMP_X, ADIS16350_XTEMP_OUT, NULL);
static IIO_SCAN_EL_C(temp_y, ADIS16350_SCAN_TEMP_Y, ADIS16350_YTEMP_OUT, NULL);
static IIO_SCAN_EL_C(temp_z, ADIS16350_SCAN_TEMP_Z, ADIS16350_ZTEMP_OUT, NULL);
static IIO_CONST_ATTR_SCAN_EL_TYPE(temp, s, 12, 16);

static IIO_SCAN_EL_C(in1, ADIS16350_SCAN_ADC_0, ADIS16350_AUX_ADC, NULL);
static IIO_CONST_ATTR_SCAN_EL_TYPE(in1, u, 12, 16);

static IIO_SCAN_EL_TIMESTAMP(11);
static IIO_CONST_ATTR_SCAN_EL_TYPE(timestamp, s, 64, 64);

static struct attribute *adis16350_scan_el_attrs[] = {
	&iio_scan_el_in0_supply.dev_attr.attr,
	&iio_const_attr_in0_supply_index.dev_attr.attr,
	&iio_const_attr_in0_supply_type.dev_attr.attr,
	&iio_scan_el_gyro_x.dev_attr.attr,
	&iio_const_attr_gyro_x_index.dev_attr.attr,
	&iio_scan_el_gyro_y.dev_attr.attr,
	&iio_const_attr_gyro_y_index.dev_attr.attr,
	&iio_scan_el_gyro_z.dev_attr.attr,
	&iio_const_attr_gyro_z_index.dev_attr.attr,
	&iio_const_attr_gyro_type.dev_attr.attr,
	&iio_scan_el_accel_x.dev_attr.attr,
	&iio_const_attr_accel_x_index.dev_attr.attr,
	&iio_scan_el_accel_y.dev_attr.attr,
	&iio_const_attr_accel_y_index.dev_attr.attr,
	&iio_scan_el_accel_z.dev_attr.attr,
	&iio_const_attr_accel_z_index.dev_attr.attr,
	&iio_const_attr_accel_type.dev_attr.attr,
	&iio_scan_el_temp_x.dev_attr.attr,
	&iio_const_attr_temp_x_index.dev_attr.attr,
	&iio_scan_el_temp_y.dev_attr.attr,
	&iio_const_attr_temp_y_index.dev_attr.attr,
	&iio_scan_el_temp_z.dev_attr.attr,
	&iio_const_attr_temp_z_index.dev_attr.attr,
	&iio_const_attr_temp_type.dev_attr.attr,
	&iio_scan_el_in1.dev_attr.attr,
	&iio_const_attr_in1_index.dev_attr.attr,
	&iio_const_attr_in1_type.dev_attr.attr,
	&iio_scan_el_timestamp.dev_attr.attr,
	&iio_const_attr_timestamp_index.dev_attr.attr,
	&iio_const_attr_timestamp_type.dev_attr.attr,
	NULL,
};

static struct attribute_group adis16350_scan_el_group = {
	.attrs = adis16350_scan_el_attrs,
	.name = "scan_elements",
};

/**
 * adis16350_poll_func_th() top half interrupt handler called by trigger
 * @private_data:	iio_dev
 **/
static void adis16350_poll_func_th(struct iio_dev *indio_dev, s64 time)
{
	struct adis16350_state *st = iio_dev_get_devdata(indio_dev);
	st->last_timestamp = time;
	schedule_work(&st->work_trigger_to_ring);
}

/**
 * adis16350_spi_read_burst() - read all data registers
 * @dev: device associated with child of actual device (iio_dev or iio_trig)
 * @rx: somewhere to pass back the value read (min size is 24 bytes)
 **/
static int adis16350_spi_read_burst(struct device *dev, u8 *rx)
{
	struct spi_message msg;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct adis16350_state *st = iio_dev_get_devdata(indio_dev);
	u32 old_speed_hz = st->us->max_speed_hz;
	int ret;

	struct spi_transfer xfers[] = {
		{
			.tx_buf = st->tx,
			.bits_per_word = 8,
			.len = 2,
			.cs_change = 0,
		}, {
			.rx_buf = rx,
			.bits_per_word = 8,
			.len = 22,
			.cs_change = 0,
		},
	};

	mutex_lock(&st->buf_lock);
	st->tx[0] = ADIS16350_READ_REG(ADIS16350_GLOB_CMD);
	st->tx[1] = 0;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);

	st->us->max_speed_hz = ADIS16350_SPI_BURST;
	spi_setup(st->us);

	ret = spi_sync(st->us, &msg);
	if (ret)
		dev_err(&st->us->dev, "problem when burst reading");

	st->us->max_speed_hz = old_speed_hz;
	spi_setup(st->us);
	mutex_unlock(&st->buf_lock);
	return ret;
}

/* Whilst this makes a lot of calls to iio_sw_ring functions - it is to device
 * specific to be rolled into the core.
 */
static void adis16350_trigger_bh_to_ring(struct work_struct *work_s)
{
	struct adis16350_state *st
		= container_of(work_s, struct adis16350_state,
			       work_trigger_to_ring);
	struct iio_ring_buffer *ring = st->indio_dev->ring;

	int i = 0;
	s16 *data;
	size_t datasize = ring->access.get_bytes_per_datum(ring);

	data = kmalloc(datasize , GFP_KERNEL);
	if (data == NULL) {
		dev_err(&st->us->dev, "memory alloc failed in ring bh");
		return;
	}

	if (ring->scan_count)
		if (adis16350_spi_read_burst(&st->indio_dev->dev, st->rx) >= 0)
			for (; i < ring->scan_count; i++)
				data[i] = be16_to_cpup(
					(__be16 *)&(st->rx[i*2]));

	/* Guaranteed to be aligned with 8 byte boundary */
	if (ring->scan_timestamp)
		*((s64 *)(data + ((i + 3)/4)*4)) = st->last_timestamp;

	ring->access.store_to(ring,
			(u8 *)data,
			st->last_timestamp);

	iio_trigger_notify_done(st->indio_dev->trig);
	kfree(data);

	return;
}

void adis16350_unconfigure_ring(struct iio_dev *indio_dev)
{
	kfree(indio_dev->pollfunc);
	iio_sw_rb_free(indio_dev->ring);
}

int adis16350_configure_ring(struct iio_dev *indio_dev)
{
	int ret = 0;
	struct adis16350_state *st = indio_dev->dev_data;
	struct iio_ring_buffer *ring;
	INIT_WORK(&st->work_trigger_to_ring, adis16350_trigger_bh_to_ring);

	ring = iio_sw_rb_allocate(indio_dev);
	if (!ring) {
		ret = -ENOMEM;
		return ret;
	}
	indio_dev->ring = ring;
	/* Effectively select the ring buffer implementation */
	iio_ring_sw_register_funcs(&ring->access);
	ring->bpe = 2;
	ring->scan_el_attrs = &adis16350_scan_el_group;
	ring->scan_timestamp = true;
	ring->preenable = &iio_sw_ring_preenable;
	ring->postenable = &iio_triggered_ring_postenable;
	ring->predisable = &iio_triggered_ring_predisable;
	ring->owner = THIS_MODULE;

	/* Set default scan mode */
	iio_scan_mask_set(ring, iio_scan_el_in0_supply.number);
	iio_scan_mask_set(ring, iio_scan_el_gyro_x.number);
	iio_scan_mask_set(ring, iio_scan_el_gyro_y.number);
	iio_scan_mask_set(ring, iio_scan_el_gyro_z.number);
	iio_scan_mask_set(ring, iio_scan_el_accel_x.number);
	iio_scan_mask_set(ring, iio_scan_el_accel_y.number);
	iio_scan_mask_set(ring, iio_scan_el_accel_z.number);
	iio_scan_mask_set(ring, iio_scan_el_temp_x.number);
	iio_scan_mask_set(ring, iio_scan_el_temp_y.number);
	iio_scan_mask_set(ring, iio_scan_el_temp_z.number);
	iio_scan_mask_set(ring, iio_scan_el_in1.number);

	ret = iio_alloc_pollfunc(indio_dev, NULL, &adis16350_poll_func_th);
	if (ret)
		goto error_iio_sw_rb_free;

	indio_dev->modes |= INDIO_RING_TRIGGERED;
	return 0;

error_iio_sw_rb_free:
	iio_sw_rb_free(indio_dev->ring);
	return ret;
}

