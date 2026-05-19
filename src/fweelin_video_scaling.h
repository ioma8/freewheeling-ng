#ifndef __FWEELIN_VIDEO_SCALING_H
#define __FWEELIN_VIDEO_SCALING_H

struct FweelinVideoScale {
  int logical_width;
  int logical_height;
  int drawable_width;
  int drawable_height;
  float scale_x;
  float scale_y;
};

inline FweelinVideoScale FweelinComputeVideoScale(int logical_width,
                                                  int logical_height,
                                                  int drawable_width,
                                                  int drawable_height) {
  FweelinVideoScale scale = {logical_width, logical_height, drawable_width,
                             drawable_height, 1.0f, 1.0f};

  if (scale.logical_width <= 0)
    scale.logical_width = (scale.drawable_width > 0 ? scale.drawable_width : 1);
  if (scale.logical_height <= 0)
    scale.logical_height =
        (scale.drawable_height > 0 ? scale.drawable_height : 1);
  if (scale.drawable_width <= 0)
    scale.drawable_width = scale.logical_width;
  if (scale.drawable_height <= 0)
    scale.drawable_height = scale.logical_height;

  scale.scale_x =
      static_cast<float>(scale.drawable_width) / scale.logical_width;
  scale.scale_y =
      static_cast<float>(scale.drawable_height) / scale.logical_height;
  return scale;
}

inline int FweelinScaleExtent(int value, float scale) {
  if (value <= 0)
    return 0;
  if (scale <= 0.0f)
    return value;

  int scaled = static_cast<int>(value * scale + 0.5f);
  return (scaled > 0 ? scaled : 1);
}

inline int FweelinScaleFontPointSize(int point_size, float scale) {
  return FweelinScaleExtent(point_size, scale);
}

struct FweelinRenderMetrics {
  int logical_width;
  int logical_height;
  int drawable_width;
  int drawable_height;
  float scale_x;
  float scale_y;

  static FweelinRenderMetrics FromDrawableSize(int logical_width,
                                               int logical_height,
                                               int drawable_width,
                                               int drawable_height) {
    FweelinVideoScale scale = FweelinComputeVideoScale(
        logical_width, logical_height, drawable_width, drawable_height);
    FweelinRenderMetrics metrics = {scale.logical_width, scale.logical_height,
                                    scale.drawable_width, scale.drawable_height,
                                    scale.scale_x, scale.scale_y};
    return metrics;
  }

  int ScaleX(int value) const { return FweelinScaleExtent(value, scale_x); }
  int ScaleY(int value) const { return FweelinScaleExtent(value, scale_y); }
  int ScaleFont(int point_size) const {
    return FweelinScaleFontPointSize(point_size, scale_y);
  }
};

#endif
