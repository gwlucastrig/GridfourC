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
#include "GvrsCrossPlatform.h"
#include "GvrsError.h"
#include "GvrsCodec.h"


typedef struct huffmanAppInfoTag {
	GvrsInt nDecoded; // both uniform and non-uniform tiles
	GvrsInt nDecodedUniform;
	GvrsLong nBitsInDecodeTree;
	GvrsLong nBitsInDecodeBody;
}huffmanAppInfo;

// TO DO:  test the one-symbol special case.  It is possible that
//         even the baseline Java code is not correctly implemented.


static void cleanUp( GvrsByte* output, GvrsBitInput *input, GvrsM32 *m32, int *nodeIndex) {
	if (output) {
		free(output);
	}
	input = GvrsBitInputFree(input);
	m32 = GvrsM32Free(m32);
	if (nodeIndex) {
		free(nodeIndex);
	}
}

// case-sensitive name of codec
static const char* identification = "GvrsHuffman";
static const char* description = "Implements the standard GVRS compression using Huffman coding";
static GvrsCodec* destroyCodecHuffman(struct GvrsCodecTag* codec) {
	if (codec) {
		if (codec->description) {
			free(codec->description);
			codec->description = 0;
		}
		if (codec->appInfo) {
			free(codec->appInfo);
			codec->appInfo = 0;
		}
		free(codec);
	}
	return 0;
}

 
static GvrsInt* decodeTree(GvrsBitInput* input,  int *indexSize, int *errCode) {
	*errCode = 0;
	int nLeafsToDecode = GvrsBitInputGetByte(input) + 1;
	int rootBit = GvrsBitInputGetBit(input);
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
		nodeIndex[0] =  GvrsBitInputGetByte(input);
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

		int bit = GvrsBitInputGetBit(input);  
		if (bit == 1) {
			if (iStack >= stackSize || nodeIndexCount + 3 >= nodeIndexSize) {
				free(stack);
				free(nodeIndex);
				*errCode = GVRSERR_BAD_COMPRESSION_FORMAT;
				return 0;
			}
			// leaf node
			nLeafsDecoded++;
			nodeIndex[nodeIndexCount++] = GvrsBitInputGetByte(input);   
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

static int decodeInt(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsInt* data, void *appInfo) {
	int i;
	GvrsByte* output = 0;
	GvrsBitInput* input = 0;
	GvrsM32* m32 = 0;
	int* nodeIndex = 0;
	huffmanAppInfo* hInfo = (huffmanAppInfo*)appInfo;

	int compressorIndex = (int)packing[0];
	int predictorIndex = (int)packing[1];
	int seed
		= (packing[2] & 0xff)
		| ((packing[3] & 0xff) << 8)
		| ((packing[4] & 0xff) << 16)
		| ((packing[5] & 0xff) << 24);
	int nM32 = (packing[6] & 0xff)
		| ((packing[7] & 0xff) << 8)
		| ((packing[8] & 0xff) << 16)
		| ((packing[9] & 0xff) << 24);

	input = GvrsBitInputAlloc(packing + 10, (size_t)(packingLength - 10));
	if (!input) {
		cleanUp(output, input, m32, nodeIndex);
		return GVRSERR_NOMEM;
	}
 
	int indexSize;
	int errCode;
   
	nodeIndex = decodeTree(input, &indexSize, &errCode);
	GvrsInt nBitsInTree = GvrsBitInputGetPosition(input);
	if (!nodeIndex) {
		cleanUp(output, input, m32, nodeIndex);
		return errCode;
	}
 
    output = (unsigned char*)malloc(nM32);
	if (!output) {
		cleanUp(output, input, m32, nodeIndex);
		return GVRSERR_NOMEM;
	}

	int pos0 = GvrsBitInputGetPosition(input);
	hInfo->nDecoded++;
	hInfo->nBitsInDecodeTree += pos0;

	int status = 0;
	if (indexSize == 1) {
		hInfo->nDecodedUniform++;
		// special case, uniform encoding.  There may be more than one m32 code, but
		// all the values are the same.
		// TO DO: I also have to review Java code to make sure it's right.
		//        Are M32 codes even involved in this case?
		for (i = 0; i < nM32; i++) {
			output[i] = nodeIndex[0];
		}
        m32 = GvrsM32Alloc(output, nM32);
		if (!m32) {
			cleanUp(output, input, m32, nodeIndex);
			return GVRSERR_NOMEM;
		}
		int value = GvrsM32GetNextSymbol(m32);
		int nCell = nRow * nColumn;
		for (i = 0; i < nCell; i++) {
			data[i] = value;
		}
		cleanUp(output, input, m32, nodeIndex);
		return 0;
	} else {
		for (i = 0; i < nM32; i++) {
			// start from the root node at nodeIndex[0]
			// for branch nodes, nodeIndex[offset] will be -1.  when nodeIndex[offset] > -1,
			// the traversal has reached a terminal node and is complete.
			// We know that the root node is always a branch node, so we have a shortcut.
			// We don't have to check to see if nodeIndex[0] == -1 
			int offset = nodeIndex[1 + GvrsBitInputGetBit(input)]; // start from the root node
			while (nodeIndex[offset] == -1) {
				offset = nodeIndex[offset + 1 + GvrsBitInputGetBit(input)];
			}
			output[i] = (GvrsByte)nodeIndex[offset];
		}

		int pos1 = GvrsBitInputGetPosition(input);
		hInfo->nBitsInDecodeBody += (pos1 - pos0);
		m32 = GvrsM32Alloc(output, nM32);
		if (!m32) {
			cleanUp(output, input, m32, nodeIndex);
			return GVRSERR_NOMEM;
		}


		switch (predictorIndex) {
		case 0:
			status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
			break;
		case 1:
			GvrsPredictor1(nRow, nColumn, seed, m32, data);
			break;
		case 2:
			GvrsPredictor2(nRow, nColumn, seed, m32, data);
			break;
		case 3:
			GvrsPredictor3(nRow, nColumn, seed, m32, data);
			break;
		default:
			// should never happen
			status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
			break;
		}
	}
	cleanUp(output, input, m32, nodeIndex);
	return status;
}

GvrsCodec* GvrsCodecHuffmanAlloc() {
    GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	codec->appInfo = calloc(1, sizeof(huffmanAppInfo));
	if (!codec->appInfo) {
		return 0;
	}

    GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
    codec->description = GVRS_STRDUP(description);
    codec->decodeInt = decodeInt;
	codec->destroyCodec = destroyCodecHuffman;


	return codec;
}
