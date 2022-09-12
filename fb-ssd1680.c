#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

// We do 10 partial updates to every full if we're in partial updates mode.
// (FIXME : This is not working right now.  We're working on this one...)
const int num_partials = 20;

enum ssd1680_devices {
    DEV_WS_213,
    DEV_WS_27,
    DEV_WS_29,
};

struct ssd1680_device_properties {
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
};


// FIXME - There's 6 values at the end they kind of hard-coded into this "LUT" for the example that
//         really needs to be in it's own table or in the final settings for init.
//         (This is duplicated in both LUTs...seems inefficient...  We want it pluggable if these
//          values change from panel to panel (Possible...), so we want to make this more flexible.)
unsigned char DEV_WS_213_FULL_LUT[] =
{
	0x80,	0x4A,	0x40,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x40,	0x4A,	0x80,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x80,	0x4A,	0x40,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x40,	0x4A,	0x80,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0xF,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0xF,	0x0,	0x0,	0xF,	0x0,	0x0,	0x2,
	0xF,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x1,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
	0x22,	0x22,	0x22,	0x22,	0x22,	0x22,	0x0,	0x0,	0x0,
	0x22,	0x17,	0x41,	0x0,	0x32,	0x36
};
const int DEV_WS_213_FULL_LUT_SIZE = sizeof(DEV_WS_213_FULL_LUT);

unsigned char DEV_WS_213_PARTIAL_LUT[] =
{
	0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x14,0x0,0x0,0x0,0x0,0x0,0x0,
	0x1,0x0,0x0,0x0,0x0,0x0,0x0,
	0x1,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,
	0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
	0x22,0x17,0x41,0x00,0x32,0x36,
};
const int DEV_WS_213_PARTIAL_LUT_SIZE = sizeof(DEV_WS_213_PARTIAL_LUT);


static struct ssd1680_device_properties devices[] =
{
    [DEV_WS_213] = {.width = 128, .height = 250, .bpp = 1},
    [DEV_WS_27]  = {.width = 176, .height = 264, .bpp = 1},
    [DEV_WS_29]  = {.width = 128, .height = 296, .bpp = 1},
};

struct ssd1680_state {
    struct spi_device *spi;
    struct fb_info *info;
    const struct ssd1680_device_properties *props;
    int rst;
    int dc;
    int busy;
    bool do_partial;
    int partials;
};

static void device_write_data(struct ssd1680_state *par, u8 data)
{
    int ret = 0;

    /* Tell the device we're writing data instead of commands */
    gpio_set_value(par->dc, 1);
    ret = spi_write(par->spi, &data, sizeof(data));
    if (ret < 0) {
        pr_err("%s: write data %02x failed with status %d\n", par->info->fix.id, data, ret);
    }
}

static int device_write_data_buf(struct ssd1680_state *par, const u8 *txbuf, size_t size)
{
    int ret = 0;

    /* Tell the device we're writing data instead of commands */
    gpio_set_value(par->dc, 1);
    if (ret < 0) {
        pr_err("%s: write data buf %p failed with status %d\n", par->info->fix.id, txbuf, ret);
    }
    return spi_write(par->spi, txbuf, size);
}

static void device_write_cmd(struct ssd1680_state *par, u8 cmd)
{
    int ret = 0;

    /* Tell the device we're issuing commands to it */
    gpio_set_value(par->dc, 0);
    ret = spi_write(par->spi, &cmd, sizeof(cmd));
    if (ret < 0) {
        pr_err("%s: write command %02x failed with status %d\n", par->info->fix.id, cmd, ret);
    }
}

static void device_wait_until_idle(struct ssd1680_state *par)
{
    int wait_count = 0;
    while ((gpio_get_value(par->busy) == 0) && (wait_count < 200))
    {
        /*
            Wait 10 msec for up to 2 seconds (All ops total on this family of devices should take 2-ish)
            This way we don't pin a CPU and all...
        */
        mdelay(10);
        wait_count++;
    }

    /*
        According to the code in their library, we wait an additional 10msec
        from the line going low here...  Okay...
    */
    mdelay(10);
}

static void device_reset(struct ssd1680_state *par)
{
    gpio_set_value(par->rst, 1);
    mdelay(20);
    gpio_set_value(par->rst, 0);
    mdelay(2);
    gpio_set_value(par->rst, 1);
    mdelay(20);
}

static void set_rendering_window(struct ssd1680_state *par,
    unsigned int Xstart, unsigned int Ystart, unsigned int Xend, unsigned int Yend)
{
    device_write_cmd(par, 0x44);                        // SET_RAM_X_ADDRESS_START_END_POSITION
    device_write_data(par, (Xstart>>3) & 0xFF);
    device_write_data(par, (Xend>>3) & 0xFF);

    device_write_cmd(par, 0x45);                        // SET_RAM_Y_ADDRESS_START_END_POSITION
    device_write_data(par, Ystart & 0xFF);
    device_write_data(par, (Ystart >> 8) & 0xFF);
    device_write_data(par, Yend & 0xFF);
    device_write_data(par, (Yend >> 8) & 0xFF);
}

