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
static int COMPRESSION_TYPE_CANON_HUFFMAN = 2;
/**
 * A code value indicating that the post-prediction code sequence was compressed
 * using Gridfour's canonical Huffman format.
 */
static int COMPRESSION_TYPE_DEFLATE = 1;
/**
 * A mask for extracting the compression type from a packing.
 */


static int COMPRESSION_TYPE_MASK = 0x0f;
/**
 * A bit flag indicating that the packing includes a checksum.
*/
static int VALUE_CHECKSUM_INCLUDED = 0x80;

/**
A bit flag indicating that the LSOP header is stored in the revised format
*/
static int REVISION_FLAG = 0x40;


static int32_t unpackInteger(uint8_t input[], int offset) {
	return (input[offset] & 0xff)
		| ((input[offset + 1] & 0xff) << 8)
		| ((input[offset + 2] & 0xff) << 16)
		| ((input[offset + 3] & 0xff) << 24);
}

static float unpackFloat(uint8_t input[], int offset) {
	float f = 0;
	int32_t* p = (int32_t*)(&f);
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



static int doInflate(uint8_t* input, int inputLength, uint8_t* output, int outputLength, int* inputUsed) {
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



static void cleanUp(
	uint8_t* initializerM32,
	uint8_t* interiorM32,
	int* intInitializer,
	int* intInterior,
	GvrsBitInput* inputBits) {
	if (initializerM32) {
		free(initializerM32);
	}
	if (interiorM32) {
		free(interiorM32);
	}
	if (intInitializer) {
		free(intInitializer);
	}
	if (intInterior) {
		free(intInterior);
	}
	if (inputBits) {
		GvrsBitInputFree(inputBits);
	}
}

static int decodeInitializers(int nRows, int nColumns, int seed, int* initializerInt, int* values) {
	int k = 0;
	int i;
	// step 1, the first row -------------------
	values[0] = seed;
	int v = seed;
	for (i = 1; i < nColumns; i++) {
		v += initializerInt[k++];
		values[i] = v;
	}

	// step 2, the left column -------------------------
	v = seed;
	for (i = 1; i < nRows; i++) {
		v += initializerInt[k++];
		values[i * nColumns] = v;
	}

	// now use the triangle predictor ------------------------------
	// step 4, the second row
	for (i = 1; i < nColumns; i++) {
		int index = nColumns + i;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(initializerInt[k++] + ((a + c) - b));
	}

	// step 5, the second column ------------------------
	for (i = 2; i < nRows; i++) {
		int index = i * nColumns + 1;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(initializerInt[k++] + ((a + c) - b));
	}
	return k;
}

static void decodeInterior(int nRows, int nColumns, float* u, int initializerOffset, int* interiorInt, int* initializerInt, int* values) {
	int k = 0;
	int kInit = initializerOffset;
	int iRow;
	int iCol;

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
			int estimate = (int)floorf(p + 0.5f);
			values[index] = estimate + interiorInt[k++];

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
		// Also note that we count the position in the initializer array using kInit, not k
		index = iRow * nColumns + nColumns - 2;
		long a = values[index - 1];
		long b = values[index - nColumns - 1];
		long c = values[index - nColumns];
		values[index] = (int)(initializerInt[kInit++] + ((a + c) - b));
		index++;
		a = values[index - 1];
		b = values[index - nColumns - 1];
		c = values[index - nColumns];
		values[index] = (int)(initializerInt[kInit++] + ((a + c) - b));
	}




}


static int decodeInt(int nRows, int nColumns, int packingLength, uint8_t* packing, int32_t* values, void* appInfo) {

	// The following pointers are used to track memory that will be allocated and freed.
	uint8_t* initializerM32 = 0;  // sotrage for extracted m32 codes
	uint8_t* interiorM32 = 0;
	int* intInitializer = 0;  // storage for extracted integer "corrector" codes
	int* intInterior = 0;
	GvrsBitInput* inputBits = 0;  // storage for bit sources used by Huffman variations

	// In order to support legacy formats, there are two layouts used for
		// the header.
		//  Legacy
		//   the header is 15+N*4 bytes + 4 byte optional checksum:
		//   for 8 predictor coefficients:  47 bytes
		//   for 12 predictor coefficients: 63 bytes
		//    1 byte     codecIndex
		//    1 byte     number of predictor coefficients (N)
		//    4 bytes    seed
		//    N*4 bytes  coefficients
		//    4 bytes    nInitializationCodes
		//    4 bytes    nInteriorM32
		//    1 byte     method
		//    if checksum is included
		//       4 bytes value checksum
		//  Revised added to support Canonical Huffman
		//   the header is 7+N*4 bytes + 8 bytes if not canonical huffman + optional checksum:
		//    1 byte     codecIndex
		//    1 byte
		//        bit 7:     checksum included
		//        bit 6:     revision flag
		//        bit 5:     reserved
		//        bits 0 to 4:  type code
		//    1 byte     number of predictor coefficients (N)
		//    4 bytes    seed
		//    N*4 bytes  coefficients
		//    if type is deflate or legacy Huffman
		//       4 bytes    nInitializationCodes
		//       4 bytes    nInteriorM32
		//    if checksum is included
		//       4 bites value checksum


	int i;
	// int codecIndex = packing[0];
	int packingFlags = packing[1];
	int revisionTest = packingFlags & REVISION_FLAG;
	int nCoefficients;
	int32_t seed;
	int offset;
	float u[12]; // room for up to 12 coefficients
	int32_t nInitializerM32;  // the number of m32 codes
	int32_t nInteriorM32;
	int nIntInitializer; // the number of integers for predictor input
	int nIntInterior;

	int method;
	int compressionType;;
	int valueChecksumIncluded;
	int headerSize;



	if (revisionTest == 0) {
		// Legacy configuration -----------------------------------
		nCoefficients = packing[1];
		if (nCoefficients != 12) {
			// the 8-coefficient variation is not implemented
			return GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
		}

		seed = unpackInteger(packing, 2);
		offset = 6;
		for (i = 0; i < nCoefficients; i++) {
			u[i] = unpackFloat(packing, offset);
			offset += 4;
		}
		nInitializerM32 = unpackInteger(packing, offset);
		offset += 4;
		nInteriorM32 = unpackInteger(packing, offset);
		offset += 4;
		method = packing[offset++];
		compressionType = method & COMPRESSION_TYPE_MASK;
		valueChecksumIncluded = (method & VALUE_CHECKSUM_INCLUDED) != 0;
		if (valueChecksumIncluded) {
			// int32_t valueChecksum = unpackInteger(packing, filePos);
			offset += 4;
		}
		headerSize = offset;
	}
	else {
		// Revised configuration ----------------------------------
		method = packing[1];
		valueChecksumIncluded = (packing[1] & VALUE_CHECKSUM_INCLUDED) != 0;
		compressionType = method & COMPRESSION_TYPE_MASK;
		nCoefficients = packing[2];
		if (nCoefficients != 12) {
			// the 8-coefficient variation is not implemented
			return GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
		}

		seed = unpackInteger(packing, 3);
		offset = 7;
		for (i = 0; i < nCoefficients; i++) {
			u[i] = unpackFloat(packing, offset);
			offset += 4;
		}

		if (compressionType == COMPRESSION_TYPE_CANON_HUFFMAN) {
			nInitializerM32 = 0;
			nInteriorM32 = 0;
		}
		else {
			nInitializerM32 = unpackInteger(packing, offset);
			offset += 4;
			nInteriorM32 = unpackInteger(packing, offset);
			offset += 4;
		}
		if (valueChecksumIncluded) {
			// int32_t valueChecksum = unpackInteger(packing, filePos);
			offset += 4;
		}
		headerSize = offset;

	}

	int status = 0;
	uint8_t* inputBytes = packing + offset;
	int inputBytesLength = packingLength - offset;

	nIntInitializer = nRows * 4 + nColumns * 2 - 9;
	nIntInterior = (nRows - 2) * (nColumns - 4);

	//intInitializer = calloc((size_t)(nIntInitializer + 1), sizeof(int));
	//intInterior = calloc((size_t)(nIntInterior + 1), sizeof(int));
	intInitializer = (int*)malloc((size_t)(nIntInitializer + 1) * sizeof(int));
	intInterior = (int*)malloc((size_t)(nIntInterior + 1) * sizeof(int));
	if (!intInitializer || !intInterior) {
		cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
		return GVRSERR_NOMEM;
	}

	if (compressionType == COMPRESSION_TYPE_CANON_HUFFMAN) {
		inputBits = GvrsBitInputAlloc(inputBytes, inputBytesLength, &status);
		if (!inputBits) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return status;
		}

		status = GvrsCanonicalHuffmanDecode(inputBits, nIntInitializer + 1, intInitializer, (void*)0);
		if (status) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return status;
		}

		status = GvrsCanonicalHuffmanDecode(inputBits, nIntInterior + 1, intInterior, (void*)0);
		if (status) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return status;
		}
		inputBits = GvrsBitInputFree(inputBits);
	}
	else {
		// Legacy --------------------------------------------------------------
		//   Both the legacy Huffman and the Deflate decode the input bytes to populate
		//   arrays of bytes containing m32-formatted data.  These, are then expanded
		//   into integer arrays used by the LSOP predictor code.

		initializerM32 = (uint8_t*)malloc(nInitializerM32);
		interiorM32 = (uint8_t*)malloc(nInteriorM32);
		if (!initializerM32 || !interiorM32) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return GVRSERR_NOMEM;
		}


		if (compressionType == COMPRESSION_TYPE_HUFFMAN) {
			inputBits = GvrsBitInputAlloc(inputBytes, inputBytesLength, &status);
			if (!inputBits) {
				cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
				return status;
			}
			status = doHuff(inputBits, nInitializerM32, initializerM32);
			if (status) {
				cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
				return status;
			}
			status = doHuff(inputBits, nInteriorM32, interiorM32);
			if (status) {
				cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
				return status;
			}
			inputBits = GvrsBitInputFree(inputBits);
		}
		else if (compressionType == COMPRESSION_TYPE_DEFLATE) {
			// TO DO:  inflate.  The next_in from the inflate structure will let me
			//                   know how many bytes were used?  Maybe one of the other elements.
			int inputUsed;
			status = doInflate(inputBytes, inputBytesLength, initializerM32, nInitializerM32, &inputUsed);
			if (status) {
				cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
				return status;
			}
			inputBytes += inputUsed;
			inputBytesLength -= inputUsed;
			status = doInflate(inputBytes, inputBytesLength, interiorM32, nInteriorM32, &inputUsed);
			if (status) {
				cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
				return status;
			}
		}
		else {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return GVRSERR_BAD_COMPRESSION_FORMAT;
		}


		GvrsM32* mInit = 0;
		status = GvrsM32Alloc(initializerM32, nInitializerM32, &mInit);
		if (status) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return status;
		}

		for (i = 0; i < nIntInitializer; i++) {
			intInitializer[i] = GvrsM32GetNextSymbol(mInit);
		}
		mInit = GvrsM32Free(mInit);
		free(initializerM32);
		initializerM32 = 0;

		GvrsM32* mInterior = 0;
		status = GvrsM32Alloc(interiorM32, nInteriorM32, &mInterior);
		if (status) {
			cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);
			return status;
		}
		for (i = 0; i < nIntInterior; i++) {
			intInterior[i] = GvrsM32GetNextSymbol(mInterior);
		}
		mInterior = GvrsM32Free(mInterior);
		free(interiorM32);
		interiorM32 = 0;
	}

	int kInit = decodeInitializers(nRows, nColumns, seed, intInitializer, values);
	decodeInterior(nRows, nColumns, u, kInit, intInterior, intInitializer, values);

	cleanUp(initializerM32, interiorM32, intInitializer, intInterior, inputBits);

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
