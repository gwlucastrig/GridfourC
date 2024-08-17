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
#include "GvrsPrimaryTypes.h"
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



static GvrsInt unpackInteger(GvrsByte input[], int offset) {
	return (input[offset] & 0xff)
		| ((input[offset + 1] & 0xff) << 8)
		| ((input[offset + 2] & 0xff) << 16)
		| ((input[offset + 3] & 0xff) << 24);
}

static GvrsFloat unpackFloat(GvrsByte input[], int offset) {
	GvrsFloat f=0;
	GvrsInt* p = (GvrsInt *)(&f);
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
	GvrsBoolean valueChecksumIncluded;
	GvrsInt  valueChecksum;
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
 
static GvrsInt* decodeTree(GvrsBitInput* input,  int *indexSize, int *errCode) {
	*errCode = 0;
	int nLeafsToDecode = GvrsBitInputGetByte(input, errCode) + 1;
	int rootBit = GvrsBitInputGetBit(input, errCode);
	int* nodeIndex;

	if (rootBit == 1) {
		// This is the special case where a non-zero root
		// bit indicates that there is only one symbol in the whole
		// encoding.   There is not a proper tree, only a single root node.
		nodeIndex = (GvrsInt*)malloc(sizeof(GvrsInt));
		if (nodeIndex == 0) {
			*errCode = GVRSERR_NOMEM;
			return 0;
		}
		nodeIndex[0] = 1;
		*indexSize = 1;
		return nodeIndex;
	}

	// This particular implementation follows the lead of the original Java
	// code in that it manages the Huffman tree using an integer array.
	// The array based representation of the Huffman tree
	// is laid out as triplets of integer values each
	// representing a node
	//     [offset+0] symbol code (or -1 for a branch node)
	//     [offset+1] index to left child node (zero if leaf)
	//     [offset+2] index to right child node (zero if leaf)
	// The maximum number of symbols is 256.  The number of nodes in
	// a Huffman tree is always 2*n-1.  We allocate 3 integers per
	// node, or 2*n*3.
	int nodeIndexSize = nLeafsToDecode * 6;
	nodeIndex = calloc(nodeIndexSize, sizeof(GvrsInt));
	if (!nodeIndex) {
		*errCode = GVRSERR_NOMEM;
		return 0;
	}



   // Although it would be simpler to decode the Huffman tree using a recursive
   // function approach, the depth of the recursion end up being too large
   // for some software environments. The maximum depth of a Huffman tree is
   // the number of symbols minus 1. Since there are a maximum of 256
   // symbols, the maximum depth of the tree could be as large as 255.
   // While the probability of that actually happening is extremely small
   // we address it by using a stack-based implementation rather than recursion.
   //
   // Initialization:
   //    The root node is virtually placed on the base of the stack.
   // It's left and right child-node references are zeroed out.
   // The node type is set to -1 to indicate a branch node.
   // The iStack variable is always set to the index of the element
   // on top of the stack.

	int stackSize = nLeafsToDecode + 1;
	int* stack = calloc(stackSize, sizeof(GvrsInt));
	if (!stack) {
		free(nodeIndex);
		*errCode = GVRSERR_NOMEM;
		return 0;
	}

	int iStack = 0;
	int nodeIndexCount = 3;
	nodeIndex[0] = -1;
	nodeIndex[1] = 0;
	nodeIndex[2] = 0;
 

	int nLeafsDecoded = 0;
	while (nLeafsDecoded < nLeafsToDecode) {
		int offset = stack[iStack];
		// We are going to generate a new node. It will be stored
		// in the node-index array starting at position nodeIndexCount.
		// We are going to store an integer reference to the new node as
		// one of the child nodes of the node at the current position
		// on the stack.  If the left-side node is already populates (offset+1),
		// we will store the reference as the right-side child node (offset+2).
		if (nodeIndex[offset + 1] == 0) {
			nodeIndex[offset + 1] = nodeIndexCount;
		}
		else {
			nodeIndex[offset + 2] = nodeIndexCount;
		}

		int bit = GvrsBitInputGetBit(input, errCode);  
		if (bit == 1) {
			if (iStack >= stackSize || nodeIndexCount + 3 >= nodeIndexSize) {
				free(stack);
				free(nodeIndex);
				*errCode = GVRSERR_BAD_COMPRESSION_FORMAT;
				return 0;
			}
			// leaf node
			nLeafsDecoded++;
			nodeIndex[nodeIndexCount++] = GvrsBitInputGetByte(input, errCode);   
			nodeIndex[nodeIndexCount++] = 0; // not required, just a diagnostic aid
			nodeIndex[nodeIndexCount++] = 0; // not required, just a diagnostic aid

			if (nLeafsDecoded == nLeafsToDecode) {
				// the tree will be fully populated, all nodes saturated.
				// there will be no open indices left to populate.
				// no further processing is required for the tree.
				break;
			}
			// pop upwards on the stack until you find the first node with a
			// non-populated right-side node reference. This may, in fact,
			// be the current node on the stack.
			while (nodeIndex[offset + 2] != 0) {
				stack[iStack] = 0;
				iStack--;
				offset = stack[iStack];
			}
		}
		else {
			// branch node, create a new branch node an push it on the stack
			iStack++;
			if (iStack >= stackSize || nodeIndexCount + 3 >= nodeIndexSize) {
				free(stack);
				free(nodeIndex);
				*errCode = GVRSERR_BAD_COMPRESSION_FORMAT;
				return 0;
			}
			stack[iStack] = nodeIndexCount;
			nodeIndex[nodeIndexCount++] = -1;
			nodeIndex[nodeIndexCount++] = 0; // left node not populated
			nodeIndex[nodeIndexCount++] = 0; // right node not populated
		}
	}

	free(stack);
	*indexSize = nodeIndexCount;
	return nodeIndex;
}

static int doHuff(GvrsBitInput* input, int nSymbols, GvrsByte *output) {
	int i;
	int indexSize;
	int errCode = 0;
	int* nodeIndex;
	nodeIndex = decodeTree(input,  &indexSize, &errCode);
	if (!nodeIndex) {
		return errCode;
	}
 
	if (indexSize == 1) {
		// special case, uniform encoding. 
		memset(output, nodeIndex[0], nSymbols);
	} else {
		for (i = 0; i < nSymbols; i++) {
			// start from the root node at nodeIndex[0]
			// for branch nodes, nodeIndex[offset] will be -1.  when nodeIndex[offset] > -1,
			// the traversal has reached a terminal node and is complete.
			// We know that the root node is always a branch node, so we have a shortcut.
			// We don't have to check to see if nodeIndex[0] == -1 
			int offset = nodeIndex[1 + GvrsBitInputGetBit(input, &errCode)]; // start from the root node
			while (nodeIndex[offset] == -1) {
				offset = nodeIndex[offset + 1 + GvrsBitInputGetBit(input, &errCode)];
			}
			output[i] = (GvrsByte)nodeIndex[offset];
		}
	}
	free(nodeIndex);
	return errCode;
}



static int doInflate(GvrsByte* input, int inputLength, GvrsByte *output, int outputLength, int *inputUsed) {
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



static void cleanUp(GvrsByte* initializerCodes, GvrsByte* interiorCodes, GvrsBitInput* inputBits) {
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


static int decodeInt(int nRows, int nColumns, int packingLength, GvrsByte* packing, GvrsInt* values, void *appInfo) {
	int i, iRow, iCol;
	GvrsByte* initializerCodes = 0;
	GvrsByte* interiorCodes = 0;
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

	GvrsInt seed = unpackInteger(packing, 2);
	int offset = 6;
	GvrsFloat u[12]; // room for up to 12 coefficients
	for (i = 0; i <nCoefficients; i++) {
		u[i] = unpackFloat(packing, offset);
		offset += 4;
	}
	GvrsInt nInitializerCodes = unpackInteger(packing, offset);
	offset += 4;
	GvrsInt nInteriorCodes = unpackInteger(packing, offset);
	offset += 4;
	int method = packing[offset++];
	int compressionType = method&COMPRESSION_TYPE_MASK;
	GvrsBoolean valueChecksumIncluded = (method & VALUE_CHECKSUM_INCLUDED) != 0;
	if (valueChecksumIncluded) {
		// GvrsInt valueChecksum = unpackInteger(packing, offset);
		offset += 4;
	}

	initializerCodes = (GvrsByte *)malloc(nInitializerCodes);
	if (!initializerCodes) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return GVRSERR_NOMEM;
	}
	 interiorCodes = (GvrsByte *)malloc(nInteriorCodes);
	if (!interiorCodes) {
		cleanUp(initializerCodes, interiorCodes, inputBits);
		return GVRSERR_NOMEM;
	}


	int status = 0;
	GvrsByte* inputBytes = packing + offset;
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

	GvrsM32* mInit = GvrsM32Alloc(initializerCodes, nInitializerCodes);

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

	GvrsM32* m32 = GvrsM32Alloc(interiorCodes, nInteriorCodes);

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
