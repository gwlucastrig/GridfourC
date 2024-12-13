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


#define TEXT_GROWTH_FACTOR  8192

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


int GvrsBitOutputAlloc(GvrsBitOutput** outputReference) {
	if (!outputReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*outputReference = 0;
	GvrsBitOutput* bout = calloc(1, sizeof(GvrsBitOutput));
	if (!bout) {
		return GVRSERR_NOMEM;
	}
	bout->text = calloc(TEXT_GROWTH_FACTOR, sizeof(uint8_t));
	bout->nBytesAllocated = TEXT_GROWTH_FACTOR;
	if (!bout->text) {
		free(bout);
		return GVRSERR_NOMEM;
	}

	*outputReference = bout;
	return 0;
}
 

GvrsBitOutput* GvrsBitOutputFree(GvrsBitOutput* output) {
	if (output) {
		if (output->text) {
			free(output->text);
			output->text = 0;
		}
		memset(output, 0, sizeof(GvrsBitOutput)); // diagnostic
		free(output);
	}
	return 0;
}


 int growText(GvrsBitOutput* output, int growthSize) {
	int nGrowth = output->nBytesAllocated + growthSize;
	uint8_t* p = (uint8_t* )malloc((size_t)nGrowth * sizeof(uint8_t));
	if (!p) {
		return GVRSERR_NOMEM;
	}
	memmove(p, output->text, output->nBytesAllocated);
	memset(p + output->nBytesAllocated, 0, growthSize);
	free(output->text);
	output->text = p;
	output->nBytesAllocated = nGrowth;
	return 0;
}
  

int GvrsBitOutputPutBit(GvrsBitOutput* output, int bit) {
	if (output->iBit == 0) {
		output->scratch = bit&0x01;
		output->iBit = 1;
	}else{
		output->scratch |= ((bit & 1) << output->iBit);
		output->iBit++;
		if (output->iBit == 8) {
			output->text[output->nBytesProcessed++] = output->scratch;
			output->scratch = 0;
			output->iBit = 0;
			if (output->nBytesProcessed == output->nBytesAllocated) {
				return  growText(output, TEXT_GROWTH_FACTOR);
			}
		}
	}
	return 0;
}

int GvrsBitOutputPutByte(GvrsBitOutput* output, int symbol) {
	if (output->iBit == 0) {
		output->text[output->nBytesProcessed++] = (uint8_t)(symbol&0xff);
	}
	else {
		// if we get here, iBit is not aligned with a byte boundary.
	    // We need to combine bits from the current "scratch" byte with
	    // part of the bits in the next symbol.
		// store the low-order bits of the symbol in the current text position (e.g. scratch)
		// and then advance to the next text position, storing the high-order bits in scratch.
		int nBitsConsumed = output->iBit;
		int nBitsAvailable = 8 - nBitsConsumed;
		output->scratch |= (uint8_t)((symbol << nBitsConsumed) & 0xff);
		output->text[output->nBytesProcessed++] = output->scratch;
		output->scratch = (uint8_t)((symbol>>nBitsAvailable) & mask[nBitsConsumed]);  // TO DO: is mask required?
		// output->iBit = nBitsConsumed;   iBit doesn't actually change
	}

	if (output->nBytesProcessed == output->nBytesAllocated) {
		return growText(output, TEXT_GROWTH_FACTOR);
	}
	return 0;
}

int GvrsBitOutputReserveBytes(GvrsBitOutput* output, int nBytesToReserve, uint8_t** reservedByteReference){
	if (!output || !reservedByteReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (nBytesToReserve < 0) {
		return GVRSERR_INVALID_PARAMETER;
	}

	*reservedByteReference = 0;

	if (output->iBit) {
		// one or more bits in the scratch element have not yet been transferred
		// to the internal text array.  This action is essentially a flush operation.
		output->text[output->nBytesProcessed++] = (uint8_t)(output->scratch & 0xff);
		output->iBit = 0;
	}
	int available = output->nBytesAllocated - (output->nBytesProcessed + nBytesToReserve + 1);
	if (available<=0) {
		int shortfall = -available;
		int status = growText(output, shortfall + TEXT_GROWTH_FACTOR);
		if (status) {
			return status;
		}
	}

	*reservedByteReference = output->text + output->nBytesProcessed;
	output->nBytesProcessed += nBytesToReserve;
	return 0;
}

int GvrsBitOutputGetText(GvrsBitOutput* output, int* nBytesInText, uint8_t** text) {
	if (!output || !output->text || !text || !nBytesInText) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*nBytesInText = 0;
	*text = 0;
	uint8_t* t = malloc((size_t)(output->nBytesProcessed + 1)*sizeof(uint8_t));
	if (!t) {
		return GVRSERR_NOMEM;
	}
	memmove(t, output->text, output->nBytesProcessed);
	if (output->iBit) {
		// one or more bits in the scratch element have not yet been transferred
		// to the internal text array
		t[output->nBytesProcessed] = (uint8_t)(output->scratch&0xff);
		*nBytesInText = output->nBytesProcessed + 1;
	}
	else {
		*nBytesInText = output->nBytesProcessed;
	}
	*text = t;
	
	return 0;
}
 
int GvrsBitOutputGetBitCount(GvrsBitOutput* output) {
	if (output) {
		return output->nBytesProcessed * 8 + output->iBit;
	}
	return 0; // Actually, this is an error condition.  Let it go.
}

int GvrsBitOutputFlush(GvrsBitOutput* output) {
	if (!output) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (output->iBit) {
		output->text[output->nBytesProcessed++] = output->scratch;
		output->iBit = 0;
		output->scratch = 0;
		if (output->nBytesProcessed == output->nBytesAllocated) {
			return growText(output, TEXT_GROWTH_FACTOR);
		}
	}
	return 0;
}