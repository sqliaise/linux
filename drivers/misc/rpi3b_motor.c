#include <linux/init.h> 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEBUG
#ifdef DEBUG
#define MOTOR_DBG(fmt, arg...)  printk(KERN_WARNING fmt, ##arg)
#else
#define MOTOR_DBG(fmt, arg...)  printk(KERN_DEBUG fmt, ##arg)
#endif

#define MOTOR_STOP       0
#define MOTOR_FORWARD    1
#define NOT_USED	 2
#define MOTOR_BACK       3
#define MOTOR_LEFT       4
#define MOTOR_RIGHT      5
#define MOTOR_LEFTFO     6
#define MOTOR_RIGHTBA    7
#define MOTOR_RIGHTFO    8
#define MOTOR_LEFTBA     9
#define MOTOR_TURNLEFT   10
#define MOTOR_TURNRIGHT  11

static const int motor_cmd[12][8] = {{0,0,0,0,0,0,0,0},
				     {1,0,1,0,1,0,1,0}, {0}, {0,1,0,1,0,1,0,1},
				     {0,1,1,0,0,1,1,0}, {1,0,0,1,1,0,0,1},
				     {0,0,1,0,0,0,1,0}, {0,0,0,1,0,0,0,1},
				     {1,0,0,0,1,0,0,0}, {0,1,0,0,0,1,0,0},
				     {1,0,0,1,0,1,1,0}, {0,1,1,0,1,0,0,1}};

struct motors {
	int motor1_pins[4];
	int motor2_pins[4];
	int motor_status;
	enum of_gpio_flags of_pins_flag;
	struct pinctrl *pwm_pinctrl;
	struct device_node *np;
	struct pinctrl_state *pwm_state;
};
struct motors *motor;

static int motor_ctrl(const int *cmd)
{
	int i;
	int motorgrp = 1;
	for (i=0;i<4;) {
		if (motorgrp == 1) {
			gpio_set_value(motor->motor1_pins[i], cmd[i]);
			i++;
			if (i == 4) {
				i = 0;
				motorgrp = 2; 
			}
		}
		else if(motorgrp == 2) {
			gpio_set_value(motor->motor2_pins[i], cmd[i+4]);
			i++;
		}
	}
	return 0;
}

static int motor_init_pins(struct motors *motor)
{
	int i;
	int ret;
	char pin_name[11];
	struct device_node *np = motor->np;
	MOTOR_DBG("[%s]:atom debug\n", __func__);
	if (!np) {
		printk("[%s]:cant find device node!\n",__func__);
		return -1;
	} 
	for (i=0;i<4;i++) {
		motor->motor1_pins[i] = of_get_named_gpio_flags(np, "motor1_pins", i, &(motor->of_pins_flag));
		motor->motor2_pins[i] = of_get_named_gpio_flags(np, "motor2_pins", i, &(motor->of_pins_flag));
		if (!motor->motor1_pins[i] || !motor->motor2_pins[i]) {
			printk("[%s]:get motor pins error!\n",__func__);
			return -1;
		}
		MOTOR_DBG("[%s]:motor1_pin:%d, motor2_pin:%d, flag:%d\n",\
				__func__, motor->motor1_pins[i], motor->motor2_pins[i], motor->of_pins_flag);
	}

	for (i=0;i<4;i++) {
		if (!gpio_is_valid(motor->motor1_pins[i]) || !gpio_is_valid(motor->motor2_pins[i])) {
			printk("[%s]:motor gpio%d is not valid!\n", __func__, i);
			return -1;
		}
		sprintf(pin_name,"motor1_pin%d",i);
		ret = gpio_request(motor->motor1_pins[i], pin_name);
		if (ret) {
			printk("[%s]:request %s error!\n", __func__, pin_name);
			return -1;
		}
		ret = gpio_direction_output(motor->motor1_pins[i], 0);
		if (ret) {
			printk("[%s]:set %s to output error!\n", __func__, pin_name);
			return -1;
		}
		sprintf(pin_name,"motor2_pin%d",i);
		ret = gpio_request(motor->motor2_pins[i], pin_name);
		if (ret) {
			printk("[%s]:request %s error!\n", __func__, pin_name);
			return -1;
		}
		ret = gpio_direction_output(motor->motor2_pins[i], 0);
		if (ret) {
			printk("[%s]:set %s to output error!\n", __func__, pin_name);
			return -1;
		}
	}
	motor->motor_status = MOTOR_STOP;
	return 0;
}

