#ifndef __LIB_KERNEL_FIXEDPOINT_H
#define __LIB_KERNEL_FIXEDPOINT_H

#include <stdint.h>

/* 2^14 conversion factor */
#define FP_F 16384 

typedef int32_t fixed_point_t;

fixed_point_t fp_int_to_fp(int32_t);
int32_t fp_to_int_trunc(fixed_point_t);
int32_t fp_to_int_round(fixed_point_t);

fixed_point_t fp_add(fixed_point_t, fixed_point_t);
fixed_point_t fp_sub(fixed_point_t, fixed_point_t);
fixed_point_t fp_mult(fixed_point_t, fixed_point_t);
fixed_point_t fp_div(fixed_point_t, fixed_point_t);

fixed_point_t fp_add_int(fixed_point_t, int32_t);
fixed_point_t fp_sub_int(fixed_point_t, int32_t);
fixed_point_t fp_mult_int(fixed_point_t, int32_t);
fixed_point_t fp_div_int(fixed_point_t, int32_t);

#endif /* lib/kernel/fixed-point.h */
