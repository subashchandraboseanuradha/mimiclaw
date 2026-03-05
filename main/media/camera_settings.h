#pragma once

#include <stdbool.h>

/* Map human-friendly framesize names to esp-camera framesize constants. */
bool media_framesize_from_name(const char *name, int *out_framesize);

/* Convert esp-camera framesize constant to human-friendly name. */
const char *media_framesize_name(int framesize);
