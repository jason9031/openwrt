/*
 * Mediatek Pulse Width Modulator driver
 *
 * Copyright (C) 2015 John Crispin <blogic@openwrt.org>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/reboot.h>
#include <linux/hrtimer.h>


struct mtk_gpio_pwm_device
{
	unsigned int gpio;
	unsigned char gpio_value;
	struct hrtimer timer;
	struct pwm_device *pwm_dev;
	bool is_actived;
	struct mutex lock;
};

struct mtk_gpio_pwm_chip
{
	struct pwm_chip chip; 
	unsigned char gpios;
	struct mtk_gpio_pwm_device devices[];
};


static inline struct mtk_gpio_pwm_chip *to_mtk_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_gpio_pwm_chip, chip);
}

static enum hrtimer_restart mtk_pwm_function_callback(struct hrtimer *data)
{
	int delay_on = 0;
	int delay_off = 0;
	int ns = 0;
	
	struct mtk_gpio_pwm_device *mDevice = container_of(data, struct mtk_gpio_pwm_device, timer);
	struct pwm_device *pwm_dev = mDevice->pwm_dev;

	delay_on = pwm_dev->duty_cycle;
	delay_off = pwm_dev->period - pwm_dev->duty_cycle;
	if(mDevice->gpio_value>0)
	{
		ns = delay_off;
	}
	else
	{
		ns = delay_on;
	}
	
	mDevice->gpio_value = (mDevice->gpio_value > 0)?0:1;
	gpio_set_value(mDevice->gpio, mDevice->gpio_value);

	hrtimer_forward_now(&mDevice->timer, ns_to_ktime(ns));

	return HRTIMER_RESTART;
}

static int mtk_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret = 0;
	/*in pwmchip_sysfs_export, does not call pwm_get/pwm_request, so move gpio_request to pwm_enable*/
	struct mtk_gpio_pwm_chip *pChip = to_mtk_pwm_chip(chip);
	struct mtk_gpio_pwm_device *mDevice = &(pChip->devices[pwm->hwpwm]);
	printk("%s:%d\n", __FUNCTION__, __LINE__);

	gpio_free(mDevice->gpio);
	//ret = gpio_request_one(mDevice->gpio, GPIOF_DIR_OUT, pwm->label);
	ret = devm_gpio_request_one(chip->dev, mDevice->gpio, GPIOF_DIR_OUT, pwm->label);
	if (ret < 0) {
		dev_warn(chip->dev, "Unable to re quest GPIO %d: %d\n",mDevice->gpio, ret);	
	}else{
		printk(KERN_INFO "success request gpio %d\n",mDevice->gpio);
		gpio_direction_output(mDevice->gpio, 1); //设置输出的电平
	}
	
	return ret;
}

static void mtk_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	/*in pwmchip_sysfs_export, does not call pwm_get/pwm_request, so move pwm_free to pwm_disable*/
	struct mtk_gpio_pwm_chip *pChip = to_mtk_pwm_chip(chip);
	struct mtk_gpio_pwm_device *mDevice = &(pChip->devices[pwm->hwpwm]);
	printk("%s:%d\n", __FUNCTION__, __LINE__);

	mutex_lock(&mDevice->lock);
	if(mDevice->is_actived)
	{
		printk("%s:%d\n", __FUNCTION__, __LINE__);
		hrtimer_cancel(&mDevice->timer);
		mDevice->is_actived = false;
	}
	mutex_unlock(&mDevice->lock);
	gpio_free(mDevice->gpio);
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_gpio_pwm_chip *pChip = to_mtk_pwm_chip(chip);
	struct mtk_gpio_pwm_device *mDevice = &(pChip->devices[pwm->hwpwm]);

	mutex_lock(&mDevice->lock);
	if(!mDevice->is_actived)
	{
		hrtimer_init(&mDevice->timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
		mDevice->timer.function = mtk_pwm_function_callback;
		mDevice->is_actived = true;
	}	
	mutex_unlock(&mDevice->lock);
	hrtimer_start(&mDevice->timer, ktime_add_ns(ktime_get(), pwm->duty_cycle), HRTIMER_MODE_ABS);
	
	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_gpio_pwm_chip *pChip = to_mtk_pwm_chip(chip);
	struct mtk_gpio_pwm_device *mDevice = &(pChip->devices[pwm->hwpwm]);

	mutex_lock(&mDevice->lock);
	if(mDevice->is_actived)
	{
		hrtimer_cancel(&mDevice->timer);
		mDevice->is_actived = false;
	}
	mutex_unlock(&mDevice->lock);
}