static void set_cursor(struct ssd1680_state *par, unsigned int Xstart, unsigned int Ystart)
{
    device_write_cmd(par, 0x4E);             // SET_RAM_X_ADDRESS_COUNTER
    device_write_data(par, Xstart & 0xFF);

    device_write_cmd(par, 0x4F);             // SET_RAM_Y_ADDRESS_COUNTER
    device_write_data(par, Ystart & 0xFF);
    device_write_data(par, (Ystart >> 8) & 0xFF);
}


static void init_display(struct ssd1680_state *par)
{
    unsigned char *cur_lut;
    unsigned int cur_lut_size;

    /* Pulled from the vendor's 2.13" display library code */
	device_reset(par);

	device_write_cmd(par, 0x12);    //SWRESET
	device_wait_until_idle(par);

	device_write_cmd(par, 0x01);    //Driver output control
	device_write_data(par, 0xf9);
	device_write_data(par, 0x00);
	device_write_data(par, 0x00);

	device_write_cmd(par, 0x11);     //data entry mode
	device_write_data(par, 0x03);

	set_rendering_window(par, 0, 0, par->info->var.xres-1, par->info->var.yres-1);
	set_cursor(par, 0, 0);

	device_write_cmd(par, 0x3C);    //Border Wavefrom chosen. We're going to do white borders.
	device_write_data(par, 0x05);

	device_write_cmd(par, 0x21);    // Display update control
	device_write_data(par, 0x00);
	device_write_data(par, 0x80);

	device_write_cmd(par, 0x18);    // Read built-in temperature sensor for temp compensation on LUT.
	device_write_data(par, 0x80);

    /*
        Determine LUT we're using.  If we're in a partial mode, we need to be doing that unless
        it's a new full update line in the sand...
    */
    if (par->do_partial && (par->partials < num_partials)) {
        // Partial update commanded...so...set that up...  We'll bump counter at the end of an update
        // if we're in the mode...
        cur_lut = DEV_WS_213_PARTIAL_LUT;
        cur_lut_size = DEV_WS_213_PARTIAL_LUT_SIZE;

        // Set for ping-pong RAM configuration for partials.
        device_write_cmd(par, 0x37);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x40);  ///RAM Ping-Pong enable
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
        device_write_data(par, 0x00);
    } else {
        // FULL update commanded...so...set that up...
        cur_lut = DEV_WS_213_FULL_LUT;
        cur_lut_size = DEV_WS_213_FULL_LUT_SIZE;
        par->partials = 0;
    }

    device_write_cmd(par, 0x32);    // Push the LUT for normal operation...  FIXME- This really needs to be bolted on per device.
    device_write_data_buf(par, cur_lut, cur_lut_size-6);  // Vendor's library has the settings for everything else in the array, so it's 6 less.
    device_wait_until_idle(par);

    // Now do the last bits we think we need- it's not fully set up without this stuff...
    device_write_cmd(par, 0x3F);
    device_write_data(par, cur_lut[cur_lut_size-6]);
    device_write_cmd(par, 0x03);    // Gate voltage
    device_write_data(par, cur_lut[cur_lut_size-5]);
    device_write_cmd(par, 0x04);    // Source voltage
    device_write_data(par, cur_lut[cur_lut_size-4]);
    device_write_data(par, cur_lut[cur_lut_size-3]);
    device_write_data(par, cur_lut[cur_lut_size-2]);
    device_write_cmd(par, 0x2C);     // VCOM
    device_write_data(par, cur_lut[cur_lut_size-1]);
}

static void write_display (struct ssd1680_state *par) {
    /* Push our FB data out to the panel... */
    device_write_cmd(par, 0x24);
    device_write_data_buf(par, par->info->screen_base, par->info->fix.smem_len);

    /* Call the display output calls */
	device_write_cmd(par, 0x22);     // Display Update Control
	device_write_data(par, (par->do_partial && (par->partials < num_partials)) ? 0x0f : 0xcf);    // fast:0x0c, quality:0x0f, full:0xcf -- we'll do Full/Quality here...
	device_write_cmd(par, 0x20);     // Activate Display Update Sequence
	device_wait_until_idle(par);

    /* Completion...we increment count to run the number of partial updates up...if it's set...*/
    if (par->do_partial) {
        par->partials++;
    }
}

static void sleep_display(struct ssd1680_state *par) {
	device_write_cmd(par, 0x10);        //enter deep sleep
	device_write_data(par, 0x01);
	mdelay(100);
}


