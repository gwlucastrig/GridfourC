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

	/**
	* Free any memory associated with the codec and otherwise dispose of resources.
	* It is expected that some codecs may include unique elements that must be managed
	* by the specific implementation.  So the GVRS API requires that codecs supply
	* their own clean-up functions.
	* @param a valid codec; implementatins should include handling to ignore a null reference.
	* @return a null reference.
	*/
	struct GvrsCodecTag* (*destroyCodec)(struct GvrsCodecTag* codec);

	/**
	* Allocate a new instance of a codec, initializing all internal elements as appropriate.
	* This function is intended for use in the GvrsBuilder implementation when initializing
	* a new GVRS instance.  This approach is necessary because each GVRS instance must have 
	* a unique and independent set of elements.  Some codecs may maintain state information.
	* This function allows specific implementations to create new instances of compressors
	* that manage their own internal information as necessary.
	* <p>
	* For example, the zlib API (Deflate algorithm) allows a compressor to specify the
	* level of compression. The default compression level is 6, but applications requiring
	* a higher compression level could specify a level of 9 (the maxiumum).  In such a case,
	* an application could create its own instance of a deflate codec and pass it in to
	* the GvrsBuilder.  The GvrsBuilder would then duplicate the information in the codec
	* when it allocates a new codec to be included in the GVRS object.
	* @param a valid codec; implementatins should include handling to ignore a null reference.
	* @return a valid reference to a codec.
	*/
	struct GvrsCodecTag* (*allocateNewCodec)(struct GvrsCodecTag* codec);

	void* appInfo;
}GvrsCodec;


typedef struct GvrsM32Tag {
	GvrsByte* buffer;
	GvrsInt   bufferLimit;
	GvrsInt   offset;
	int       bufferIsManaged;
}GvrsM32;

typedef struct GvrsBitInputTag {
	GvrsByte* text;
	int iBit;
	int nBytesInText;
	int nBytesProcessed;
	int scratch;
}GvrsBitInput;

/**
* Wraps the input in an M32 structure.
* <p>
* When a M32 structure is initialized using this call, the management of the memory
* for the buffer is assumed to be under the control of the calling application.
* When the M32 structure is freed, the buffer will not be modified.
* Note that this behavior is different than that of the alloc-for-output function.
* @param buffer an array of bytes supplying a sequence of one or more M32 codes.
* @param bufferLength the number of bytes in the buffer; because some M32 codes have
* multi-byte counts, this value may be larger than the number of symbols in the sequence.
* @return if successful, a valid reference; otherwise, a null reference.
*/
GvrsM32* GvrsM32Alloc(GvrsByte* buffer, GvrsInt bufferLength);
GvrsM32* GvrsM32Free(GvrsM32*);
GvrsInt  GvrsM32GetNextSymbol(GvrsM32*);

/**
* Allocates a M32 structure, including internal buffer.  The internal buffer is assumed
* to be under the management of the GvrsM32 functions.  When the allocated M32 structure is
* freed, its buffer will also be freed.  Note that this behavior is different than that of the
* alternate alloc function.
* @return if successful, a valid reference; otherwise, a null.
*/
GvrsM32* GvrsM32AllocForOutput();
int GvrsM32AppendSymbol(GvrsM32* m32, int symbol);

GvrsBitInput* GvrsBitInputAlloc( GvrsByte* text, size_t nBytesInText);
GvrsBitInput* GvrsBitInputFree(GvrsBitInput* input);
int GvrsBitInputGetBit( GvrsBitInput* input);
int GvrsBitInputGetByte(GvrsBitInput* input);
int GvrsBitInputGetPosition(GvrsBitInput* input);

void GvrsPredictor1(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor2(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor3(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);

GvrsM32* GvrsPredictor1encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, int* errCode);
GvrsM32* GvrsPredictor2encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, int* errCode);
GvrsM32* GvrsPredictor3encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, int* errCode);

#ifdef __cplusplus
}
#endif


#endif
