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


//  About the nodeIndex array:
//  Early versions of this code used a tree based structure with pointers to
//  the child nodes for each branch node.  But testing indicated that the overhead
//  for navigating the tree had a strong impact on performance.  As an alternate
//  we use a nodeIndex array to represent the virtual structure for the Huffman code:
//    1. Given a Huffman tree with nNode nodes (including branches and terminal nodes),
//       allocate nodeIndex = int[2*nNode].  
//    2. Each node is indicated by a pair of adjacent array entries.  So the ith node will
//       be located at nodeIndex[i*2] and nodeIndex[i*2+1].
//    3. For branch nodes,  nodeIndex[i*2] indicates the left-side child of the branch
//       (the branch associated with bit value of 0) and node[index[i*2+1] the right
//       (the branch associated with bit value of 1).
//    4. For symbol (terminal) nodes, nodeIndex[i*2] is set to zero.  NodeIndex[i*2+1] is
//       set to the value of the symbol (which may be any integer, including zero and negative values).
//  
//  About the quick-entry table:
//  The quick-entry table provides us with a way of avoiding, or shortening, the traversal
//  of the nodeIndex array.  As the code loops through the encoded symbols in the input bit stream,
//  it extracts 8 bits at the top of the loop.  These bits give the code an index in the range
//  0 to 255 for looking up an entry in the quick-entry table.  
//     1. For symbols with an encoded bit length of 8 or less, the quick-entry table
//        gives a value for the symbol.  
//     2. For symbols with an encoded bit length greater than 8, the quick-entry table
//        gives an index into the nodeIndex array.
//     3. The quick-entry table tells the code how many bits to "consume" for the symbol
//        (1 to 8) or the transition to nodeIndex (all 8).
//  The quick-entry table is designed so that the bit-sequence for a symbol becomes the
//  low-order bits for an array index into the table elements.  For example, a symbol 
//  with bit sequence 110 (length 3) becomes array index of 6.  When the code extracts
//  8 bits from the encoded bit-stream, the code does not actually know how many of those
//  bits may apply to the symbol.  To resolve those 8 bits to either a symbol or a nodeIndex offset,
//  a symbol with a short bit sequence would appear in the quick-entry table multiple times.
//  This approach allows the quick-entry table to match any bit sequence that would include the
//  symbol.  Using the example above, the code would store the data for a symbol at the
//  following array indices:
//        00000 110  (array index 6)
//        00001 110  (array index 6+8 = 14)
//        00010 110  (array index 6+8+8 = 22)
//        00011 110  (array index 6+8+8+8 = 30)
//  Note that the step increment for each entry is simply 2 raised to the power of the bit length.
//  In this case, a bit length of 3 leads to a step increment of 8.
//  
//  

#include "GvrsFramework.h"

#include "GvrsCrossPlatform.h"
#include "GvrsError.h"
#include "GvrsCodec.h"
#include "GvrsCanonicalHuffman.h"
 