static void display_frame(struct ssd1680_state *par) {
    /*
        The *proper* sequence for the display (And this is for all Solomon based parts) is as follows:

        - Init display.  In some chips you also have to power it on before init.
          (So in other devices, we'll nestle the power on as part of the init.)
        - Push the update or partial update.  Docs say you have to wipe, but
          in at least the case of this class of part, we just need to push the
          FB as defined and it'll take care of itself.  No clear needed.
        - Power down/deep sleep.  Powered/not deep sleeping leaves the panel's
          circuitry in the buck-boosted high voltage states for doing EPD updates
          which will eventually damage the panel.  This would be bad...

        So...we're going to follow that to the letter.  This takes about 2-ish
        seconds or less for full updates on the V3 panel, so we're going to set
        deferred I/O to be at that atom so it works cleanly.
    */
    init_display(par);
    write_display(par);
    sleep_display(par);
}

int init_gpio_from_of(struct device *dev, const char *gpio_name, int dir, int *init_gpio)
{
    struct device_node *np = dev->of_node;
    int gpio;
    int ret;

    gpio = of_get_named_gpio(np, gpio_name, 0);
    if (!gpio_is_valid(gpio)) {
        dev_err(dev, "No valid gpio found for %s\n", gpio_name);
        return -ENODEV;
    }

    ret = devm_gpio_request(dev, gpio, "sysfs");
    if (ret) {
        dev_err(dev, "Failed to request gpio %s\n", gpio_name);
        return ret;
    }

    if (dir)
        gpio_direction_input(gpio);
    else
        gpio_direction_output(gpio, 0);

    gpio_export(gpio, true);

    *init_gpio = gpio;
    return 0;
}

static ssize_t ssd_1680_write(struct fb_info *info, const char __user *buf, size_t count,	loff_t *ppos)
{
    ssize_t res = fb_sys_write(info, buf, count, ppos);
    schedule_delayed_work(&info->deferred_work, info->fbdefio->delay);
    return res;
}

static void ssd_1680_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    cfb_fillrect(info, rect);
    schedule_delayed_work(&info->deferred_work, info->fbdefio->delay);
}

static void ssd_1680_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
    cfb_copyarea(info, area);
    schedule_delayed_work(&info->deferred_work, info->fbdefio->delay);
}

static void ssd_1680_imageblit(struct fb_info *info, const struct fb_image *image)
{
    cfb_imageblit(info, image);
    schedule_delayed_work(&info->deferred_work, info->fbdefio->delay);
}

static struct fb_ops ssd1680_ops = {
    .owner	= THIS_MODULE,
    .fb_read	= fb_sys_read,
    .fb_write	= ssd_1680_write,
    .fb_fillrect	= ssd_1680_fillrect,
    .fb_copyarea	= ssd_1680_copyarea,
    .fb_imageblit	= ssd_1680_imageblit,
};

static const struct of_device_id ssd1680_of_match[] = {
    { .compatible = "waveshare,213", .data = (void *)DEV_WS_213 },
    { .compatible = "waveshare,27", .data = (void *)DEV_WS_27 },
    { .compatible = "waveshare,29", .data = (void *)DEV_WS_29 },
    {},
};
MODULE_DEVICE_TABLE(of, ssd1680_of_match);

static struct spi_device_id ssd1680_tbl[] = {
    { "waveshare_213", DEV_WS_213 },
    { "waveshare_27",  DEV_WS_27 },
    { "waveshare_29",  DEV_WS_29 },
    { },
};
MODULE_DEVICE_TABLE(spi, ssd1680_tbl);

static void ssd1680_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
    display_frame(info->par);
}

/*
    FIXME -- Can we start with a given size and then
             if we have a DT entry, adjust this default
             value on init...?
*/
static struct fb_deferred_io ssd1680_defio = {
  .delay	= HZ*2,                         // V3's can update full screen in 2 secs.  So...
  .deferred_io	= ssd1680_deferred_io,
};

