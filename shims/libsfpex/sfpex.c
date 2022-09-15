#define FP_EX_INVALID	0x01
#define FP_EX_DIVZERO	0x02
#define FP_EX_OVERFLOW	0x04
#define FP_EX_UNDERFLOW	0x08
#define FP_EX_INEXACT	0x10

void
__sfp_handle_exceptions (int _fex)
{
  const float fp_max = __FLT_MAX__;
  const float fp_min = __FLT_MIN__;
  const float fp_1e32 = 1.0e32f;
  const float fp_zero = 0.0;
  const float fp_one = 1.0;
  unsigned long fpsr;

  if (_fex & FP_EX_INVALID)
    {
      __asm__ __volatile__ ("fdiv\ts0, %s0, %s0"
			    :
			    : "w" (fp_zero)
			    : "s0");
      __asm__ __volatile__ ("mrs\t%0, fpsr" : "=r" (fpsr));
    }
  if (_fex & FP_EX_DIVZERO)
    {
      __asm__ __volatile__ ("fdiv\ts0, %s0, %s1"
			    :
			    : "w" (fp_one), "w" (fp_zero)
			    : "s0");
      __asm__ __volatile__ ("mrs\t%0, fpsr" : "=r" (fpsr));
    }
  if (_fex & FP_EX_OVERFLOW)
    {
      __asm__ __volatile__ ("fadd\ts0, %s0, %s1"
			    :
			    : "w" (fp_max), "w" (fp_1e32)
			    : "s0");
      __asm__ __volatile__ ("mrs\t%0, fpsr" : "=r" (fpsr));
    }
  if (_fex & FP_EX_UNDERFLOW)
    {
      __asm__ __volatile__ ("fmul\ts0, %s0, %s0"
			    :
			    : "w" (fp_min)
			    : "s0");
      __asm__ __volatile__ ("mrs\t%0, fpsr" : "=r" (fpsr));
    }
  if (_fex & FP_EX_INEXACT)
    {
      __asm__ __volatile__ ("fsub\ts0, %s0, %s1"
			    :
			    : "w" (fp_max), "w" (fp_one)
			    : "s0");
      __asm__ __volatile__ ("mrs\t%0, fpsr" : "=r" (fpsr));
    }
}
