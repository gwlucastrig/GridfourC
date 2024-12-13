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

#include <math.h>
#include "GvrsFramework.h"

#include "GvrsCrossPlatform.h"
#include "GvrsError.h"
#include "GvrsCodec.h"
#include "zlib.h"

GvrsCodec* GvrsCodecLsopAlloc();

/**
 * A code value indicating that the post-prediction code sequence was
 * compressed using Huffman coding in the Gridfour format
 */
static int COMPRESSION_TYPE_HUFFMAN = 0;
/**
 * A code value indicating that the post-prediction code sequence was compressed
 * using the Deflate library.
 */
static int COMPRESSION_TYPE_DEFLATE = 1;
/**
 * A mask for extracting the compression type from a packing.
 */

static int COMPRESSION_TYPE_MASK = 0x0f;
/**
 * A bit flag indicating that the packing includes a checksum.
*/
int VALUE_CHECKSUM_INCLUDED = 0x80;



static int32_t unpackInteger(uint8_t input[], int offset) {
	return (input[offset] & 0xff)
		| ((input[offset + 1] & 0xff) << 8)
		| ((input[offset + 2] & 0xff) << 16)
		| ((input[offset + 3] & 0xff) << 24);
}

static float unpackFloat(uint8_t input[], int offset) {
	float f=0;
	int32_t* p = (int32_t* )(&f);
	*p = (input[offset] & 0xff)
		| ((input[offset + 1] & 0xff) << 8)
		| ((input[offset + 2] & 0xff) << 16)
		| ((input[offset + 3] & 0xff) << 24);
	return f;
}

typedef struct LsHeaderTag {
	int codecIndex;
	int nCoefficients;
	int seed;
	float u[12];  // up to 12 coefficients
	int nInitializerCodes;
	int nInteriorCodes;
	int compressionType;
	int headerSize;
	int valueChecksumIncluded;
	int32_t  valueChecksum;
}LsHeader;

// the header is 15+N*4 bytes:
//   for 8 predictor coefficients:  47 bytes
//   for 12 predictor coefficients: 63 bytes
//    1 byte     codecIndex
//    1 byte     number of predictor coefficients (N)
//    4 bytes    seed
//    N*4 bytes  predictor coefficients
//    4 bytes    nInitializationCodes
//    4 bytes    nInteriorCodes
//    1 byte     method

 
// case-sensitive name of codec
static const char* identification = "LSOP12";
static const char* description = "Implements the optional LSOP compression";

static GvrsCodec* destroyCodecLsop(struct GvrsCodecTag* codec) {
	if (codec) {
		if (codec->description) {
			free(codec->description);
			codec->description = 0;
		}
		free(codec);
	}
	return 0;
}
static GvrsCodec* allocateCodecLsop(struct GvrsCodecTag* codec) {
	return GvrsCodecLsopAlloc();
}
 

static int doHuff(GvrsBitInput* input, int nSymbols, uint8_t* output) {
	int indexSize;
	int errCode = 0;
	int* nodeIndex;
	errCode = GvrsHuffmanDecodeTree(input, &indexSize, &nodeIndex);
	if (errCode) {
		// nodeIndex will be null.
		return errCode;
	}
 
	errCode = GvrsHuffmanDecodeText(input, indexSize, nodeIndex, nSymbols, output);
 
	free(nodeIndex);
	return errCode;
}



static int doInflate(uint8_t* input, int inputLength, uint8_t* output, int outputLength, int *inputUsed) {
	Bytef* zInput = (Bytef*)input;
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = inputLength;
	zs.next_in = zInput;
	zs.avail_out = (uInt)outputLength;
	zs.next_out = (Bytef*)output;

	inflateInit2(&zs, 15);
	int status = inflate(&zs, Z_FINISH);
	inflateEnd(&zs);
	if (status != Z_STREAM_END) {
		return  GVRSERR_BAD_COMPRESSION_FORMAT;
	}

	// *inputUsed = (int)(zs.next_in - input);
	*inputUsed = zs.total_in;
	return 0;
}



static void cleanUp(uint8_t* initializerCodes, uint8_t* interiorCodes, GvrsBitInput* inputBits) {
	if (initializerCodes) {
		free(initializerCodes);
	}
	if (interiorCodes) {
		free(interiorCodes);
	}
	if (inputBits) {
		GvrsBitInputFree(inputBits);
	}
}