static int ssd1680_spi_probe(struct spi_device *spi)
{
    struct fb_info *info;
    const struct spi_device_id *spi_id;
    const struct ssd1680_device_properties *props;
    struct ssd1680_state *par;
    u8 *vmem;
    int vmem_size;
    struct device *dev = &spi->dev;
    const struct of_device_id *match;
    unsigned int of_prop;
    int rst_gpio, dc_gpio, busy_gpio;
    int i;
    int ret = 0;

    match = of_match_device(ssd1680_of_match, dev);
    if (match) {
        props = &devices[(kernel_ulong_t)match->data];
    } else {
        spi_id = spi_get_device_id(spi);
        if (!spi_id) {
            dev_err(dev, "device id not supported!\n");
            return -EINVAL;
        }
        props = &devices[spi_id->driver_data];
    }

    ret = init_gpio_from_of(dev, "solomon,rst-gpios", 0, &rst_gpio);
    if (!ret) {
        ret = init_gpio_from_of(dev, "solomon,dc-gpios", 0, &dc_gpio);
        if (!ret) {
            ret = init_gpio_from_of(dev, "solomon,busy-gpios", 0, &busy_gpio);
            if (ret) {
                dev_err(dev, "Couldn't set busy GPIO\n");
                return ret;
            }
        }
        else {
            dev_err(dev, "Couldn't set data/command GPIO\n");
            return ret;
        }
    }
    else {
        dev_err(dev, "Couldn't set reset GPIO\n");
        return ret;
    }

    /* plus one to include size % 8 */
    vmem_size = props->width * props->height * props->bpp / 8;
    vmem = vzalloc(vmem_size);

    if (!vmem) {
        dev_err(dev, "Didn't allocate framebuffer memory!\n");
        return -ENOMEM;
    }

    info = framebuffer_alloc(sizeof(struct ssd1680_state), dev);
    if (!info) {
        dev_err(dev, "Didn't allocate framebuffer info!\n");
        ret = -ENOMEM;
        goto fballoc_fail;
    }

    info->screen_base = (u8 __force __iomem *)vmem;
    info->fbops = &ssd1680_ops;

    /* why WARN_ON here? */
    WARN_ON(strlcpy(info->fix.id, "ssd1680", sizeof(info->fix.id)) >= sizeof(info->fix.id));
    info->fix.type		    = FB_TYPE_PACKED_PIXELS;
    info->fix.visual	    = FB_VISUAL_MONO10;                 // FIXME - We're going for MONOCHROME right now.
    info->fix.smem_len	    = vmem_size;
    info->fix.xpanstep	    = 0;
    info->fix.ypanstep	    = 0;
    info->fix.ywrapstep	    = 0;
    info->fix.line_length	= props->width * props->bpp / 8; // +1?

    info->var.xres		        = props->width;
    info->var.yres		        = props->height;
    info->var.xres_virtual	    = props->width;
    info->var.yres_virtual	    = props->height;
    info->var.bits_per_pixel	= props->bpp;

    info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

    info->fbdefio = &ssd1680_defio;
    fb_deferred_io_init(info);

    par = info->par;
    par->info		    = info;
    par->spi		    = spi;
    par->props	        = props;
    par->rst		    = rst_gpio;
    par->dc		        = dc_gpio;
    par->busy		    = busy_gpio;

    /*
        Go fetch the solomon,partial-updates parameter from the device tree...
        If it's there, use it.  If not, default to full updates.
    */
    if (0 == device_property_read_u32(dev, "solomon,partial-update", &of_prop)) {
        /* Got it...use the property... */
        par->do_partial = (bool)(of_prop);
        /*
            Check that result.  If it's set greater than zero, we're doing partials.
            That means we need to do a bit of initial staging work for this to work
            (Initialization takes a bit longer, because we've got to set ping-pongs
            with at least all blank slates...)
        */
        if (par->do_partial) {
            par->partials = num_partials;
            device_write_cmd(par, 0x26);
            for (i = 0; i < vmem_size; i++) {
                device_write_data(par, 0xFF);
            }
        }
    }

    ret = register_framebuffer(info);
    if (ret < 0) {
        dev_err(dev, "framebuffer registration failed\n");
        goto fbreg_fail;
    }

    spi_set_drvdata(spi, par);

    dev_info(dev, "fb%d: %s frame buffer, %d KiB VRAM, planes %d, partial mode- %d\n",
        info->node, info->fix.id, vmem_size, info->var.bits_per_pixel, of_prop);

    return 0;

    fbreg_fail:
        fb_deferred_io_cleanup(info);
        framebuffer_release(info);

    fballoc_fail:
        vfree(vmem);

    return ret;
}

static int ssd1680_spi_remove(struct spi_device *spi)
{
    struct ssd1680_state *par = spi_get_drvdata(spi);
    struct fb_info *info = par->info;
    unregister_framebuffer(info);
    fb_deferred_io_cleanup(info);
    //fb_dealloc_cmap(&info->cmap);
    iounmap(info->screen_base);
    framebuffer_release(info);

    return 0;
}

static struct spi_driver ssd1680_driver = {
    .driver = {
    .name	= "fb-ssd1680",
    .owner	= THIS_MODULE,
    .of_match_table = ssd1680_of_match,
    },

    .id_table	= ssd1680_tbl,
    .probe	    = ssd1680_spi_probe,
    .remove 	= ssd1680_spi_remove,
};
module_spi_driver(ssd1680_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frank C. Earl");
MODULE_DESCRIPTION("FB driver for SSD1680 based eink displays");
MODULE_VERSION("0.1");
