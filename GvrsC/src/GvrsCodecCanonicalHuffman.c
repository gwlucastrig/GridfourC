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

#define N_LEN_SYMBOLS      19
#define MAX_LEN_STANDARD   15
#define REPEAT_PREV_2BITS  16
#define REPEAT_ZERO_3BITS  17
#define REPEAT_ZERO_7BITS  18

#define N_TXT_SYMBOLS  260
#define N_SYMBOLS_STANDARD 256
#define I_NULL_DATA_CODE   256
#define I_ESCAPE_1BYTE     257
#define I_ESCAPE_2BITS     258
#define I_END_OF_TEXT      259


typedef struct canonicalHuffmanAppInfoTag {
	int32_t nDecoded;
	int64_t nBitsInCodeTable;
	int64_t nBitsInEncodedBody;

	int revision;

}canonicalHuffmanAppInfo;


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



typedef struct {
	int bits;   // the bit sequence generated by the canonical Huffman algorithm
	int xbit;   // the bit sequence in order transmitted by the input-stream.
	int symbol;
	int bitsLength;
}CodeEntry;

typedef struct {
	int* nodeIndex;       // represents the structure of the Huffman tree
	// The "quick-entry" table elements
	int qEntryIndex[256]; // index for entry to the nodeIndex array
	int qSymbol[256];     // the symbol
	int qConsumed[256];   // number of bits for symbol
}CodeTable;


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

static CodeTable* codeTableFree(CodeTable* table) {
	if (table) {
		table->nodeIndex = cleanFree(table->nodeIndex);
		free(table);
	}
	return (CodeTable*)0;
}


static void summarize(FILE* fp, struct GvrsCodecTag* codec) {
	canonicalHuffmanAppInfo* h = (canonicalHuffmanAppInfo*)(codec->appInfo);
	int64_t nbCode = h->nBitsInCodeTable;
	int64_t nbBody = h->nBitsInEncodedBody;
	fprintf(fp, "%s\n", codec->identification);
	double d = h->nDecoded == 0 ? 1 : (double)h->nDecoded;
	fprintf(fp, "  Times decoded:             %8d\n", h->nDecoded);
	fprintf(fp, "  Avg bits for code table:   %10.1f\n", nbCode / d);
	fprintf(fp, "  Avg bits for encoded text: %10.1f\n", nbBody / d);
	fprintf(fp, "  Avg bits total:            %10.1f\n", (nbCode + nbBody) / d);

}


// Build a tree for the specified code lengths.  The last symbol is always the end-of-text symbol.
// So nCodeLengths is the number of unique possible symbols, including the end-of-text symbol.
// Some of the unique symbols may have zero length.
static CodeTable* buildCodeTableFromLengths(int* codeLengths, int nCodeLengths) {
	int n = nCodeLengths * 16;
	int* populated = calloc(n, sizeof(int));
	if (!populated) {
		return (CodeTable*)0;
	}
	int nPopulated = 0;
	for (int i = 0; i < nCodeLengths; i++) {
		if (codeLengths[i] > 0) {
			int index = codeLengths[i] * nCodeLengths + i;
			populated[index] = 1;
			nPopulated++;
		}
	}

	CodeEntry* sortCodes = calloc((size_t)(nPopulated + 1), sizeof(CodeEntry));
	if (!sortCodes) {
		free(populated);
		return (CodeTable*)0;
	}

	// The popluated field is indexed by (codeLength,index) where index refers to
	// the codeLengths array.  The first nCodeLengths entries would refer to 
	// a code-length of zero.  These are never populated (they are just included in memory
	// to simplify arithmetic).  So, we can skip the first nCodeLengths block of populated flags. 
	int kSort = 0;
	for (int i = nCodeLengths; i < n; i++) {
		if (populated[i]) {
			CodeEntry* entry = sortCodes + kSort;
			int index = i / nCodeLengths;
			int symbol = i - index * nCodeLengths;
			int length = codeLengths[symbol];
			entry->bitsLength = length;
			entry->symbol = symbol;
			kSort++;
			if (kSort == nPopulated) {
				break;
			}
		}
	}

	for (int i = 1; i < kSort; i++) {
		CodeEntry* p = sortCodes + (i - 1);  // prior sort code
		CodeEntry* s = sortCodes + i;
		s->bits = p->bits + 1;
		if (s->bitsLength > p->bitsLength) {
			s->bits = s->bits << (s->bitsLength - p->bitsLength);
		}
	}


	populated = cleanFree(populated);

	// The number of nodes is the Huffman tree where k is the number of symbols
	// would be k+(k-1).  Allocate enough nodes to hold the tree
	int nNode = 2 * kSort;
	CodeTable* codeTable = calloc(1, sizeof(CodeTable));
	int* nodeIndex = calloc((size_t)(2 * nNode + 2), sizeof(int));
	if (!codeTable || !nodeIndex) {
		codeTable = failFree(codeTable);
		nodeIndex = failFree(nodeIndex);
		free(sortCodes);
		return (CodeTable*)0;
	}

	codeTable->nodeIndex = nodeIndex;

	// populate the node index
	int kIndex = 2;
	for (int i = 0; i < kSort; i++) {
		CodeEntry* s = sortCodes + i;
		int offset = 0;
		int xmit = 0;
		int nodeEntryIndex = 0;
		for (int j = 0; j < s->bitsLength; j++) {
			int bit = (s->bits >> (s->bitsLength - 1 - j)) & 1;
			xmit |= (bit << j);
			int index = offset + bit;
			if (nodeIndex[index]) {
				offset = nodeIndex[index];
			}
			else {
				nodeIndex[index] = kIndex;
				offset = kIndex;
				kIndex += 2;
			}
			if (j == 7) {
				nodeEntryIndex = offset;
			}
		}
		// the path to the symbol is now established,
		// the offset is pointing to the position of the terminal node
		// nodeIndex[offset] will be zero, indicating a terminal node
		nodeIndex[offset + 1] = s->symbol;

		// Populate the quick-entry elements for this symbol
		// If the entire symbol can be specified by the quick-entry table
		// then the bit-consumption value for quick-entry will be the symbol length
		// If the symbol's bit length is greater than 8, then we limit it.  Also,
		// the quick entry will provide an index into the nodeIndex array rather
		// than a symbol.  As a development diagnostic, note that if s->bitLength
		// is greater than 8, then nodeIndex will be non-zero.
		int nConsumed = s->bitsLength > 8 ? 8 : s->bitsLength;
		int nInterval = 1 << nConsumed;
		int test = xmit & 0xff;
		for (int j = test; j < 256; j += nInterval) {
			if (nodeEntryIndex) {
				codeTable->qEntryIndex[j] = nodeEntryIndex;
			}
			else {
				codeTable->qSymbol[j] = s->symbol;
			}
			codeTable->qConsumed[j] = nConsumed;
		}
	}

	free(sortCodes);
	return codeTable;
}

