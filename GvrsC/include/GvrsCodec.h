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

#ifndef GVRS_CODEC_H
#define GVRS_CODEC_H

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

	int (*encodeInt)(  int nRow, int nColumn, GvrsInt* data,   int index, int* packingLength, GvrsByte** packingReference, void *appInfo);
	int (*encodeFloat)(int nRow, int nColumn, GvrsFloat* data, int index, int* packingLength, GvrsByte** packingReference, void *appInfo);

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

typedef struct GvrsBitOutputTag {
	GvrsByte* text;
	int iBit;
	int nBytesAllocated;
	int nBytesProcessed;
	int scratch;
}GvrsBitOutput;


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
* @param m32 a pointer to a pointer for a variable to receive the address for the GvrsM32 structure.
* @return if successful, zero; otherwise an integer value indicating an error condition.
*/
int  GvrsM32Alloc(GvrsByte* buffer, GvrsInt bufferLength, GvrsM32** m32);

/**
* Deallocates the memory associated with the m32 codec.
* @param a valid m32 reference.
* @return a null.
*/
GvrsM32* GvrsM32Free(GvrsM32* m32);

/**
* Get the next integer value provided by the GvrsM32 instance.  The responsibility for determining
* whether additional symbols are available is left to the application.  If no future symbols are
* available, this function returns an INT_MIN (indicating "no data").  Note that in ordinary use,
* an INT_MIN is a valid symbol, so that value cannot be used to detect an end-of-data condition.
* @param m32 a valid instance of a M32 codec.
* @return if successful, a valid integer; otherwise an INT_MIN.
*/
GvrsInt  GvrsM32GetNextSymbol(GvrsM32* m32);

/**
* Allocates a M32 structure, including internal buffer.  The internal buffer is assumed
* to be under the management of the GvrsM32 functions.  When the allocated M32 structure is
* freed, its buffer will also be freed.  Note that this behavior is different than that of the
* alternate alloc function.
* @return if successful, a valid reference; otherwise, a null.
*/
GvrsM32* GvrsM32AllocForOutput();

/**
* Appends the specified integer value to the specified m32 encoding.
* @param m32 a valid instance, initialized for output.
* @return if successful, zero; otherwise an integer value indicating an error condition.
*/
int GvrsM32AppendSymbol(GvrsM32* m32, int symbol);

GvrsBitInput* GvrsBitInputAlloc(GvrsByte* text, size_t nBytesInText, int *errorCode);
GvrsBitInput* GvrsBitInputFree( GvrsBitInput* input);
int GvrsBitInputGetBit( GvrsBitInput* input, int *errorCode);
int GvrsBitInputGetByte(GvrsBitInput* input, int *errorCode);
int GvrsBitInputGetPosition(GvrsBitInput* input);

int GvrsBitOutputAlloc(GvrsBitOutput** outputReference);
int GvrsBitOutputPutBit(GvrsBitOutput* output, int bit);
int GvrsBitOutputPutByte(GvrsBitOutput* output, int symbol);
int GvrsBitOutputReserveBytes(GvrsBitOutput* output, int nBytesToReserve, GvrsByte** reservedByteReference);
int GvrsBitOutputGetBitCount(GvrsBitOutput* output);
int GvrsBitOutputFlush(GvrsBitOutput* output);
GvrsBitOutput* GvrsBitOutputFree(GvrsBitOutput* output);

/**
* Get a safe copy of the current text in the output.  The memory allocated for
* the text is assumed to be under the management of the application and will
* not be affected by subsequent calls to GVRS bit operations.
* @param output a valid instance.
* @param nBytesAllocated a pointer to a variable to receive the number of bytes in the text
* @param text a pointer to a pointer variable to receive the text.
* @return if successful, a value of zero; otherwise an error code indicating the cause of the failure.
*/
int GvrsBitOutputGetText(GvrsBitOutput* output, int* nBytesInText, GvrsByte** text);

void GvrsPredictor1(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor2(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);
void GvrsPredictor3(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output);

int GvrsPredictor1encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, GvrsM32** m32);
int GvrsPredictor2encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, GvrsM32** m32);
int GvrsPredictor3encode(int nRows, int nColumns, GvrsInt* values, GvrsInt *encodedSeed, GvrsM32** m32);


GvrsCodec* GvrsCodecHuffmanAlloc();
#ifdef GVRS_ZLIB
GvrsCodec* GvrsCodecDeflateAlloc();
GvrsCodec* GvrsCodecFloatAlloc();
GvrsCodec* GvrsCodecLsopAlloc();
#endif



#ifdef __cplusplus
}
#endif


#endif
