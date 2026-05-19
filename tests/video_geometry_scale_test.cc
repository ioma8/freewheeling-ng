#include <pthread.h>

#include "fweelin_config.h"

#include <assert.h>

namespace {
class TestGeometry : public FloLayoutElementGeometry {
 public:
  int last_left = 0;
  int last_top = 0;

  void Draw(SDL_Surface *, SDL_Color,
            const FweelinRenderMetrics &metrics) override {
    last_left = metrics.ScaleX(5);
    last_top = metrics.ScaleY(6);
  }

  char Inside(int, int) override { return 0; }
};

class TestDisplay : public FloDisplay {
 public:
  TestDisplay() : FloDisplay(0), last_x(0), last_y(0), reset_calls(0) {}

  void Draw(SDL_Surface *, const FweelinRenderMetrics &metrics) override {
    last_x = metrics.ScaleX(xpos);
    last_y = metrics.ScaleY(ypos);
  }

  void ResetRenderCache() override { reset_calls++; }

  int last_x;
  int last_y;
  int reset_calls;
};
}  // namespace

int main() {
  FweelinRenderMetrics metrics =
      FweelinRenderMetrics::FromDrawableSize(640, 480, 1280, 960);

  FloLayoutElement elem;
  elem.nxpos = 5;
  elem.nypos = 6;
  elem.loopx = 9;
  elem.loopy = 10;
  elem.loopsize = 11;
  elem.geo = new TestGeometry();

  elem.geo->Draw(nullptr, SDL_Color{0, 0, 0, 0}, metrics);
  assert(static_cast<TestGeometry *>(elem.geo)->last_left == 10);
  assert(static_cast<TestGeometry *>(elem.geo)->last_top == 12);
  assert(elem.nxpos == 5);
  assert(elem.nypos == 6);
  assert(elem.loopx == 9);
  assert(elem.loopy == 10);
  assert(elem.loopsize == 11);

  TestDisplay display;
  display.xpos = 10;
  display.ypos = 12;
  display.Draw(nullptr, metrics);
  assert(display.last_x == 20);
  assert(display.last_y == 24);
  assert(display.xpos == 10);
  assert(display.ypos == 12);
  assert(display.reset_calls == 0);
  display.ResetRenderCache();
  assert(display.reset_calls == 1);

  return 0;
}