static int decodeInt(int nRows, int nColumns, int packingLength, uint8_t* packing, int32_t* values, void *appInfo) {
	int i, iRow, iCol;
	uint8_t* initializerCodes = 0;
	uint8_t* interiorCodes = 0;
	GvrsBitInput* inputBits = 0;

// the header is 15+N*4 bytes:
//   for 8 predictor coefficients:  47 bytes
//   for 12 predictor coefficients: 63 bytes
//    1 byte     codecIndex
//    1 byte     number of predictor coefficients (N)
//    4 bytes    seed
//    N*4 bytes  predictor coefficients
//    4 bytes    nInitializationCodes
//    4 bytes    nInteriorCodes
//    1 byte     method
 
	// int codecIndex = packing[0];
	int nCoefficients = packing[1];
	if (nCoefficients != 12) {
		// the 8-coefficient variation is not implemented
		return GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
	}

	int32_t seed = unpackInteger(packing, 2);
	int offset = 6;
	float u[12]; // room for up to 12 coefficients
	for (i = 0; i <nCoefficients; i++) {
		u[i] = unpackFloat(packing, offset);
		offset += 4;
	}
	int32_t nInitializerCodes = unpackInteger(packing, offset);
	offset += 4;
	int32_t nInteriorCodes = unpackInteger(packing, offset);
	offset += 4;
	int method = packing[offset++];
	int compressionType = method&COMPRESSION_TYPE_MASK;
	int valueChecksumIncluded = (method & VALUE_CHECKSUM_INCLUDED) != 0;
	if (valueChecksumIncluded) {
		// int32_t valueChecksum = unpackInteger(packing, filePos);
		offset += 4;
	}

	initializerCodes = (uint8_t* )malloc(nInitializerCodes);
	if (!initializerCodes) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return GVRSERR_NOMEM;
	}
	 interiorCodes = (uint8_t* )malloc(nInteriorCodes);
	if (!interiorCodes) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return GVRSERR_NOMEM;
	}


	int status = 0;
	uint8_t* inputBytes = packing + offset;
	int inputBytesLength = packingLength - offset;
	if (compressionType == COMPRESSION_TYPE_HUFFMAN) {
		inputBits = GvrsBitInputAlloc(inputBytes, inputBytesLength, &status);
		if (!inputBits) {
			cleanUp(initializerCodes, interiorCodes, inputBits);
			return status;
		}
		status =  doHuff(inputBits, nInitializerCodes, initializerCodes );
		if (status) {
			cleanUp(initializerCodes, interiorCodes, inputBits);
			return status;
		}
		status = doHuff(inputBits, nInteriorCodes, interiorCodes);
		if (status) {
			cleanUp(initializerCodes, interiorCodes, inputBits);
			return status;
		}
		inputBits = GvrsBitInputFree(inputBits);
	}
	else if (compressionType == COMPRESSION_TYPE_DEFLATE) {
		// TO DO:  inflate.  The next_in from the inflate structure will let me
		//                   know how many bytes were used?  Maybe one of the other elements.
		int inputUsed;
		status = doInflate(inputBytes, inputBytesLength, initializerCodes, nInitializerCodes, &inputUsed);
		if (status) {
			cleanUp(initializerCodes, interiorCodes, inputBits);
			return status;
		}
		inputBytes += inputUsed;
		inputBytesLength -= inputUsed;
		status = doInflate(inputBytes, inputBytesLength, interiorCodes, nInteriorCodes, &inputUsed);
		if (status) {
			cleanUp(initializerCodes, interiorCodes, inputBits);
			return status;
		}
	}
	else {
		return GVRSERR_BAD_COMPRESSION_FORMAT;
	}

	GvrsM32* mInit;
	status = GvrsM32Alloc(initializerCodes, nInitializerCodes, &mInit);
	if (status) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return status;
	}

	// step 1, the first row -------------------
	values[0] = seed;
	int v = seed;
	for (i = 1; i < nColumns; i++) {
		v += GvrsM32GetNextSymbol(mInit);
		values[i] = v;
	}

	// step 2, the left column -------------------------
	v = seed;
	for (i = 1; i < nRows; i++) {
		v += GvrsM32GetNextSymbol(mInit);
		values[i * nColumns] = v;
	}

	// now use the triangle predictor ------------------------------
	// step 4, the second row
	for (i = 1; i < nColumns; i++) {
		int index = nColumns + i;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(GvrsM32GetNextSymbol(mInit) + ((a + c) - b));
	}

	// step 5, the second column ------------------------
	for (i = 2; i < nRows; i++) {
		int index = i * nColumns + 1;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(GvrsM32GetNextSymbol(mInit) + ((a + c) - b));
	}

	// Although the array u[] is indexed from zero, the coefficients
	// for the predictors are numbered starting at one. Here we copy them
	// out so that the code will match up with the indexing used in the
	// original papers.  There may be some small performance gain to be
	// had by not indexing the array u[] multiple times in the loop below,
	// but that is not the main motivation for using the copy variables.
	float u1 = u[0];
	float u2 = u[1];
	float u3 = u[2];
	float u4 = u[3];
	float u5 = u[4];
	float u6 = u[5];
	float u7 = u[6];
	float u8 = u[7];
	float u9 = u[8];
	float u10 = u[9];
	float u11 = u[10];
	float u12 = u[11];

	GvrsM32* m32;
	status = GvrsM32Alloc(interiorCodes, nInteriorCodes, &m32);
	if (status) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return status;
	}

	// in the loop below, we wish to economize on processing by copying
	 // the neighbor values into local variables.  In the inner (column)
	 // loop, as each raster cell is processed, the local copies of the z values
	 // (z1, z2, etc.) are shifted to the left to populate the neighbor
	 // values for the next cell in the loop.
	 // The reason we do this is two-fold. First, due to arry bounds checking,
	 // accessing an array element in Java is more expensive than reading
	 // a local variable. Second, promoting an integer to a float also
	 // carries a small overhead. The shifting strategy below helps
	 // save that processing.  In testing, the extra coding for local variables
	 // resulted in about a 10 percent reduction in processing time.
	 //
	 //   For a given grid cell of interest P, the layout for neighbors is
	 //                              iCol
	 //        iRow      z6      z1    P     --    --
	 //        iRow-1    z7      z2    z3    z4    z5
	 //        iRow-2    z8      z9    z10   z11   z12
	 //
	 //  For example, as we increment iCol, the z1 value from the first iteration
	 //  becomes the z6 value for the second, the z2 value from the first becomes
	 //  the z7 value for the second, etc.
	for (iRow = 2; iRow < nRows; iRow++) {
		int index = iRow * nColumns + 2;
		float z1 = (float)values[index - 1];
		float z2 = (float)values[index - nColumns - 1];
		float z3 = (float)values[index - nColumns];
		float z4 = (float)values[index - nColumns + 1];
		float z5; // = values[index - nColumns + 2];  computed below
		float z6 = (float)values[index - 2];
		float z7 = (float)values[index - nColumns - 2];
		float z8 = (float)values[index - 2 * nColumns - 2];
		float z9 = (float)values[index - 2 * nColumns - 1];
		float z10 = (float)values[index - 2 * nColumns];
		float z11 = (float)values[index - 2 * nColumns + 1];
		float z12; // values[index - 2 * nColumns + 2];  computed below
		for (iCol = 2; iCol < nColumns - 2; iCol++) {
			index = iRow * nColumns + iCol;
			z5 = (float)values[index - nColumns + 2];
			z12 = (float)values[index - 2 * nColumns + 2];
			float p = u1 * z1
				+ u2 * z2
				+ u3 * z3
				+ u4 * z4
				+ u5 * z5
				+ u6 * z6
				+ u7 * z7
				+ u8 * z8
				+ u9 * z9
				+ u10 * z10
				+ u11 * z11
				+ u12 * z12;
			int estimate = (int)floorf(p+0.5f);
			values[index] = estimate + GvrsM32GetNextSymbol(m32);

			// perform the shifting operation for all variables so that
			// only z5 and z12 will have to be read from the values array.
			z6 = z1;
			z1 = (float)values[index];

			z7 = z2;
			z2 = z3;
			z3 = z4;
			z4 = z5;

			z8 = z9;
			z9 = z10;
			z10 = z11;
			z11 = z12;
		}

		// The last two columns in the row are "unreachable" to
		// the Optimal Predictor and must be populated using some other
		// predictor.  In this case, we apply the Triangle Predictor.
		index = iRow * nColumns + nColumns - 2;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(GvrsM32GetNextSymbol(mInit) + ((a + c) - b));
		index++;
		a = values[index - 1];
		b = values[index - nColumns - 1];
		c = values[index - nColumns];
		values[index] = (int)(GvrsM32GetNextSymbol(mInit) + ((a + c) - b));
	}
	
	GvrsM32Free(mInit);
	GvrsM32Free(m32);
	cleanUp(initializerCodes, interiorCodes, inputBits);


	 
	return 0;
}

GvrsCodec* GvrsCodecLsopAlloc() {
    GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
    GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
    codec->description = GVRS_STRDUP(description);
    codec->decodeInt = decodeInt;
	codec->destroyCodec = destroyCodecLsop;
	codec->allocateNewCodec = allocateCodecLsop;
	return codec;
}
