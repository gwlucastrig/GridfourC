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

#ifndef GVRS_FRAMEWORK_H
#define GVRS_FRAMEWORK_H

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define GVRS_CODEC_IDENTIFICATION_MAXLEN 16

typedef struct GvrsCodecTag {
	char identification[GVRS_CODEC_IDENTIFICATION_MAXLEN+1]; // follows GVRS Identifier syntax
	int  index; // populated by GvrsOpen at run time
	char* description; // arbitrary string supplied by CODEC implementation

	// A CODEC has the option of implementing one or more of the following 
	// functions by populating the appropriate pointer-to-a-function.
	// The GVRS runtime will check to see whether a function pointer is populated
	// before invoking it.  When encoding data, the GVRS API uses the pointer
	// to indicate if an encoding is applicable to a particular data type.
	int (*decodeInt)(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsInt* data, void *appInfo);
	int (*decodeFloat)(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsFloat* data, void *appInfo);

	GvrsByte* (*encodeInt)(int nRow, int nColumn, GvrsInt* data, int index, int* packingLength, int *errCode, void *appInfo);
	GvrsByte* (*encodeFloat)(int nRow, int nColumn, GvrsFloat* data, int index, int* packingLength, int* errCode, void *appInfo);

	// We expect that some codecs may include unique elements that must be managed
	// by the specific implementation.  So we require that codecs supply their own
	// clean-up function.
	void* (*destroyCodec)(struct GvrsCodecTag*);

	void* appInfo;
}GvrsCodec;


typedef struct GvrsM32Tag {
	GvrsByte* buffer;
	GvrsInt   bufferLimit;
	GvrsInt   offset;
	
}GvrsM32;

typedef struct GvrsBitInputTag {
	GvrsByte* text;
	int iBit;
	int nBytesInText;
	int nBytesProcessed;
	int scratch;
}GvrsBitInput;

GvrsM32* GvrsM32Alloc(GvrsByte* buffer, GvrsInt bufferLength);
GvrsM32* GvrsM32Free(GvrsM32*);
GvrsInt  GvrsM32GetNextSymbol(GvrsM32*);

GvrsBitInput* GvrsBitInputAlloc( GvrsByte* text, size_t nBytesInText);
GvrsBitInput* GvrsBitInputFree(GvrsBitInput* input);
int GvrsBitInputGetBit( GvrsBitInput* input);
int GvrsBitInputGetByte(GvrsBitInput* input);
int GvrsBitInputGetPosition(GvrsBitInput* input);

void GvrsPredictor1(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor2(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor3(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);

#ifdef __cplusplus
}
#endif


#endif
