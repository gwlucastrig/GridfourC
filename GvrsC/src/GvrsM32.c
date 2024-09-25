/* --------------------------------------------------------------------
 *
 * The MIT License
 *
 * Copyright (C) 2024  Gary W. Lucas.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ---------------------------------------------------------------------
 */

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"
#include "GvrsError.h"
#include "GvrsCodec.h"


int GvrsM32Alloc(GvrsByte* input, GvrsInt inputLength, GvrsM32** m32Reference) {
	if (!input || !m32Reference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*m32Reference = 0;
	GvrsM32* m32 = calloc(1, sizeof(GvrsM32));
	if (m32) {
		m32->buffer = input;
		m32->bufferLimit = inputLength;
		*m32Reference = m32;
		return 0;
	}
	return GVRSERR_NOMEM;;
}


GvrsM32* GvrsM32Free(GvrsM32* m32) {
	if (m32) {
		if (m32->bufferIsManaged) {
			free(m32->buffer);
			m32->buffer = 0;
		}
		memset(m32, 0, sizeof(GvrsM32)); // for diagnostic purposes.
		free(m32);
	}
	return 0;
}

#define N_SEGMENTS_MAX 5
static int segmentBaseValue[] = {
			127, 255, 16639, 2113791, 270549247
};


int GvrsM32GetNextSymbol(GvrsM32 *m32) {
	int i;
	int limit = m32->bufferLimit;
	GvrsByte* buffer = m32->buffer;

	if (m32->offset >= limit) {
		return INT_MIN; // arbitrary null data code
	}

	int symbol = (int)buffer[m32->offset++];
	// the signed byte value for -128 is 0x80
	if (symbol == 0x80) {
		return INT_MIN;
	}

	// The M32 codes treat the initial byte as a signed quantity.
	// Since GvrsByte is unsigned, we need to do some manipulation
	// to convert the value to a signed integer.   If the symbol is in the
	// range -127 < symbol < 127, it is complete in one-byte
	if (symbol & 0x80) {
		symbol |= 0xffffff00;
		if (symbol > -127) {
			return symbol;
		}
	}
	else if (symbol < 127) {
		return symbol;
	}
	
	// The symbol extends to multiple bytes

	int delta=0;
	for (i = 0; i < N_SEGMENTS_MAX; i++) {
		if (m32->offset >= limit) {
			return INT_MIN;
		}
		int sample = buffer[m32->offset++];
		delta = (delta << 7) | (sample & 0x7f);
		// if the high-bit is clear, the sequence is complete
		if (!(sample & 0x80)) {
			// we need special handling for the case where
			// the return value would be Integer.MIN_VALUE
			// because of the asymmetric nature of signed integer.
			// and -Integer.MIN_VALUE does not equal Integer.MAX_VALUE.
			if (symbol == -127) {
				return  -delta - segmentBaseValue[i];
			}
			else {
				return delta + segmentBaseValue[i];
			}
		}
	}
	return 0;
}

GvrsM32*
GvrsM32AllocForOutput() {
	GvrsM32* m32 = calloc(1, sizeof(GvrsM32));
	if (m32) {
		m32->buffer = malloc(8192);
		if (!m32->buffer) {
			free(m32);
			return 0;
		}
		m32->bufferLimit = 8192;
		m32->bufferIsManaged = 1;
	}
	return m32;
}


static const int loMask = 0b01111111; // 0x7f
static const int hiBit = 0b10000000;  // 0x80


int
GvrsM32AppendSymbol(GvrsM32* m32, int symbol) {
	//  Bytes required for range of values
	//    1             0           126
	//    2           127           254
	//    3           255         16638
	//    4         16639       2113790
	//    5       2113791     270549246
	//    6     270549247    2147483647
	//

	// The max length of a symbol encoding is 6. Make sure that there is at least enough room
	// remaining in the buffer to hold 6 bytes.
	if (m32->offset + 6 >= m32->bufferLimit) {
		m32->bufferLimit += 8192;
		GvrsByte* b = realloc(m32->buffer, m32->bufferLimit);
		if (!b) {
			// malloc failed, do nothing
			return GVRSERR_NOMEM;
		}
		m32->buffer = b;

	}
	GvrsByte* buffer = m32->buffer;
	int absValue;
	if (symbol < 0) {
		if (symbol == INT_MIN) {
			buffer[m32->offset++] = (GvrsByte)(-128);   // 0x80
			return 0;
		}
		else if (symbol > -127) {
			buffer[m32->offset++] = (GvrsByte)symbol;
			return 0;
		}
		buffer[m32->offset++] = (GvrsByte)(-127);
		absValue = -symbol;
	}
	else {
		if (symbol < 127) {
			buffer[m32->offset++] = (GvrsByte)symbol;
			return 0;
		}
		buffer[m32->offset++] = (GvrsByte)127;
		absValue = symbol;
	}

	if (absValue <= 254) {
		int delta = absValue - 127;
		buffer[m32->offset++] = (GvrsByte)delta;
	}
	else if (absValue <= 16638) {
		int delta = absValue - 255;
		buffer[m32->offset++] = (GvrsByte)(((delta >> 7) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(delta & loMask);
	}
	else if (absValue <= 2113790) {
		int delta = absValue - 16639;
		buffer[m32->offset++] = (GvrsByte)(((delta >> 14) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 7) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(delta & loMask);
	}
	else if (absValue <= 270549246) {
		int delta = absValue - 2113791;
		buffer[m32->offset++] = (GvrsByte)(((delta >> 21) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 14) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 7) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(delta & loMask);
	}
	else {
		int delta = absValue - 270549247;
		buffer[m32->offset++] = (GvrsByte)(((delta >> 28) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 21) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 14) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(((delta >> 7) & loMask) | hiBit);
		buffer[m32->offset++] = (GvrsByte)(delta & loMask);
	}
	return 0;
}

 