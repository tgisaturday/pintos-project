#ifndef FIXED_POINT_REAL_ARITHMETIC
#define FIXED_POINT_REAL_ARITHMETIC

#define FRAC_BASE 14
#define F (1 << FRAC_BASE)

#define int_to_fix(n) ((n) * F)
#define fix_to_int_zero(x) ((x) / F)
#define fix_to_int_nearest(x) (((x) >= 0 ? ((x) + (F/2)) : ((x) - (F/2))) / F)

#define fix_add(x,y) ((x) + (y))

#define fix_sub(x,y) ((x) - (y))

#define fix_mul(x,y) ((((int64_t)(x)) * (y)) / F)

#define fix_div(x,y) ((((int64_t)(x)) * F) / (y))

#endif
