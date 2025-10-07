// drivers/input/uinput_touch/uinput_touch.c
#include <linux/module.h>
#include <linux/input.h>

static struct input_dev *virt_touch;

static int __init virt_touch_init(void)
{
    int err;
    virt_touch = input_allocate_device();
    if (!virt_touch)
        return -ENOMEM;

    virt_touch->name = "Virtual Touch Device";
    virt_touch->id.bustype = BUS_VIRTUAL;
    virt_touch->evbit[0] = BIT_MASK(EV_ABS);
    set_bit(ABS_X, virt_touch->absbit);
    set_bit(ABS_Y, virt_touch->absbit);

    input_set_abs_params(virt_touch, ABS_X, 0, 1080, 0, 0);
    input_set_abs_params(virt_touch, ABS_Y, 0, 1920, 0, 0);

    err = input_register_device(virt_touch);
    if (err) {
        input_free_device(virt_touch);
        return err;
    }

    pr_info("Virtual Touch Device Registered\n");
    return 0;
}

static void __exit virt_touch_exit(void)
{
    input_unregister_device(virt_touch);
    pr_info("Virtual Touch Device Unregistered\n");
}

module_init(virt_touch_init);
module_exit(virt_touch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Hybrid Virtual Touch Input for Headless Android");