// case-sensitive name of codec
static const char* identification = "GvrsCanonicalHuffman";
static const char* description = "Implements the standard GVRS compression using canonical Huffman coding";
static GvrsCodec* destroyCodecCanonicalHuffman(struct GvrsCodecTag* codec) {
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

static GvrsCodec* allocateCodecCanonicalHuffman(struct GvrsCodecTag* codec) {
	return GvrsCodecCanonicalHuffmanAlloc();
}


// a utility for freeing elements during an allocation failure.
// if everything is implemented correctly, this function should never be used.
static void* failFree(void* f) {
	if (f) {
		free(f);
	}
	return (void*)0;
}

static void* cleanFree(void* f) {
	if (f) {
		free(f);
	}
	return (void*)0;
}

 


static void summarize(FILE* fp, struct GvrsCodecTag* codec) {
	GvrsCanonicalHuffmanAppInfo* h = (GvrsCanonicalHuffmanAppInfo*)(codec->appInfo);
	int64_t nbCode = h->nBitsInCodeTable;
	int64_t nbBody = h->nBitsInEncodedBody;
	fprintf(fp, "%s\n", codec->identification);
	double d = h->nDecoded == 0 ? 1 : (double)h->nDecoded;
	fprintf(fp, "  Times decoded:             %8d\n", h->nDecoded);
	fprintf(fp, "  Avg bits for code table:   %10.1f\n", nbCode / d);
	fprintf(fp, "  Avg bits for encoded text: %10.1f\n", nbBody / d);
	fprintf(fp, "  Avg bits total:            %10.1f\n", (nbCode + nbBody) / d);

}
 

static int decodeInt(int nRow, int nColumn, int packingLength, uint8_t* packing, int32_t* data, void* appInfo) {
	int errCode = 0;
	GvrsCanonicalHuffmanAppInfo* hInfo = (GvrsCanonicalHuffmanAppInfo*)appInfo;
	int nCellsInTile = nRow * nColumn;
	int nSymbolsInText = nCellsInTile - 1;

	hInfo->nDecoded++;

	// int compressorIndex = (int)packing[0];
	int predictorIndex = (int)packing[1];
	int seed
		= (packing[2] & 0xff)
		| ((packing[3] & 0xff) << 8)
		| ((packing[4] & 0xff) << 16)
		| ((packing[5] & 0xff) << 24);

	// special case: check to see if there is a uniform encoding
	if (predictorIndex == 0 && packingLength==6) {
		for (int i = 0; i < nCellsInTile; i++) {
			data[i] = seed;
		}
		return 0;
	}

	int32_t* text = calloc(nCellsInTile, sizeof(int32_t));
	if (!text) {
		return GVRSERR_NOMEM;
	}

	GvrsBitInput* input = GvrsBitInputAlloc(packing + 6, (size_t)(packingLength - 6), &errCode);
	if (!input) {
		free(text);
		return GVRSERR_NOMEM;
	}

	int status = GvrsCanonicalHuffmanReadInt(input, nSymbolsInText, text, appInfo);
	if (status<0) {
		GvrsBitInputFree(input);
		free(text);
		return status;
	}
	
	 
	status = 0;
	switch (predictorIndex) {
	case 0:
		status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
		break;
	case 1:
		GvrsPredictor1i(nRow, nColumn, seed, text, data);
		break;
	case 2:
		GvrsPredictor2i(nRow, nColumn, seed, text, data);
		break;
	case 3:
		GvrsPredictor3i(nRow, nColumn, seed, text, data);
		break;
	default:
		// should never happen
		status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
		break;
	}

	GvrsBitInputFree(input);
	free(text);

	return status;
}
 


static int encodeInt(
	int nRow,
	int nColumn,
	int32_t* data,
	int index,
	int* packingLengthReference,
	uint8_t** packingReference,
	void* appInfo) {
	if (!data || !packingLengthReference || !packingReference) {
		return GVRSERR_NULL_ARGUMENT;
	}

	*packingLengthReference = 0;
	*packingReference = 0;

	int packingLength = 0;
	uint8_t* packing = 0;
	int32_t seed = data[0];

	int nCells = nRow * nColumn;
	int n;
 
	int uniformValue = 1;
	for (int i = 1; i < nCells; i++) {
		if (data[i] != seed) {
			uniformValue = 0;
			break;
		}
	}

	if (uniformValue) {
		uint8_t* h = calloc(6, sizeof(uint8_t));
		if (!h) {
			return GVRSERR_NOMEM;
		}
		h[0] = (uint8_t)index;
		h[1] = (uint8_t)(0);
		h[2] = (uint8_t)(seed & 0xff);
		h[3] = (uint8_t)((seed >> 8) & 0xff);
		h[4] = (uint8_t)((seed >> 16) & 0xff);
		h[5] = (uint8_t)((seed >> 24) & 0xff);
		*packingReference = h;
		*packingLengthReference = 6;
		return 0;
	}

	int status;
	for (int iPack = 1; iPack <= 3; iPack++) {
		int* residuals = calloc(nCells, sizeof(int));
		int nResiduals;
		if (iPack == 1) {
			nResiduals = GvrsPredictor1iEncode(nRow, nColumn, data, &seed, residuals);
		}
	    else if (iPack == 2) {
			nResiduals = GvrsPredictor2iEncode(nRow, nColumn, data, &seed, residuals);
		}
		else {
			nResiduals = GvrsPredictor3iEncode(nRow, nColumn, data, &seed, residuals);
		}
		 

		GvrsBitOutput* bitOutput;
		status = GvrsBitOutputAlloc(&bitOutput);
		if (status) {
			if (packing) {
				free(packing);
			}
			return status;
		}


		uint8_t* h;
		status = GvrsBitOutputReserveBytes(bitOutput, 6, &h);
		if (status) {
			if (packing) {
				free(packing);
				return status;
			}
		}
		h[0] = (uint8_t)index;
		h[1] = (uint8_t)(iPack);
		h[2] = (uint8_t)(seed & 0xff);
		h[3] = (uint8_t)((seed >> 8) & 0xff);
		h[4] = (uint8_t)((seed >> 16) & 0xff);
		h[5] = (uint8_t)((seed >> 24) & 0xff);
		 

		status = GvrsCanonicalHuffmanWriteInt(nResiduals, residuals, bitOutput);
		free(residuals);
			 
		if (status) {
			bitOutput = GvrsBitOutputFree(bitOutput);
			if (packing) {
				free(packing);
			}
			return status;
		}

		// If the packing is defined, we will only accept the newer results if the output text size
		// is smaller than the previous results.
		if (packing) {
		    n = (GvrsBitOutputGetBitCount(bitOutput) + 7) / 8;
			if (n >= packingLength) {
				bitOutput = GvrsBitOutputFree(bitOutput);
				continue;
			}
			else {
				// The results will be smaller than the previous packing.
				// Free the previous packing in preparation of replacement with the newer results.
				free(packing);
				packing = 0;
				packingLength = 0;
			}
		}

		uint8_t* b = 0;;
		int bLen = 0;
		status = GvrsBitOutputGetText(bitOutput, &bLen, &b);
		bitOutput = GvrsBitOutputFree(bitOutput);
		if (status) {
			return status;
		}

		packing = b;
		packingLength = bLen;

	}

	*packingReference = packing;
	*packingLengthReference = packingLength;
	return 0;
}


GvrsCodec* GvrsCodecCanonicalHuffmanAlloc() {
	GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	codec->appInfo = calloc(1, sizeof(GvrsCanonicalHuffmanAppInfo));
	if (!codec->appInfo) {
		return 0;
	}

	GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
	codec->description = GVRS_STRDUP(description);
	codec->decodeInt = decodeInt;
	codec->encodeInt = encodeInt;
	codec->destroyCodec = destroyCodecCanonicalHuffman;
	codec->allocateNewCodec = allocateCodecCanonicalHuffman;
	codec->summarize = summarize;
	return codec;
}
