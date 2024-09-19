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

// in the logic below, iBit indicates the number of bits from the current
// "scratch" byte that have been consumed.  If it reaches the value 8,
// then any access operation must advance to the next byte in the input text.

static int mask[] = {
	0x00,
	0x01,
	0x03,
	0x07,
	0x0f,
	0x1f,
	0x3f,
	0x7f,
	0xff
};

GvrsBitInput* GvrsBitInputAlloc(GvrsByte* text, size_t nBytesInText, int *errorCode) {
	GvrsBitInput* input = calloc(1, sizeof(GvrsBitInput));
	if (!input) {
		*errorCode = GVRSERR_NOMEM;
		return 0;
	}
	if (nBytesInText < 1) {
		*errorCode = GVRSERR_FILE_ERROR;
	}
	input->text = text;
	input->nBytesInText = (int)nBytesInText;
	input->scratch = text[0];
	input->nBytesProcessed = 0;
	input->iBit = 8;
	return input;
}

int GvrsBitInputGetBit(GvrsBitInput* input) {
	if (input->iBit == 8) {
		if (input->nBytesProcessed >= input->nBytesInText) {
			// This is an encoding error.  Just return a zero-value
			return 0;
		}
		input->scratch = input->text[input->nBytesProcessed++];
		input->iBit = 0;
	}
	int bit = (input->scratch) & 1;
	(input->scratch) >>= 1;
	input->iBit++;
	return bit;
}

int GvrsBitInputGetByte(GvrsBitInput* input, int *errorCode){
	 
	// at this point, the process is going to require one more byte.
	// if there is no more data left, an error occurs
	if (input->nBytesProcessed >= input->nBytesInText) {
		*errorCode = GVRSERR_FILE_ERROR;
		return 0;
	}

	if (input->iBit == 8) {
		// note that the value of input->iBit will remain as input->iBit = 8;
		// input->scratch is already invalid, and it will remain so.
		return input->text[input->nBytesProcessed++];
	}

	// if we get here, iBit is not aligned with a byte boundary.
	// We need to combine bits from the current "scratch" byte with
	// part of the bits in the next symbol.
	
	int nBitsNeeded = input->iBit;  // the process already consumed iBits from scratch
	int nBitsRemaining = 8 - nBitsNeeded;
	int a = input->scratch;
	int b = input->text[input->nBytesProcessed++];
	int c = b & mask[nBitsNeeded];
	int result = a | (c << nBitsRemaining);
	input->scratch = b >> nBitsNeeded;
	input->iBit = nBitsNeeded;
	return result;
}

int GvrsBitInputGetPosition(GvrsBitInput* input) {
	if (input->nBytesProcessed == 0) {
		return 0;
	}
	else {
		return (input->nBytesProcessed - 1) * 8 + input->iBit;
	}
}


GvrsBitInput* GvrsBitInputFree(GvrsBitInput* input) {
	if (input) {
		input->text = 0;
		free(input);
	}
	return 0;
}