static CodeTable* decodeCodeTable(canonicalHuffmanAppInfo* hInfo, GvrsBitInput* input) {
	int codeLengths[N_LEN_SYMBOLS + 1];
	int k = 0;
	int prior = 0;
	int n;
	// loop on the number of symbols, plus one for the end-of-text symbol
	while (k < N_LEN_SYMBOLS + 1) {
		int index = GvrsBitInputGetBits(input, 5);
		if (index <= MAX_LEN_STANDARD) {
			prior = index;
			codeLengths[k++] = index;
		}
		else {
			switch (index) {
			case REPEAT_PREV_2BITS:
				n = GvrsBitInputGetBits(input, 2) + 3;
				for (int i = 0; i < n; i++) {
					codeLengths[k++] = prior;
				}
				break;
			case REPEAT_ZERO_3BITS:
				prior = 0;
				n = GvrsBitInputGetBits(input, 3) + 3;
				for (int i = 0; i < n; i++) {
					codeLengths[k++] = 0;
				}
				break;
			case REPEAT_ZERO_7BITS:
				prior = 0;
				n = GvrsBitInputGetBits(input, 7) + 11;
				for (int i = 0; i < n; i++) {
					codeLengths[k++] = 0;
				}
				break;
			default:
				break;
			}
		}
	}

	CodeTable* countTable = buildCodeTableFromLengths(codeLengths, N_LEN_SYMBOLS + 1);
	if (!countTable) {
		// the was an internal error or malloc failure
		return 0;
	}

	int* nodeIndex = countTable->nodeIndex;
	prior = 0;
	k = 0;
	int textLengths[N_TXT_SYMBOLS];
	while (k < N_TXT_SYMBOLS) {

		int offset = 0;
		while (nodeIndex[offset]) {
			int bit = GvrsBitInputGetBit(input);
			offset = nodeIndex[offset + bit];
		}
		int index = nodeIndex[offset + 1];
		if (index <= MAX_LEN_STANDARD) {
			prior = index;
			textLengths[k++] = index;
		}
		else {
			switch (index) {
			case REPEAT_PREV_2BITS:
				n = GvrsBitInputGetBits(input, 2) + 3;
				for (int i = 0; i < n; i++) {
					textLengths[k++] = prior;
				}
				break;
			case REPEAT_ZERO_3BITS:
				prior = 0;
				n = GvrsBitInputGetBits(input, 3) + 3;
				for (int i = 0; i < n; i++) {
					textLengths[k++] = 0;
				}
				break;
			case REPEAT_ZERO_7BITS:
				prior = 0;
				n = GvrsBitInputGetBits(input, 7) + 11;
				for (int i = 0; i < n; i++) {
					textLengths[k++] = 0;
				}
				break;
			default:
				break;
			}
		}
	}
	codeTableFree(countTable);
	return buildCodeTableFromLengths(textLengths, N_TXT_SYMBOLS);
}

