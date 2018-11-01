// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once



inline unsigned int countLeadingZeros32(unsigned int x)
{
	unsigned long result = 32 ^ 31;
	if (!_BitScanReverse(&result, x))
		result = 32 ^ 31; 
	PREFAST_SUPPRESS_WARNING(6102);
	result ^= 31; // needed because the index is from LSB (whereas all other implementations are from MSB)
	return result;
}

inline unsigned long long countLeadingZeros64(unsigned long long x)
{
#if CRY_PLATFORM_X64
	unsigned long result = 64 ^ 63;
	if (!_BitScanReverse64(&result, x))
		result = 64 ^ 63;
	PREFAST_SUPPRESS_WARNING(6102);
	result ^= 63; // needed because the index is from LSB (whereas all other implementations are from MSB)
#else
	unsigned long result = (x & 0xFFFFFFFF00000000ULL)
		? countLeadingZeros32(unsigned int(x >> 32)) +  0
		: countLeadingZeros32(unsigned int(x >>  0)) + 32;
#endif
	return result;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Count the number of trailing zeros
// Result ranges from 0 to 32/64
#if (CRY_COMPILER_GCC || CRY_COMPILER_CLANG)

	// builtin doesn't want to work for 0
	#define countTrailingZeros32(x) ((x) ? __builtin_ctz  (x) : 32)
	#define countTrailingZeros64(x) ((x) ? __builtin_ctzll(x) : 64)

#elif CRY_PLATFORM_BMI1
	#include "ammintrin.h"

	#define countTrailingZeros32(x) _tzcnt_u32(x)
	#define countTrailingZeros64(x) _tzcnt_u64(x)

#else  // Windows implementation

inline unsigned int countTrailingZeros32(unsigned int x)
{
	unsigned long result = 32;
	if (!_BitScanForward(&result, x))
		result = 32;
	PREFAST_SUPPRESS_WARNING(6102);
	return result;
}

inline unsigned long long countTrailingZeros64(unsigned long long x)
{
#if CRY_PLATFORM_X64
	unsigned long result = 64;
	if (!_BitScanForward64(&result, x))
		result = 64;
	PREFAST_SUPPRESS_WARNING(6102);
#else
	unsigned long result = (x & 0x00000000FFFFFFFFULL)
		? countTrailingZeros32(unsigned int(x >>  0)) +  0
		: countTrailingZeros32(unsigned int(x >> 32)) + 32;
#endif
	return result;
}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CRY_COMPILER_GCC || CRY_COMPILER_CLANG

inline unsigned int circularShift(unsigned int shiftBits, unsigned int value)
{
	return (value << shiftBits) | (value >> (32 - shiftBits));
}

#else  // Windows implementation

	#define circularShift(shiftBits, value) _rotl(value, shiftBits)

#endif

#if CRY_PLATFORM_BMI1

	#define isolateLowestBit(x) _blsi_u32(x)
	#define clearLowestBit(x) _blsr_u32(x)

#else  // Windows implementation

inline unsigned int isolateLowestBit(unsigned int x)
{
	PREFAST_SUPPRESS_WARNING(4146);
	return x & (-x);
}

inline unsigned int clearLowestBit(unsigned int x)
{
	return x & (x - 1);
}
#endif

#if CRY_PLATFORM_TBM

	#define fillFromLowestBit32(x) _blsfill_u32(x)
	#define fillFromLowestBit64(x) _blsfill_u64(x)

#else

inline unsigned int fillFromLowestBit32(unsigned int x)
{
	return x | (x - 1);
}

inline unsigned long long fillFromLowestBit64(unsigned long long x)
{
	return x | (x - 1);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBitIter
{
public:
	inline CBitIter(unsigned char  x) : m_val(x) {}
	inline CBitIter(unsigned short x) : m_val(x) {}
	inline CBitIter(unsigned int x) : m_val(x) {}

	template<class T>
	inline bool Next(T& rIndex)
	{
		unsigned int maskLSB = isolateLowestBit(m_val);
		rIndex = countTrailingZeros32(maskLSB);
		m_val ^= maskLSB;
		return maskLSB != 0;
	}

private:
	unsigned int m_val;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename TInteger>
inline bool IsPowerOfTwo(TInteger x)
{
	return clearLowestBit(x) == 0;
}

//! Compile-time version of IsPowerOfTwo, useful for STATIC_CHECK.
template<int nValue>
struct IsPowerOfTwoCompileTime
{
	enum { IsPowerOfTwo = ((nValue & (nValue - 1)) == 0) };
};

// Calculates the base-2 logarithm for a given number.
// Passing 0 will return -1 in an unsigned datatype.
// Result ranges from -1 to 7/15/31/63
#if CRY_COMPILER_GCC || CRY_COMPILER_CLANG

static inline unsigned char  IntegerLog2(unsigned char  v) { return unsigned char (31U   - __builtin_clz  (v)); }
static inline unsigned short IntegerLog2(unsigned short v) { return unsigned short(31U   - __builtin_clz  (v)); }
static inline unsigned int IntegerLog2(unsigned int v) { return unsigned int(31UL  - __builtin_clz  (v)); }
static inline unsigned long long IntegerLog2(unsigned long long v) { return unsigned long long(63ULL - __builtin_clzll(v)); }

#else  // Windows implementation

static inline unsigned char  IntegerLog2(unsigned char  v) { unsigned long result = ~0U; _BitScanReverse  (&result, v); return unsigned char (result); }
static inline unsigned short IntegerLog2(unsigned short v) { unsigned long result = ~0U; _BitScanReverse  (&result, v); return unsigned short(result); }
static inline unsigned int IntegerLog2(unsigned int v) { unsigned long result = ~0U; _BitScanReverse  (&result, v); return unsigned int(result); }
#if CRY_PLATFORM_X64
static inline unsigned long long IntegerLog2(unsigned long long v) { unsigned long result = ~0U; _BitScanReverse64(&result, v); return unsigned long long(result); }
#else
static inline unsigned long long IntegerLog2(unsigned long long v)
{
	unsigned long result = ~0U;
	if      (v & 0xFFFFFFFF00000000ULL) _BitScanReverse(&result, unsigned int(v >> 32)), result += 32;
	else if (v & 0x00000000FFFFFFFFULL) _BitScanReverse(&result, unsigned int(v >>  0)), result +=  0;
	return result;
}
#endif

#endif

// Calculates the number of bits needed to represent a number.
// Passing 0 will return the maximum number of bits.
// Result ranges from 0 to 8/16/32/64
static inline unsigned char  IntegerLog2_RoundUp(unsigned char  v) { return unsigned char (IntegerLog2(unsigned char (v - 1U  )) + 1U  ); }
static inline unsigned short IntegerLog2_RoundUp(unsigned short v) { return unsigned short(IntegerLog2(unsigned short(v - 1U  )) + 1U  ); }
static inline unsigned int IntegerLog2_RoundUp(unsigned int v) { return unsigned int(IntegerLog2(unsigned int(v - 1UL )) + 1UL ); }
static inline unsigned long long IntegerLog2_RoundUp(unsigned long long v) { return unsigned long long(IntegerLog2(unsigned long long(v - 1ULL)) + 1ULL); }

// Calculates the power-of-2 upper range of a given number.
// Passing 0 will return 1 on x86 (shift modulo datatype size is no shift) and 0 on other platforms.
// Result ranges from 1 to all bits set.
inline unsigned int NextPower2(unsigned int x)
{
	return 1UL << IntegerLog2_RoundUp(x);
}

inline unsigned long long NextPower2_64(unsigned long long x)
{
	return 1ULL << IntegerLog2_RoundUp(x);
}

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
inline unsigned long int IntegerLog2(unsigned long int x)
{
	#if CRY_PLATFORM_64BIT
	return IntegerLog2((unsigned long long)x);
	#else
	return IntegerLog2((unsigned int)x);
	#endif
}

#endif

#if CRY_PLATFORM_ORBIS
inline size_t IntegerLog2(size_t x)
{
	if (sizeof(size_t) == sizeof(unsigned int))
		return IntegerLog2((unsigned int)x);
	else
		return IntegerLog2((unsigned long long)x);
}
#endif

// Find-first-MSB-set
static inline unsigned char BitIndex(unsigned char  v) { return unsigned char(IntegerLog2(v)); }
static inline unsigned char BitIndex(unsigned short v) { return unsigned char(IntegerLog2(v)); }
static inline unsigned char BitIndex(unsigned int v) { return unsigned char(IntegerLog2(v)); }
static inline unsigned char BitIndex(unsigned long long v) { return unsigned char(IntegerLog2(v)); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculates the number of bits set.
#if CRY_COMPILER_GCC || CRY_COMPILER_CLANG

static inline unsigned char CountBits(unsigned char  v) { return unsigned char(__builtin_popcount  (v)); }
static inline unsigned char CountBits(unsigned short v) { return unsigned char(__builtin_popcount  (v)); }
static inline unsigned char CountBits(unsigned int v) { return unsigned char(__builtin_popcount  (v)); }
static inline unsigned char CountBits(unsigned long long v) { return unsigned char(__builtin_popcountll(v)); }

#elif CRY_PLATFORM_SSE4

static inline unsigned char CountBits(unsigned char  v) { return unsigned char(__popcnt  (v)); }
static inline unsigned char CountBits(unsigned short v) { return unsigned char(__popcnt  (v)); }
static inline unsigned char CountBits(unsigned int v) { return unsigned char(__popcnt  (v)); }
static inline unsigned char CountBits(unsigned long long v) { return unsigned char(__popcnt64(v)); }

#else

static inline unsigned char CountBits(unsigned char v)
{
	unsigned char c = v;
	c = ((c >> 1) & 0x55) + (c & 0x55);
	c = ((c >> 2) & 0x33) + (c & 0x33);
	c = ((c >> 4) & 0x0f) + (c & 0x0f);
	return c;
}

static inline unsigned char CountBits(unsigned short v)
{
	return
		CountBits((unsigned char)(v & 0xff)) +
		CountBits((unsigned char)((v >> 8) & 0xff));
}

static inline unsigned char CountBits(unsigned int v)
{
	return
		CountBits((unsigned short)(v & 0xffff)) +
		CountBits((unsigned short)((v >> 16) & 0xffff));
}

static inline unsigned char CountBits(unsigned long long v)
{
	return
		CountBits((unsigned int)(v & 0xffffffff)) +
		CountBits((unsigned int)((v >> 32) & 0xffffffff));
}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//! Branchless version of "return v < 0 ? alt : v;".
inline int Isel32(int v, int alt)
{
	return ((static_cast<int>(v) >> 31) & alt) | ((static_cast<int>(~v) >> 31) & v);
}

template<unsigned int ILOG>
struct CompileTimeIntegerLog2
{
	static const unsigned int result = 1 + CompileTimeIntegerLog2<(ILOG >> 1)>::result;
};

template<>
struct CompileTimeIntegerLog2<1>
{
	static const unsigned int result = 0;
};

template<>
struct CompileTimeIntegerLog2<0>; //!< Keep it undefined, we cannot represent "minus infinity" result.

static_assert(CompileTimeIntegerLog2<1>::result == 0, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<2>::result == 1, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<3>::result == 1, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<4>::result == 2, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<5>::result == 2, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<255>::result == 7, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<256>::result == 8, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2<257>::result == 8, "Wrong calculation result!");

template<unsigned int ILOG>
struct CompileTimeIntegerLog2_RoundUp
{
	static const unsigned int result = CompileTimeIntegerLog2<ILOG>::result + ((ILOG & (ILOG - 1)) != 0);
};
template<>
struct CompileTimeIntegerLog2_RoundUp<0>; //!< We can return 0, but let's keep it undefined (same as CompileTimeIntegerLog2<0>).

static_assert(CompileTimeIntegerLog2_RoundUp<1>::result == 0, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<2>::result == 1, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<3>::result == 2, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<4>::result == 2, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<5>::result == 3, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<255>::result == 8, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<256>::result == 8, "Wrong calculation result!");
static_assert(CompileTimeIntegerLog2_RoundUp<257>::result == 9, "Wrong calculation result!");

// Character-to-bitfield mapping

inline unsigned int AlphaBit(char c)
{
	return c >= 'a' && c <= 'z' ? 1 << (c - 'z' + 31) : 0;
}

inline unsigned long long AlphaBit64(char c)
{
	return (c >= 'a' && c <= 'z' ? 1U << (c - 'z' + 31) : 0) |
	       (c >= 'A' && c <= 'Z' ? 1LL << (c - 'Z' + 63) : 0);
}

inline unsigned int AlphaBits(unsigned int wc)
{
	// Handle wide multi-char constants, can be evaluated at compile-time.
	return AlphaBit((char)wc)
	       | AlphaBit((char)(wc >> 8))
	       | AlphaBit((char)(wc >> 16))
	       | AlphaBit((char)(wc >> 24));
}

inline unsigned int AlphaBits(const char* s)
{
	// Handle string of any length.
	unsigned int n = 0;
	while (*s)
		n |= AlphaBit(*s++);
	return n;
}

inline unsigned long long AlphaBits64(const char* s)
{
	// Handle string of any length.
	unsigned long long n = 0;
	while (*s)
		n |= AlphaBit64(*s++);
	return n;
}

//! \param s should point to a buffer at least 65 chars long
inline void BitsAlpha64(unsigned long long n, char* s)
{
	for (int i = 0; n != 0; n >>= 1, i++)
		if (n & 1)
			*s++ = i < 32 ? i + 'z' - 31 : i + 'Z' - 63;
	*s++ = '\0';
}

//! If the hardware doesn't support 3Dc we can convert to DXT5 (different channels are used) with almost the same quality but the same memory requirements.
inline void ConvertBlock3DcToDXT5(unsigned char pDstBlock[16], const unsigned char pSrcBlock[16])
{
	assert(pDstBlock != pSrcBlock);   // does not work in place

	// 4x4 block requires 8 bytes in DXT5 or 3DC

	// DXT5:  8 bit alpha0, 8 bit alpha1, 16*3 bit alpha lerp
	//        16bit col0, 16 bit col1 (R5G6B5 low byte then high byte), 16*2 bit color lerp

	//  3DC:  8 bit x0, 8 bit x1, 16*3 bit x lerp
	//        8 bit y0, 8 bit y1, 16*3 bit y lerp

	for (unsigned int dwK = 0; dwK < 8; ++dwK)
		pDstBlock[dwK] = pSrcBlock[dwK];
	for (unsigned int dwK = 8; dwK < 16; ++dwK)
		pDstBlock[dwK] = 0;

	// 6 bit green channel (highest bits)
	// by using all 3 channels with a slight offset we can get more precision but then a dot product would be needed in PS
	// because of bilinear filter we cannot just distribute bits to get perfect result
	unsigned short colDst0 = (((unsigned short)pSrcBlock[8] + 2) >> 2) << 5;
	unsigned short colDst1 = (((unsigned short)pSrcBlock[9] + 2) >> 2) << 5;

	bool bFlip = colDst0 <= colDst1;

	if (bFlip)
	{
		unsigned short help = colDst0;
		colDst0 = colDst1;
		colDst1 = help;
	}

	bool bEqual = colDst0 == colDst1;

	// distribute bytes by hand to not have problems with endianess
	pDstBlock[8 + 0] = (unsigned char)colDst0;
	pDstBlock[8 + 1] = (unsigned char)(colDst0 >> 8);
	pDstBlock[8 + 2] = (unsigned char)colDst1;
	pDstBlock[8 + 3] = (unsigned char)(colDst1 >> 8);

	unsigned short* pSrcBlock16 = (unsigned short*)(pSrcBlock + 10);
	unsigned short* pDstBlock16 = (unsigned short*)(pDstBlock + 12);

	// distribute 16 3 bit values to 16 2 bit values (loosing LSB)
	for (unsigned int dwK = 0; dwK < 16; ++dwK)
	{
		unsigned int dwBit0 = dwK * 3 + 0;
		unsigned int dwBit1 = dwK * 3 + 1;
		unsigned int dwBit2 = dwK * 3 + 2;

		unsigned char hexDataIn = (((pSrcBlock16[(dwBit2 >> 4)] >> (dwBit2 & 0xf)) & 1) << 2)     // get HSB
		                  | (((pSrcBlock16[(dwBit1 >> 4)] >> (dwBit1 & 0xf)) & 1) << 1)
		                  | ((pSrcBlock16[(dwBit0 >> 4)] >> (dwBit0 & 0xf)) & 1); // get LSB

		unsigned char hexDataOut = 0;

		switch (hexDataIn)
		{
		case 0:
			hexDataOut = 0;
			break;                        // color 0
		case 1:
			hexDataOut = 1;
			break;                        // color 1

		case 2:
			hexDataOut = 0;
			break;                        // mostly color 0
		case 3:
			hexDataOut = 2;
			break;
		case 4:
			hexDataOut = 2;
			break;
		case 5:
			hexDataOut = 3;
			break;
		case 6:
			hexDataOut = 3;
			break;
		case 7:
			hexDataOut = 1;
			break;                        // mostly color 1

		default:
			assert(0);
			break;
		}

		if (bFlip)
		{
			if (hexDataOut < 2)
				hexDataOut = 1 - hexDataOut;    // 0<->1
			else
				hexDataOut = 5 - hexDataOut;  // 2<->3
		}

		if (bEqual)
			if (hexDataOut == 3)
				hexDataOut = 1;

		pDstBlock16[(dwK >> 3)] |= (hexDataOut << ((dwK & 0x7) << 1));
	}
}

//! Is a bit on in a new bit field, but off in an old bit field.
static inline bool TurnedOnBit(unsigned bit, unsigned oldBits, unsigned newBits)
{
	return (newBits & bit) != 0 && (oldBits & bit) == 0;
}

inline unsigned int cellUtilCountLeadingZero(unsigned int x)
{
	unsigned int y;
	unsigned int n = 32;

	y = x >> 16;
	if (y != 0) { n = n - 16; x = y; }
	y = x >> 8;
	if (y != 0) { n = n - 8; x = y; }
	y = x >> 4;
	if (y != 0) { n = n - 4; x = y; }
	y = x >> 2;
	if (y != 0) { n = n - 2; x = y; }
	y = x >> 1;
	if (y != 0) { return n - 2; }
	return n - x;
}

inline unsigned int cellUtilLog2(unsigned int x)
{
	return 31 - cellUtilCountLeadingZero(x);
}

inline void convertSwizzle(
  unsigned char*& dst, const unsigned char*& src,
  const unsigned int SrcPitch, const unsigned int depth,
  const unsigned int xpos, const unsigned int ypos,
  const unsigned int SciX1, const unsigned int SciY1,
  const unsigned int SciX2, const unsigned int SciY2,
  const unsigned int level)
{
	if (level == 1)
	{
		switch (depth)
		{
		case 16:
			if (xpos >= SciX1 && xpos < SciX2 && ypos >= SciY1 && ypos < SciY2)
			{
				//					*((unsigned int*&)dst)++ = ((unsigned int*)src)[ypos * width + xpos];
				//					*((unsigned int*&)dst)++ = ((unsigned int*)src)[ypos * width + xpos+1];
				//					*((unsigned int*&)dst)++ = ((unsigned int*)src)[ypos * width + xpos+2];
				//					*((unsigned int*&)dst)++ = ((unsigned int*)src)[ypos * width + xpos+3];
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 16)));
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 16 + 4)));
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 16 + 8)));
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 16 + 12)));
			}
			else
				((unsigned int*&)dst) += 4;
			break;
		case 8:
			if (xpos >= SciX1 && xpos < SciX2 && ypos >= SciY1 && ypos < SciY2)
			{
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 8)));
				*((unsigned int*&)dst)++ = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 8 + 4)));
			}
			else
				((unsigned int*&)dst) += 2;
			break;
		case 4:
			if (xpos >= SciX1 && xpos < SciX2 && ypos >= SciY1 && ypos < SciY2)
				*((unsigned int*&)dst) = *((unsigned int*)(src + (ypos * SrcPitch + xpos * 4)));
			dst += 4;
			break;
		case 3:
			if (xpos >= SciX1 && xpos < SciX2 && ypos >= SciY1 && ypos < SciY2)
			{
				*dst++ = src[ypos * SrcPitch + xpos * depth];
				*dst++ = src[ypos * SrcPitch + xpos * depth + 1];
				*dst++ = src[ypos * SrcPitch + xpos * depth + 2];
			}
			else
				dst += 3;
			break;
		case 1:
			if (xpos >= SciX1 && xpos < SciX2 && ypos >= SciY1 && ypos < SciY2)
				*dst++ = src[ypos * SrcPitch + xpos * depth];
			else
				dst++;
			break;
		default:
			assert(0);
			break;
		}
		return;
	}
	else
	{
		convertSwizzle(dst, src, SrcPitch, depth, xpos, ypos, SciX1, SciY1, SciX2, SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth, xpos + (1U << (level - 2)), ypos, SciX1, SciY1, SciX2, SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth, xpos, ypos + (1U << (level - 2)), SciX1, SciY1, SciX2, SciY2, level - 1);
		convertSwizzle(dst, src, SrcPitch, depth, xpos + (1U << (level - 2)), ypos + (1U << (level - 2)), SciX1, SciY1, SciX2, SciY2, level - 1);
	}
}

