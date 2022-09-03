# fb-ws-eink

Waveshare/Adafruit E-ink Linux framebuffer out-of-tree module(s).

The familiy of drivers _*currently*_ supports the following of Waveshare's
e-ink panels:

2.13"

Currently, full support is only for the _monochrome_ V3 model of Waveshare's
panel.  This is the fastest full updating device in the size of panel.
Update times are within 2-ish seconds for a full update of the panel.  Fast
or partial update as they describe it in the documentation is in progress
but is not complete/functional.

The main purpose of this driver is to provide a usable and consistent/common
abstraction for using these displays under Linux.  There currently are three
differing versions of the codebase in C or Python for the three outstanding
models of the product offerings from Dalian Good-Display (The ultimate
manufacturer of these displays from Waveshare, Adafruit, etc.) with code that
wasn't written to compensate for little more than differing timings on inits,
resets, waits, etc.  It's a bit of a mess and this project seeks to sort some
or all of this out, largely transparently to your code, with a device tree
overlay to select out your desired panel to work with and basically do a fire
and forget type operation on a stock Linux framebuffer device.  We handle
the rest for you.


The following is an example of the Device Tree entry for the _current_ driver
with the supported panel on a Raspberry PI type device:


```
// Waveshare Eink as frame buffer
#include <dt-bindings/gpio/gpio.h>

/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2835";

    fragment@0 {
        target = <&spidev0>;
        __overlay__ {
            status = "disabled";
        };
    };

    fragment@1 {
        target = <&spidev1>;
        __overlay__ {
            status = "disabled";
        };
    };

    fragment@2 {
        target = <&gpio>;
        __overlay__ {
            wseinkfb_pins: wseinkfb_pins {
                brcm,pins = <17 24 25>;
                brcm,function = <1>; /* out */
            };
        };
    };

    fragment@2 {
        target = <&spi0>;
        __overlay__ {
            /* needed to avoid dtc warning */
            #address-cells = <1>;
            #size-cells = <0>;

            status = "okay";

            wseinkfb@0 {
                compatible = "waveshare,213";
                reg = <0>;                      /* CE0 */
                pinctrl-names = "default";
                pinctrl-0 = <&wseinkfb_pins>;
                spi-max-frequency = <12500000>;
                ws,rst-gpios = <&gpio 17 GPIO_ACTIVE_LOW>;
                ws,dc-gpios = <&gpio 25 GPIO_ACTIVE_HIGH>;
                ws,busy-gpios = <&gpio 24 GPIO_ACTIVE_LOW>;
                status = "okay";
            };
        };
    };
};
```

So this should give you an idea about bindings.  Compatible lines equate to "waveshare,<foo>"
where <foo> is the inches and decimal value of the display size, with the decimal removed.
The rest is pretty self-explanatory to someone skilled in the art.

## BUILDING

This too, is pretty straightforward.  Like any other out-of-tree kernel build.

In order for it to work right, though, you will need the following added to
your kernel config, either as a defconfig for your target or as a Yocto
kernel config fragment:

```
CONFIG_FB=y
CONFIG_SPI=y
CONFIG_FB_SYS_FOPS=y
CONFIG_FB_CFB_FILLRECT=y
CONFIG_FB_CFB_COPYAREA=y
CONFIG_FB_CFB_IMAGEBLIT=y
CONFIG_FB_CFB_REV_PIXELS_IN_BYTE=y
CONFIG_FB_DEFERRED_IO=y
```

## Current Gotchas

### No support for anything but the latest panels.

+ Prior versions/models of the panels aren't going to be realistically
  supported.  The chips themselves will provide support for a wider
  range than I can support.  If you can hand me a panel to work with
  that's an older version (For example, the V2 2.13 inch panel needs
  a custom LUT setup, whereas the OTP programmed V3 appears to not
  need one based on a review of the "library" Waveshare provides...)
  so I can get LUTs, etc. worked in, we're good to go and I can support.

## Planned Roadmap

+ Adding the other displays in Waveshare's lineup.
  This will entail more driver modules unless it
  matches up with a current driver IC.  There's
  quite a few differences between the panels, so
  there's
+ Multicolor.  Most of the lineup for the chipsets
  from Solomon that're being used here are capable
  of tricolor and more.  The drawback being that
  the color electrophoretic changes are...much more
  time consuming for an update in many cases.