static int decodeInt(int nRow, int nColumn, int packingLength, uint8_t* packing, int32_t* data, void* appInfo) {
	int errCode = 0;
	canonicalHuffmanAppInfo* hInfo = (canonicalHuffmanAppInfo*)appInfo;
	int nSymbolsInText = nRow * nColumn;

	hInfo->nDecoded++;

	// int compressorIndex = (int)packing[0];
	int predictorIndex = (int)packing[1];
	int seed
		= (packing[2] & 0xff)
		| ((packing[3] & 0xff) << 8)
		| ((packing[4] & 0xff) << 16)
		| ((packing[5] & 0xff) << 24);

	int32_t* text = calloc(nSymbolsInText, sizeof(int32_t));
	if (!text) {
		return GVRSERR_NOMEM;
	}

	GvrsBitInput* input = GvrsBitInputAlloc(packing + 6, (size_t)(packingLength - 6), &errCode);
	if (!input) {
		free(text);
		return GVRSERR_NOMEM;
	}
	int pos0 = GvrsBitInputGetPosition(input);

	hInfo->revision = GvrsBitInputGetBit(input);

	CodeTable* codeTable = decodeCodeTable(hInfo, input);
	if (!codeTable) {
		free(text);
		GvrsBitInputFree(input);
		return GVRSERR_COMPRESSION_FAILURE;
	}
	int pos1 = GvrsBitInputGetPosition(input);
	hInfo->nBitsInCodeTable += (int64_t)(pos1 - pos0);

	int* nodeIndex = codeTable->nodeIndex;
	int prior = 0;
	int iSymbol = 0;
	uint8_t* source = input->text;
	unsigned int scratch = input->scratch;
	int iSource = input->nBytesProcessed;
	int nBit = input->nBit;
	int nSource = input->nBytesInText;
	int n;
	unsigned int bit, bits;
	while (iSymbol < nSymbolsInText) {
		if (nBit < 8) {
			// This is the only case where the loop might try to claim more bits
			// than stored in the bit source.  So we need to test.  If the logic
			// requests more than the available bits, it is okay to allow them
			// to go to zero.
			if (iSource < nSource) {
				scratch |= (source[iSource++] << nBit);
			}
			nBit += 8;
		}
		int test = scratch & 0xff;
		n = codeTable->qConsumed[test];
		scratch >>= n;
		nBit -= n;
		int symbol = codeTable->qSymbol[test];;
		int offset = codeTable->qEntryIndex[test];
		if (offset) {
			while (nodeIndex[offset]) {
				// int bit = GvrsBitInputGetBit(input); -------------------------------
				if (nBit == 0) {
					scratch = source[iSource++];
					nBit = 8;
				}
				bit = scratch & 0x01u;
				scratch >>= 1;
				nBit--;
				// end of GetBit() ----------------------------------------------------
				offset = nodeIndex[offset + bit];
			}
			symbol = nodeIndex[offset + 1];
		}

		if (symbol < N_SYMBOLS_STANDARD) {
			symbol -= 128;
			text[iSymbol++] = symbol;
			prior = symbol;
		}
		else {
			switch (symbol) {
			case I_ESCAPE_2BITS:
				// bits = GvrsBitInputGetBits(input, 2);  ------------------------------
				if (nBit < 2) {
					scratch |= (source[iSource++] << nBit);
					nBit += 8;
				}
				bits = scratch & 0x03u;
				scratch >>= 2;
				nBit -= 2;

				// end of GetBits(2) --------------------------------------------
				prior = (prior << 2) | bits;
				text[iSymbol - 1] = prior;
				break;
			case I_ESCAPE_1BYTE:
				// bits = GvrsBitInputGetByte(input);  ---------------------------------
				if (nBit < 8) {
					scratch |= (source[iSource++] << nBit);
					nBit += 8;
				}
				bits = scratch & 0xffu;
				scratch >>= 8;
				nBit -= 8;
				// end of GetByte -------------------------------------------------
				prior = (prior << 8) | bits;
				text[iSymbol - 1] = prior;
				break;
			case I_NULL_DATA_CODE:
				prior = 0x80000000;
				text[iSymbol++] = prior;
				break;
			case I_END_OF_TEXT:
				iSymbol = nSymbolsInText;  // causes exit from the loop
				break;
			default:
				free(text);
				GvrsBitInputFree(input);
				return GVRSERR_BAD_COMPRESSION_FORMAT;
			}
		}
	}

	GvrsBitInputSetState(input, iSource, nBit, scratch);

	int pos2 = GvrsBitInputGetPosition(input);
	hInfo->nBitsInEncodedBody += (int64_t)(pos2 - pos1);

	int status = 0;
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

	codeTableFree(codeTable);
	GvrsBitInputFree(input);
	free(text);


	return status;
}

