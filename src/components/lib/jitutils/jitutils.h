#ifndef JITUTILS_H
#define JITUTILS_H

#include <cos_types.h>
#include <cos_component.h>

int jitutils_replace(u8_t *src, u8_t *orig, u8_t *replace, size_t len, size_t max);

#endif