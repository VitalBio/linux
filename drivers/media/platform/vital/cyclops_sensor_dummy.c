#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>


struct pixel_format {
  u32 mbus_code;
  enum v4l2_colorspace colorspace;
};

static const struct pixel_format pixel_formats[] = {
  { MEDIA_BUS_FMT_Y8_1X8, V4L2_COLORSPACE_RAW },
  { MEDIA_BUS_FMT_Y10_1X10, V4L2_COLORSPACE_RAW },
};

struct cyclops_sensor_dummy {
  struct v4l2_subdev subdev;
  struct platform_device* platform_device;
  struct clk *sensor_clk;
  u32 selected_mbus_code;
};

static struct cyclops_sensor_dummy* sensor_from_subdev(const struct v4l2_subdev* sd)
{
  return container_of(sd, struct cyclops_sensor_dummy, subdev);
}

static int impl_enum_mbus_code(
    struct v4l2_subdev* sd,
    struct v4l2_subdev_pad_config* cfg,
    struct v4l2_subdev_mbus_code_enum* code
) {
  if (code->pad || code->index >= ARRAY_SIZE(pixel_formats))
    return -EINVAL;
  code->code = pixel_formats[code->index].mbus_code;
  return 0;
}

static int impl_enum_frame_size(
    struct v4l2_subdev* sd,
    struct v4l2_subdev_pad_config* cfg,
    struct v4l2_subdev_frame_size_enum* fse
) {
  if (fse->index)
    return -EINVAL;

  fse->max_width = fse->min_width = 4208;
  fse->max_height = fse->min_height = 3118;
  return 0;
}

static int impl_get_fmt(
    struct v4l2_subdev* sd,
    struct v4l2_subdev_pad_config* cfg,
    struct v4l2_subdev_format* format
) {
  struct cyclops_sensor_dummy* sensor = sensor_from_subdev(sd);
  struct device* dev = &sensor->platform_device->dev;

  if (format->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
    dev_err(dev, "get_fmt with which != V4L2_SUBDEV_FORMAT_ACTIVE - dummy driver does not support negotiation since mx6s_capture doesn't require it");
    return -EINVAL;
  }

  format->format.width = 4208;
  format->format.height = 3118;
  format->format.code = sensor->selected_mbus_code;
  format->format.field = V4L2_FIELD_NONE;

  return 0;
}

static int impl_set_fmt(
    struct v4l2_subdev* sd,
    struct v4l2_subdev_pad_config* cfg,
    struct v4l2_subdev_format* format
) {
  struct cyclops_sensor_dummy* sensor = sensor_from_subdev(sd);
  struct device* dev = &sensor->platform_device->dev;
  bool found;
  int i;

  if (format->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
    dev_err(dev, "set_fmt with which != V4L2_SUBDEV_FORMAT_ACTIVE - dummy driver does not support negotiation since mx6s_capture doesn't require it");
    return -EINVAL;
  }

  found = 0;
  for (i = 0; i < ARRAY_SIZE(pixel_formats); ++i) {
    if (format->format.code == pixel_formats[i].mbus_code) {
      found = 1;
      break;
    }
  }
  if (!found) {
    dev_err(dev, "set_fmt with unsupported mbus code %u", (unsigned int)format->format.code);
    return -EINVAL;
  }

  sensor->selected_mbus_code = format->format.code;

  format->format.width = 4208;
  format->format.height = 3118;
  format->format.field = V4L2_FIELD_NONE;

  return 0;
}

static const struct v4l2_subdev_pad_ops impl_pad_ops = {
  .enum_frame_size = impl_enum_frame_size,
  .enum_mbus_code = impl_enum_mbus_code,
  .set_fmt = impl_set_fmt,
  .get_fmt = impl_get_fmt,
};

static int impl_s_stream(
    struct v4l2_subdev* sd,
    int enable
) {
  return 0;
}

static const struct v4l2_subdev_video_ops impl_video_ops = {
  .s_stream = impl_s_stream,
};

static const struct v4l2_subdev_ops impl_ops = {
  .video = &impl_video_ops,
  .pad = &impl_pad_ops,
};

static int impl_probe(struct platform_device* pdev)
{
  struct device* dev = &pdev->dev;
  struct cyclops_sensor_dummy* sensor;
  struct v4l2_subdev* sd;
  int retval;

  dev_info(dev, "Vital Cyclops sensor dummy driver probing");

  sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
  if (!sensor)
    return -ENOMEM;

  sd = &sensor->subdev;
  sensor->platform_device = pdev;
  sensor->selected_mbus_code = MEDIA_BUS_FMT_Y8_1X8;

  sensor->sensor_clk = devm_clk_get(dev, "csi_mclk");
  if (IS_ERR(sensor->sensor_clk)) {
    dev_err(dev, "failed to get csi_mclk");
    return PTR_ERR(sensor->sensor_clk);
  }

  clk_prepare_enable(sensor->sensor_clk);

  v4l2_subdev_init(sd, &impl_ops);
  sd->flags |= 0;
  sd->owner = dev->driver->owner;
  sd->dev = dev;
  platform_set_drvdata(pdev, sd);
  snprintf(sd->name, sizeof(sd->name), "%s %d", dev->driver->name, pdev->id);
  v4l2_set_subdevdata(sd, pdev);

  retval = v4l2_async_register_subdev(sd);
  if (retval < 0)
    dev_err(dev, "Async register failed, ret=%d\n", retval);

  dev_info(dev, "probe complete");
  return 0;
}

static int impl_remove(struct platform_device* pdev)
{
  struct v4l2_subdev* sd = platform_get_drvdata(pdev);
  struct cyclops_sensor_dummy* sensor = sensor_from_subdev(sd);

  v4l2_async_unregister_subdev(sd);
  clk_disable_unprepare(sensor->sensor_clk);

  return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id impl_of_match[] = {
  { .compatible = "vital,cyclops_sensor_dummy", },
  {},
};
MODULE_DEVICE_TABLE(of, impl_of_match);
#endif

static struct platform_driver impl_driver = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "cyclops_sensor_dummy",
#ifdef CONFIG_OF
    .of_match_table = of_match_ptr(impl_of_match),
#endif
  },
  .probe = impl_probe,
  .remove = impl_remove,
};
module_platform_driver(impl_driver);

MODULE_AUTHOR("Vital Biosciences");
MODULE_DESCRIPTION("Vital dummy camera sensor driver for Cyclops");
MODULE_LICENSE("Proprietary");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:vital_cyclops_sensor_dummy");
