#ifndef SCOPE_H
#define SCOPE_H

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "hazel/hazel.h"

#ifdef __cplusplus
extern "C" {
#endif

void print_scope(hazel_ctx_t* ctx, double *data, int len, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
