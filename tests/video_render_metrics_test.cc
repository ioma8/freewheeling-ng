#include "fweelin_video_scaling.h"

#include <assert.h>

int main() {
  FweelinRenderMetrics metrics =
      FweelinRenderMetrics::FromDrawableSize(640, 480, 1280, 960);
  assert(metrics.logical_width == 640);
  assert(metrics.logical_height == 480);
  assert(metrics.drawable_width == 1280);
  assert(metrics.drawable_height == 960);
  assert(metrics.ScaleX(10) == 20);
  assert(metrics.ScaleY(10) == 20);
  assert(metrics.ScaleX(223) == 446);
  assert(metrics.ScaleY(42) == 84);
  assert(metrics.ScaleFont(12) == 24);

  FweelinRenderMetrics uneven =
      FweelinRenderMetrics::FromDrawableSize(800, 600, 1200, 900);
  assert(uneven.ScaleX(10) == 15);
  assert(uneven.ScaleY(12) == 18);

  FweelinRenderMetrics identity =
      FweelinRenderMetrics::FromDrawableSize(640, 480, 0, 0);
  assert(identity.ScaleX(25) == 25);
  assert(identity.ScaleY(30) == 30);
  assert(identity.ScaleFont(14) == 14);

  return 0;
}