int
GvrsCanonicalHuffmanDecode(GvrsBitInput* input, int nSymbolsInText, int* text, void* appInfo) {
	canonicalHuffmanAppInfo* hInfo;
	if (appInfo) {
		hInfo = appInfo;
	}
	else {
		canonicalHuffmanAppInfo hInfoLocal;
		memset(&hInfoLocal, 0, sizeof(hInfoLocal));
		hInfo = &hInfoLocal;
	}
	int pos0 = GvrsBitInputGetPosition(input);

	hInfo->revision = GvrsBitInputGetBit(input);

	CodeTable* codeTable = decodeCodeTable(hInfo, input);
	if (!codeTable) {
		return GVRSERR_COMPRESSION_FAILURE;
	}
	int pos1 = GvrsBitInputGetPosition(input);
	hInfo->nBitsInCodeTable += (int64_t)(pos1 - pos0);


	int* nodeIndex = codeTable->nodeIndex;
	int prior = 0;
	int iSymbol = 0;
	uint8_t* source = input->text;
	unsigned int scratch = input->scratch;
	int iSource = input->nBytesProcessed;
	int nBit = input->nBit;
	int nSource = input->nBytesInText;
	int n;
	unsigned int bit, bits;

	while (iSymbol < nSymbolsInText) {

		if (nBit < 8) {
			// This is the only case where the loop might try to claim more bits
			// than stored in the bit source.  So we need to test.  If the logic
			// requests more than the available bits, it is okay to allow them
			// to go to zero.
			if (iSource < nSource) {
				scratch |= (source[iSource++] << nBit);
			}
			nBit += 8;
		}
		int test = scratch & 0xff;
		n = codeTable->qConsumed[test];
		scratch >>= n;
		nBit -= n;
		int symbol = codeTable->qSymbol[test];;
		int offset = codeTable->qEntryIndex[test];
		if (offset) {
			while (nodeIndex[offset]) {
				// int bit = GvrsBitInputGetBit(input); -------------------------------
				if (nBit == 0) {
					scratch = source[iSource++];
					nBit = 8;
				}
				bit = scratch & 0x01u;
				scratch >>= 1;
				nBit--;
				// end of GetBit() ----------------------------------------------------
				offset = nodeIndex[offset + bit];
			}
			symbol = nodeIndex[offset + 1];
		}



		if (symbol < N_SYMBOLS_STANDARD) {
			symbol -= 128;
			text[iSymbol++] = symbol;
			prior = symbol;
		}
		else {
			switch (symbol) {
			case I_ESCAPE_2BITS:
				// bits = GvrsBitInputGetBits(input, 2);  ------------------------------
				if (nBit < 2) {
					scratch |= (source[iSource++] << nBit);
					nBit += 8;
				}
				bits = scratch & 0x03u;
				scratch >>= 2;
				nBit -= 2;

				// end of GetBits(2) --------------------------------------------
				prior = (prior << 2) | bits;
				text[iSymbol - 1] = prior;
				break;
			case I_ESCAPE_1BYTE:
				// bits = GvrsBitInputGetByte(input);  ---------------------------------
				if (nBit < 8) {
					scratch |= (source[iSource++] << nBit);
					nBit += 8;
				}
				bits = scratch & 0xffu;
				scratch >>= 8;
				nBit -= 8;
				// end of GetByte -------------------------------------------------
				prior = (prior << 8) | bits;
				text[iSymbol - 1] = prior;
				break;
			case I_NULL_DATA_CODE:
				prior = 0x80000000;
				text[iSymbol++] = prior;
				break;
			case I_END_OF_TEXT:
				iSymbol = nSymbolsInText;  // causes exit from the loop
				break;
			default:
				free(text);
				GvrsBitInputFree(input);
				return GVRSERR_BAD_COMPRESSION_FORMAT;
			}
		}
	}
	GvrsBitInputSetState(input, iSource, nBit, scratch);
	codeTableFree(codeTable);
	return 0;
}




GvrsCodec* GvrsCodecCanonicalHuffmanAlloc() {
	GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	codec->appInfo = calloc(1, sizeof(canonicalHuffmanAppInfo));
	if (!codec->appInfo) {
		return 0;
	}

	GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
	codec->description = GVRS_STRDUP(description);
	codec->decodeInt = decodeInt;
	//codec->encodeInt = encodeInt;
	codec->destroyCodec = destroyCodecCanonicalHuffman;
	codec->allocateNewCodec = allocateCodecCanonicalHuffman;
	codec->summarize = summarize;
	return codec;
}

