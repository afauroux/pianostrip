#include "Config.h"

static uint8_t gRainbowOffset = 0;

void updateRainbowDemo(char* detailLine, size_t detailSize) {
  showRainbowDemo(gRainbowOffset++);
  showStrip();
  snprintf(detailLine, detailSize, "Rainbow sweep");
}
