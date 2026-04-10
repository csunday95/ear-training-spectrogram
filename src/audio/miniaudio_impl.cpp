// Single translation unit that compiles the miniaudio implementation.
// All other files that need miniaudio types just #include "miniaudio.h" without this define.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
