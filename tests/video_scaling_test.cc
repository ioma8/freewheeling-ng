#include "fweelin_video_scaling.h"

#include <assert.h>

int main() {
  FweelinVideoScale scale = FweelinComputeVideoScale(640, 480, 1280, 960);
  assert(scale.logical_width == 640);
  assert(scale.logical_height == 480);
  assert(scale.drawable_width == 1280);
  assert(scale.drawable_height == 960);
  assert(scale.scale_x == 2.0f);
  assert(scale.scale_y == 2.0f);
  assert(FweelinScaleExtent(223, scale.scale_x) == 446);
  assert(FweelinScaleExtent(42, scale.scale_y) == 84);
  assert(FweelinScaleFontPointSize(12, scale.scale_y) == 24);

  FweelinVideoScale fallback = FweelinComputeVideoScale(640, 480, 0, 0);
  assert(fallback.drawable_width == 640);
  assert(fallback.drawable_height == 480);
  assert(fallback.scale_x == 1.0f);
  assert(fallback.scale_y == 1.0f);
  assert(FweelinScaleExtent(223, fallback.scale_x) == 223);
  assert(FweelinScaleFontPointSize(12, fallback.scale_y) == 12);

  FweelinVideoScale uneven = FweelinComputeVideoScale(800, 600, 1200, 900);
  assert(uneven.scale_x == 1.5f);
  assert(uneven.scale_y == 1.5f);
  assert(FweelinScaleExtent(5, uneven.scale_x) == 8);

  return 0;
}