static int mtk_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			    int duty_ns, int period_ns)
{
	pwm->period = period_ns;
	pwm->duty_cycle = duty_ns;

	return 0;
}

static const struct pwm_ops mtk_pwm_ops = {
	.request = mtk_pwm_request,
	.free	= mtk_pwm_free,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
	.config = mtk_pwm_config,
	.owner = THIS_MODULE,
};

static int mtk_pwm_probe(struct platform_device *pdev)
{
	int ret,i,count;
	unsigned int gpio ;
	struct mtk_gpio_pwm_chip *pmtk_gpio_pwm_chip = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	if(!node)
	{
		dev_err(&pdev->dev, "node failed\n");
		return -EINVAL;
	}
	
	count = of_gpio_count(node);
	if(count == 0)
	{
		dev_err(&pdev->dev, "node is NULL\n");
		return -EINVAL;
	}
	
	pmtk_gpio_pwm_chip = devm_kzalloc(&pdev->dev,sizeof(struct mtk_gpio_pwm_chip) + (count * sizeof(struct mtk_gpio_pwm_device)),GFP_KERNEL);
	if(pmtk_gpio_pwm_chip == NULL)
		return -ENOMEM;
	
	pmtk_gpio_pwm_chip->gpios = count;
	
	for(i=0;i<count;i++)
	{
		gpio = of_get_gpio(node,i);
		if(gpio < 0)
		{
			dev_warn(dev, "Unable to get gpio #%d\n", i);
			continue;		
		}
		
		pmtk_gpio_pwm_chip->devices[i].gpio = gpio;
		mutex_init(&(pmtk_gpio_pwm_chip->devices[i].lock));
		pmtk_gpio_pwm_chip->devices[i].is_actived = false;
		//printk(KERN_INFO "gpio %d\n",gpio);	
	}
	
	pmtk_gpio_pwm_chip->chip.dev = &pdev->dev;
	pmtk_gpio_pwm_chip->chip.ops = &mtk_pwm_ops;
	pmtk_gpio_pwm_chip->chip.base = -1;
	pmtk_gpio_pwm_chip->chip.npwm = pmtk_gpio_pwm_chip->gpios;
	
	ret = pwmchip_add(&pmtk_gpio_pwm_chip->chip);
	if (ret < 0)
	{
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}
	
	for(i=0;i< pmtk_gpio_pwm_chip->gpios;i++)
	{
		pmtk_gpio_pwm_chip->devices[i].pwm_dev  = &pmtk_gpio_pwm_chip->chip.pwms[i];
	}
	
	platform_set_drvdata(pdev, pmtk_gpio_pwm_chip);

	return 0;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	struct mtk_gpio_pwm_chip *pmtk_gpio_pwm_chip = platform_get_drvdata(pdev);
	int i;
	
	for (i = 0; i < pmtk_gpio_pwm_chip->gpios; i++)
		pwm_disable(&pmtk_gpio_pwm_chip->chip.pwms[i]);

	pwmchip_remove(&pmtk_gpio_pwm_chip->chip);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id mtk_pwm_of_match[] = {
	{ .compatible = "gpio-pwm" },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_pwm_of_match);

static struct platform_driver mtk_pwm_driver = {
	.driver = {
		.name = "mtk-gpio-pwm",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pwm_of_match,
	},
	.probe = mtk_pwm_probe,
	.remove = mtk_pwm_remove,
};

module_platform_driver(mtk_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Edward Ou <oxp@yystart.com>");
MODULE_ALIAS("platform:mt76x8-gpio-pwm");
