#include "media/camera_settings.h"

#include <string.h>
#include <strings.h>
#include "esp_camera.h"

bool media_framesize_from_name(const char *name, int *out_framesize)
{
    if (!name || !name[0] || !out_framesize) return false;

    if (strcasecmp(name, "QQVGA") == 0) { *out_framesize = FRAMESIZE_QQVGA; return true; }
    if (strcasecmp(name, "QVGA") == 0)  { *out_framesize = FRAMESIZE_QVGA;  return true; }
    if (strcasecmp(name, "VGA") == 0)   { *out_framesize = FRAMESIZE_VGA;   return true; }
    if (strcasecmp(name, "SVGA") == 0)  { *out_framesize = FRAMESIZE_SVGA;  return true; }
    if (strcasecmp(name, "XGA") == 0)   { *out_framesize = FRAMESIZE_XGA;   return true; }
    if (strcasecmp(name, "SXGA") == 0)  { *out_framesize = FRAMESIZE_SXGA;  return true; }
    if (strcasecmp(name, "UXGA") == 0)  { *out_framesize = FRAMESIZE_UXGA;  return true; }
    if (strcasecmp(name, "HD") == 0)    { *out_framesize = FRAMESIZE_HD;    return true; }
    if (strcasecmp(name, "FHD") == 0)   { *out_framesize = FRAMESIZE_FHD;   return true; }

    return false;
}

const char *media_framesize_name(int framesize)
{
    switch (framesize) {
    case FRAMESIZE_QQVGA: return "QQVGA";
    case FRAMESIZE_QVGA:  return "QVGA";
    case FRAMESIZE_VGA:   return "VGA";
    case FRAMESIZE_SVGA:  return "SVGA";
    case FRAMESIZE_XGA:   return "XGA";
    case FRAMESIZE_SXGA:  return "SXGA";
    case FRAMESIZE_UXGA:  return "UXGA";
    case FRAMESIZE_HD:    return "HD";
    case FRAMESIZE_FHD:   return "FHD";
    default:              return "UNKNOWN";
    }
}