inline void Linear2Swizzle(
  unsigned char* dst,
  const unsigned char* src,
  const unsigned int SrcPitch,
  const unsigned int width,
  const unsigned int height,
  const unsigned int depth,
  const unsigned int SciX1, const unsigned int SciY1,
  const unsigned int SciX2, const unsigned int SciY2)
{
	src -= SciY1 * SrcPitch + SciX1 * depth;
	if (width == height)
		convertSwizzle(dst, src, SrcPitch, depth, 0, 0, SciX1, SciY1, SciX2, SciY2, cellUtilLog2(width) + 1);
	else if (width > height)
	{
		unsigned int baseLevel = cellUtilLog2(width) - (cellUtilLog2(width) - cellUtilLog2(height));
		for (unsigned int i = 0; i < (1UL << (cellUtilLog2(width) - cellUtilLog2(height))); i++)
			convertSwizzle(dst, src, SrcPitch, depth, (1U << baseLevel) * i, 0, SciX1, SciY1, SciX2, SciY2, baseLevel + 1);
	}
	else
	{
		unsigned int baseLevel = cellUtilLog2(height) - (cellUtilLog2(height) - cellUtilLog2(width));
		for (unsigned int i = 0; i < (1UL << (cellUtilLog2(height) - cellUtilLog2(width))); i++)
			convertSwizzle(dst, src, SrcPitch, depth, 0, (1U << baseLevel) * i, SciX1, SciY1, SciX2, SciY2, baseLevel + 1);
	}
}
