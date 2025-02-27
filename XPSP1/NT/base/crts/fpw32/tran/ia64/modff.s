.file "modff.s"

// Copyright (c) 2000, Intel Corporation
// All rights reserved.
// 
// Contributed 2/2/2000 by John Harrison, Ted Kubaska, Bob Norin, Shane Story,
// and Ping Tak Peter Tang of the Computational Software Lab, Intel Corporation.
// 
// WARRANTY DISCLAIMER
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
// 
// Intel Corporation is the author of this code, and requests that all
// problem reports or change requests be submitted to it directly at 
// http://developer.intel.com/opensource.
//
// History
//==============================================================
// 2/02/00: Initial version
// 4/04/00: Improved speed, corrected result for NaN input
//
// API
//==============================================================
// float modff(float x, float *iptr)
// break a floating point x number into fraction and an exponent
//
// input  floating point f8, address in r33
// output floating point f8 (x fraction), and *iptr (x integral part)
//
// OVERVIEW
//==============================================================

// NO FRACTIONAL PART: HUGE
// If
// for double-extended
// If the true exponent is greater than 63
//      1003e ==> 1003e -ffff = 3f = 63(dec)
// for double
// If the true exponent is greater than 52
//                10033 -ffff = 34 = 52(dec)
// for single
// If the true exponent is greater than 23
//                10016 -ffff = 17 = 23(dec)
// then
// we are already an integer (p9 true)

// NO INTEGER PART:    SMALL
//     Is f8 exponent less than register bias (that is, is it
//     less than 1). If it is, get the right sign of
//     zero and store this in iptr.

// CALCULATION: NOT HUGE, NOT SMALL
// To get the integer part
// Take the floating-point  input and truncate 
//   then convert  this integer to fp  Call it  MODF_INTEGER_PART

// Subtract  MODF_INTEGER_PART from MODF_NORM_F8 to get fraction part
// Then put fraction part in f8 
//      put integer  part MODF_INTEGER_PART into *iptr

// Registers used
//==============================================================

// predicate registers used: 
// p6 - p13

//                      0xFFFF           0x10016
// -----------------------+-----------------+-------------
//              SMALL     |      NORMAL     | HUGE
//    p11 --------------->|<----- p12 ----->| <-------------- p9
//    p10 --------------------------------->|
//    p13 --------------------------------------------------->|
//

// floating-point registers used: 
MODF_NORM_F8               = f9
MODF_FRACTION_PART         = f10
MODF_INTEGER_PART          = f11
MODF_INT_INTEGER_PART      = f12


// general registers used 
modf_signexp    = r14
modf_GR_10016   = r15
modf_GR_FFFF    = r16
modf_17_ones    = r17 
modf_exp        = r18
// r33 = iptr
     

.align 32
.global modff#

.section .text
.proc  modff#
.align 32


// Main path is p9, p11, p8 FALSE and p12 TRUE

// Assume input is normalized and get signexp
// Normalize input just in case
// Form exponent bias 
modff: 
{ .mfi
(p0)  getf.exp  modf_signexp = f8
(p0)  fnorm          MODF_NORM_F8  = f8
(p0)  addl           modf_GR_FFFF  = 0xffff, r0
}
// Get integer part of input
// Form exponent mask
{ .mfi
      nop.m 999
(p0)  fcvt.fx.trunc.s1  MODF_INT_INTEGER_PART   = f8
(p0)  mov  modf_17_ones     = 0x1ffff ;;
}

// Is x unnorm?
// qnan snan inf norm     unorm 0 -+
// 0    0    0   0        1     0 11 = 0x0b UNORM
// Form biased exponent where input only has an integer part
{ .mfi
      nop.m 999
(p0)  fclass.m.unc p8,p0 = f8, 0x0b
(p0)  addl modf_GR_10016 = 0x10016, r0 ;;
}

// Mask to get exponent
// Is x nan or inf?
// qnan snan inf norm     unorm 0 -+
// 1    1    1   0        0     0 11 = 0xe3 NAN_INF
// Set p13 to indicate calculation path, else p6 if nan or inf 
{ .mfi
(p0)  and       modf_exp = modf_17_ones, modf_signexp 
(p0)  fclass.m.unc p6,p13 = f8, 0xe3
      nop.i 999 ;;
}

// If x unorm get signexp from normalized input
// If x unorm get integer part from normalized input
{ .mfi
(p8)  getf.exp  modf_signexp = MODF_NORM_F8
(p8)  fcvt.fx.trunc  MODF_INT_INTEGER_PART   = MODF_NORM_F8
      nop.i 999 ;;
}

// If x unorm mask to get exponent
// Is x inf? p6 if inf, p7 if nan
{ .mfi
(p8)  and       modf_exp = modf_17_ones, modf_signexp
(p6)  fclass.m.unc p6,p7 = f8, 0x23
      nop.i 999 ;;
}

// p11 <== SMALL, no integer part, fraction is everyting
// p9  <== HUGE,  no fraction part, integer is everything
// p12 <== NORMAL, fraction part and integer part
{ .mii
(p13) cmp.lt.unc p11,p10 = modf_exp, modf_GR_FFFF
      nop.i 999
      nop.i 999 ;;
}

// For SMALL set fraction to normalized input, integer part to signed 0
{ .mfi
(p10) cmp.gt.unc p9,p12  = modf_exp, modf_GR_10016
(p11) fmerge.s MODF_INTEGER_PART = f8,f0
      nop.i 999
}
{ .mfi
      nop.m 999
(p11) fnorm.s   f8 = MODF_NORM_F8
      nop.i 999 ;;
}

// For HUGE set fraction to signed 0
{ .mfi
      nop.m 999
(p9)  fmerge.s f8 = f8,f0
      nop.i 999
}
// For NORMAL float the integer part
{ .mfi
      nop.m 999
(p12) fcvt.xf    MODF_INTEGER_PART = MODF_INT_INTEGER_PART
      nop.i 999 ;;
}

// If x inf set integer part to INF, fraction to signed 0
{ .mfi
(p6)  stfs [r33] = MODF_NORM_F8
(p6)  fmerge.s  f8 = f8,f0
      nop.i 999
}
// For HUGE set integer part to normalized input
{ .mfi
      nop.m 999
(p9)  fnorm.s MODF_INTEGER_PART = MODF_NORM_F8
      nop.i 999 ;;
}

// If x nan set integer and fraction parts to NaN (quietized)
{ .mfi
(p7)  stfs [r33] = MODF_NORM_F8
(p7)  fmerge.s  f8 = MODF_NORM_F8, MODF_NORM_F8
      nop.i 999 ;;
}

// For NORMAL compute fraction part
{ .mfi
      nop.m 999
(p12) fms.s.s0   f8 = MODF_NORM_F8,f1, MODF_INTEGER_PART
      nop.i 999
}
// For NORMAL test if fraction part is zero; if so append correct sign
{ .mfi
      nop.m 999
(p12) fcmp.eq.unc p12,p0 = MODF_NORM_F8, MODF_INTEGER_PART
      nop.i 999 ;;
}

// For NORMAL if fraction part is zero append sign of input
{ .mfb
(p13) stfs [r33] = MODF_INTEGER_PART
(p12) fmerge.s f8 = MODF_NORM_F8, f8
(p0)  br.ret.sptk    b0 ;;
}

.endp modff