static int motor_open(struct inode *inode, struct file *file)
{
	int i;
	MOTOR_DBG("[%s]:atom===>motor debug!\n", __func__);
	for (i=0;i<4;i++) {
		gpio_set_value(motor->motor1_pins[i], 0);
		gpio_set_value(motor->motor2_pins[i], 0);
	}
	return 0;
}

static ssize_t motor_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int result;
	loff_t pos = *f_pos;
  
	if (pos >= 4) {
		result = 0;
		goto out;
	}
	if (count > (4 - pos))
		count = 4 - pos;
	pos += count;
	if (copy_to_user(buf, &(motor->motor_status) + *f_pos, count)) {
		count = -EFAULT;
		goto out;
	}
	*f_pos = pos;
	printk("motor read successfully!\n");
out:  
	return count;
}

long motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	MOTOR_DBG("[%s]:atom===> cmd=%d\n", __func__, cmd);
	if (cmd<0 || cmd>11) {
		printk("[%s]:cmd %d error!\n", __func__, cmd);
		return -1;
	}
	motor_ctrl(motor_cmd[cmd]);
	motor->motor_status = cmd;
	return 0;
}

static struct file_operations motor_fops = {
	.owner           =  THIS_MODULE,
	.open            =  motor_open,
	.read            =  motor_read,
	.unlocked_ioctl  =  motor_ioctl,
};

static int rpi3_motor_probe(struct platform_device *pdev)
{
	int err = -1;
	int ret;
	static int major;
	static struct class *motor_class;
	MOTOR_DBG("[%s]:atom debug\n", __func__);
	motor = devm_kzalloc(&pdev->dev, sizeof(*motor), GFP_KERNEL);

	motor->np = pdev->dev.of_node;
	if (!motor->np) {
		printk("[%s]:Cant find rpi3_motor form dts.\n",__func__);
		return err;
	}
	motor_init_pins(motor);
	motor->pwm_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!motor->pwm_pinctrl) {
		printk("[%s]:pinctrl not defined!\n", __func__);
		return err;
	}
	else {
		motor->pwm_state = pinctrl_lookup_state(motor->pwm_pinctrl,PINCTRL_STATE_DEFAULT);
		if (!motor->pwm_state) {
			dev_err(&pdev->dev,"pinctrl lookup failed for default state");
			return err;
		}
	}
	ret = pinctrl_select_state(motor->pwm_pinctrl,motor->pwm_state);
	if (ret) {
		printk("[%s]:pwm selecting default state error", __func__);
		return err;
	}
	platform_set_drvdata(pdev, motor);
	major = register_chrdev(0, "motor", &motor_fops);
	if (major < 0) {
		printk("[%s]:register_chrdev error, major=%d!\n", __func__, major);
		return -1;
	}
	motor_class = class_create(THIS_MODULE, "motor");
	device_create(motor_class, NULL, MKDEV(major, 0), NULL, "motor");
	return 0;
}

static int rpi3_motor_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id rpi3_motor_match_table[] = {
	{ .compatible = "rpi3_motor", },
	{ /* sentinel */ }
};

static struct platform_driver rpi3_motor_driver = {
	.probe	= rpi3_motor_probe,
	.remove	= rpi3_motor_remove,
	.driver	= {
		.name	= "rpi3_motor",
		.of_match_table = rpi3_motor_match_table,
	},
};

static int __init motor_init(void)
{
	MOTOR_DBG("[%s]:rpi3-motor init!\n",__func__);
	platform_driver_register(&rpi3_motor_driver);
	return 0;
}

static void __exit motor_exit(void)
{
	platform_driver_unregister(&rpi3_motor_driver);
}
 
module_init(motor_init);
module_exit(motor_exit);
 
MODULE_AUTHOR("sq.li");
MODULE_LICENSE("GPL");
