#include <fixed-point.h>

/*
Convert n to fixed point:	n * f
Convert x to integer (rounding toward zero):	x / f
Convert x to integer (rounding to nearest):	(x + f / 2) / f if x >= 0, 
(x - f / 2) / f if x <= 0.
Add x and y:	x + y
Subtract y from x:	x - y
Add x and n:	x + n * f
Subtract n from x:	x - n * f
Multiply x by y:	((int64_t) x) * y / f
Multiply x by n:	x * n
Divide x by y:	((int64_t) x) * f / y
Divide x by n:	x / n
*/

fixed_point_t fp_int_to_fp(int32_t n)
{
  return n * FP_F;
}

int32_t fp_to_int_trunc(fixed_point_t x)
{
  return x / FP_F;
}

int32_t fp_to_int_round(fixed_point_t x)
{
  int32_t result;
  
  if ((int32_t) x >= 0)
    result = (x + FP_F / 2) / FP_F;
  else
    result = (x - FP_F / 2) / FP_F;
  
  return result;
}

fixed_point_t fp_add(fixed_point_t x, fixed_point_t y)
{
  return x + y;
}

fixed_point_t fp_sub(fixed_point_t x, fixed_point_t y)
{
  return x - y;
}

fixed_point_t fp_add_int(fixed_point_t x, int32_t n)
{
  return x + n * FP_F;
}

fixed_point_t fp_sub_int(fixed_point_t x, int32_t n)
{
  return x - n * FP_F;
}

fixed_point_t fp_mult(fixed_point_t x, fixed_point_t y)
{
  return ((int64_t) x) * y / FP_F;
}

fixed_point_t fp_mult_int(fixed_point_t x, int32_t n)
{
  return x * n;
}

fixed_point_t fp_div(fixed_point_t x, fixed_point_t y)
{
  return ((int64_t) x) * FP_F / y;
}

fixed_point_t fp_div_int(fixed_point_t x, int32_t n)
{
  return x / n;
}
