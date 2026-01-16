#pragma once

// OpenCV(host) build shim.
// Some integrated Arduboy game sources use `#include <Arduino.h>`.
// In the simulator we provide an Arduino compatibility layer via Arduino.hpp.

#include "Arduino.hpp"
