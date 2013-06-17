#ifndef __DSPLIB_H__
#define __DSPLIB_H__

#include <vstypes.h>

/** Bi-quad IIR coefficients. */
struct DSP_IIR2COEFF16 {
    s_int16 b0,b1,b2,a1,a2;
};

/** Calculates bi-quad IIR for one sample. 26 cycles.
 */
__near auto s_int32 DspIir2Lq(register __a0 s_int16 val,
			      register __i0 __near const __y struct DSP_IIR2COEFF16 *baCoeff,
			      register __i2 __near s_int16 mem[5]);
/** Calculates bi-quad IIR for a block of samples. 27+10*n cycles.
 */
__near auto void DspIir2nLq(register __i1 __near s_int16 *buffer,
			    register __i0 __near const __y struct DSP_IIR2COEFF16 *baCoeff,
			    register __i2 __near s_int16 mem[5], register __a1 s_int16 n);


#endif /* !__DSPLIB_H__ */
