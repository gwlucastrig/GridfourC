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

#include "GvrsError.h"
#include "GvrsCodec.h"



static unsigned int mask[] = {
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

// in the logic below, nBit indicates the number of bits from the current
// "scratch" byte that remain.  If it reaches the value 8,
// then any access operation must advance to the next byte in the input text.

GvrsBitInput* GvrsBitInputAlloc(uint8_t* text, size_t nBytesInText, int* errorCode) {
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
	input->nBytesProcessed = 0;
	input->nBit = 0;
	return input;
}

int GvrsBitInputGetBit(GvrsBitInput* input) {
	if (input->nBit == 0) {
		input->scratch = input->text[input->nBytesProcessed++];
		input->nBit = 8;
	}
	int bit = (input->scratch) & 0x01u;
	input->scratch >>= 1;
	input->nBit--;
	return bit;
}

int GvrsBitInputGetByte(GvrsBitInput* input, int* errorCode) {

	if (input->nBit == 0) {
		// note that the value of input->nBit will remain as input->nBit = 0;
		// input->scratch is already invalid, and it will remain so.
		return input->text[input->nBytesProcessed++];
	}
	else if (input->nBit < 8) {
		input->scratch = (input->text[input->nBytesProcessed++] << input->nBit) | input->scratch;
		input->nBit += 8;
	}

	int result = input->scratch & 0xffu;
	input->scratch >>= 8;
	input->nBit -= 8;

	return result;
}

int GvrsBitInputGetBits(GvrsBitInput* input, int nBitsInValue) {
	if (nBitsInValue > 8 || nBitsInValue < 1) {
		return 0;
	}
	if (input->nBit < nBitsInValue) {
		input->scratch = (input->text[input->nBytesProcessed++] << input->nBit) | input->scratch;
		input->nBit += 8;
	}
	int result = input->scratch & mask[nBitsInValue];
	input->scratch >>= nBitsInValue;
	input->nBit -= nBitsInValue;
	return result;
}

int GvrsBitInputGetPosition(GvrsBitInput* input) {
	if (input->nBytesProcessed == 0) {
		return 0;
	}
	else if (input->nBit == 0) {
		return input->nBytesProcessed * 8;
	}
	else {
		return (input->nBytesProcessed - 1) * 8 + (8 - input->nBit);
	}
}

void GvrsBitInputSetState(GvrsBitInput* input, int nBytesProcessed, int nBit, unsigned int scratch) {
	input->nBytesProcessed = nBytesProcessed;
	input->nBit = nBit;
	input->scratch = scratch;
}

GvrsBitInput* GvrsBitInputFree(GvrsBitInput* input) {
	if (input) {
		input->text = 0;
		free(input);
	}
	return 0;
}