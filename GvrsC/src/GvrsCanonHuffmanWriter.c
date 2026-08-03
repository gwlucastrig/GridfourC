#include "GvrsError.h"
#include "GvrsCanonicalHuffman.h"


static GvrsHuffmanBuild* GvrsHuffmanBuildAlloc(int symbolSetSize) {
	GvrsHuffmanBuild* b = calloc(1, sizeof(GvrsHuffmanBuild));
	if (!b) {
		return b;  // allocation failure
	}
	b->symbolSetSize = symbolSetSize;
	int nNodes = symbolSetSize * 2; // n*2-1 is enough
	b->nodes = calloc(nNodes, sizeof(GvrsSymbolNode));
	if (!b->nodes) {
		// allocation failure
		return GvrsHuffmanBuildFree(b);
	}
	b->symbolSet = calloc(symbolSetSize, sizeof(GvrsHuffmanSymbol));
	if (!b->symbolSet) {
		return GvrsHuffmanBuildFree(b);
	}

	for (int i = 0; i < symbolSetSize; i++) {
		b->nodes[i].symbol = i;
		b->symbolSet[i].symbol = i;
	}
	b->nNodesAllocated = symbolSetSize;

	b->queue = calloc(symbolSetSize, sizeof(GvrsSymbolNode*));
	if (!b->queue) {
		return GvrsHuffmanBuildFree(b);
	}
	return b;
}

GvrsHuffmanBuild* GvrsHuffmanBuildFree(GvrsHuffmanBuild* build) {
	if (build) {
		if (build->nodes) {
			free(build->nodes);
			build->nodes = 0;
		}
		if (build->queue) {
			free(build->queue);
			build->queue = 0;
		}
		if (build->symbolSet) {
			free(build->symbolSet);
			build->symbolSet = 0;
		}
		free(build);
	}
	return 0;
}


// The sort in preparation of building the Huffman tree.
static int gvrsCanonicalSymbolComp(const void* a, const void* b) {
	const GvrsHuffmanSymbol* aP = (const GvrsHuffmanSymbol*)a;
	const GvrsHuffmanSymbol* bP = (const GvrsHuffmanSymbol*)b;

	int x = aP->nBitsInCode - bP->nBitsInCode;
	if (x == 0) {
		return aP->symbol - bP->symbol;
	}
	return x;
}

static void gvrsHuffmanBuildPrepSymbolOutput(GvrsHuffmanBuild* b) {
	for (int i = 0; i < b->symbolSetSize; i++) {
		if (b->symbolSet[i].count > 0) {
			GvrsHuffmanSymbol* s = b->symbolSet + i;
			int output = 0;
			int bits = s->code;
			int nBit = s->nBitsInCode;
			for (int j = 0; j < nBit; j++) {
				output |= (((bits >> j) & 0x01) << (nBit - 1 - j));
			}
			s->output = output;
		}
	}
}
// Count symbols based om the full-range integer system
static void GvrsHuffmanBuildCountSymbolsInt(GvrsHuffmanBuild* b, int nSymbolsInText, int* text) {
	int i, symbol, index;
	b->nSymbolsInText = nSymbolsInText;  // mainly a diagnostic
	GvrsSymbolNode* nodes = b->nodes;

	for (i = 0; i < nSymbolsInText; i++) {
		symbol = text[i];
		if (-128 <= symbol && symbol <= 127) {
			// the symbol is in the range of standard (one byte) symbols
			index = symbol + 128;
			nodes[index].count++;
		}
		else if (-512 <= symbol && symbol <= 511) {
			// b->count 8+2 bit symbol with one 2-bit b->escape sequence
			int target = (symbol >> 2) + 128;
			b->escapeCountBits2++;
			nodes[I_ESCAPE_2BITS].count++;
			nodes[target].count++;
			b->count2Bit[(symbol >> 2) & 0x03]++;
		}
		else if (-2048 <= symbol && symbol <= 2047) {
			// b->count 8+4 bit symbol as 2 sets of 2-bit b->escape sequences
			int target = (symbol >> 4) + 128;
			b->escapeCountBits4++;
			nodes[I_ESCAPE_2BITS].count += 2;
			nodes[target].count++;
			b->count2Bit[(symbol >> 2) & 0x03]++;
			b->count2Bit[symbol & 0x03]++;
		}
		else if (-8192 <= symbol && symbol <= 8191) {
			// b->count 8+6 bit symbol as 3 sets of 2-bit b->escape sequences
			int target = (symbol >> 6) + 128;
			b->escapeCountBits6++;
			nodes[I_ESCAPE_2BITS].count += 3;
			nodes[target].count++;
			b->count2Bit[(symbol >> 4) & 0x03]++;
			b->count2Bit[(symbol >> 2) & 0x03]++;
			b->count2Bit[symbol & 0x03]++;
		}
		else if (-32768 <= symbol && symbol <= 32767) {
			// b->count 8+8 bit symbol with one 8-bit b->escape sequence
			int target = (symbol >> 8) + 128;
			b->escapeCountBits8++;
			nodes[I_ESCAPE_1BYTE].count++;
			nodes[target].count++;
			b->count8Bit[(symbol >> 8) & 0xff]++;
		}
		else if (symbol == INT_MIN) {
			nodes[I_NULL_DATA_CODE].count++;
		}
		else if (-8388608 <= symbol && symbol <= 8388607) {
			int target = (symbol >> 16) + 128;
			b->escapeCountBits16++;
			nodes[I_ESCAPE_1BYTE].count += 2;
			nodes[target].count++;
			b->count8Bit[(symbol >> 8) & 0xff]++;
			b->count8Bit[(symbol >> 16) & 0xff]++;
		}
		else {
			int target = (symbol >> 24) + 128;
			b->escapeCountBits24++;
			nodes[I_ESCAPE_1BYTE].count += 3;
			nodes[target].count++;
			b->count8Bit[(symbol >> 8) & 0xff]++;
			b->count8Bit[(symbol >> 16) & 0xff]++;
			b->count8Bit[(symbol >> 24) & 0xff]++;
		}
	}
	nodes[b->symbolSetSize - 1].count = 1;  // end-of=text symbol

	int k = 0;
	for (i = 0; i < b->symbolSetSize; i++) {
		if (nodes[i].count > 0) {
			b->queue[k++] = nodes + i;
		}
	}
	b->nUniqueSymbols = k; // this includes the end-of-text symbol
	b->uniformValueText = (k == 2);
}


// Count symbols based om the full-range byte system, no end-of-text
static void GvrsHuffmanBuildCountSymbolsByte(GvrsHuffmanBuild* b, int nSymbolsInText, uint8_t* text) {
	int i, symbol;
	b->nSymbolsInText = nSymbolsInText;  // mainly a diagnostic
	GvrsSymbolNode* nodes = b->nodes;

	for (i = 0; i < nSymbolsInText; i++) {
		symbol = (int)text[i];
		nodes[symbol].count++;
	}

	int k = 0;
	for (i = 0; i < b->symbolSetSize; i++) {
		if (nodes[i].count > 0) {
			b->queue[k++] = nodes + i;
		}
	}
	b->nUniqueSymbols = k; // unlike the integer case, this does not include the end-of-text symbol
	b->uniformValueText = (k == 1);
}


typedef struct gvrsPackMergeEntry {
	int symbol;
	int count;
}gvrsPackMergeEntry;


// The sort in preparation of building the Huffman tree.
static int gvrsPackMergeComp(const void* a, const void* b) {
	const gvrsPackMergeEntry* aP = (const gvrsPackMergeEntry*)a;
	const gvrsPackMergeEntry* bP = (const gvrsPackMergeEntry*)b;

	int test = aP->count - bP->count;
	if (test == 0) {
		return aP->symbol - bP->symbol;
	}
	return test;
}

static int cleanupPackageMergeFail(gvrsPackMergeEntry* entries[]) {
	for (int i = 0; i < I_N_SYMBOLS; i++) {
		if (entries[i]) {
			free(entries[i]);
			entries[i] = 0;
	    }
	}
	return GVRSERR_NOMEM;
}

static int gvrsHuffmanBuildPackageMerge(int maxCodeLength, GvrsHuffmanBuild* b, int nInput, GvrsHuffmanSymbol* input) {

	int nEntry[I_N_SYMBOLS];
	int nBits[I_N_SYMBOLS];
	memset(nEntry, 0, sizeof(nEntry));
	memset(nBits, 0, sizeof(nBits));
	gvrsPackMergeEntry* base = calloc(b->nUniqueSymbols, sizeof(gvrsPackMergeEntry));
	if (!base) {
		return GVRSERR_NOMEM;
	}
	int nBase = 0;
	for (int i = 0; i < b->symbolSetSize; i++) {
		if (b->nodes[i].count > 0) {
			base[nBase].symbol = i;
			base[nBase].count = b->nodes[i].count;
			nBase++;
		}
	}
	nEntry[0] = nBase;
	qsort(base, nBase, sizeof(gvrsPackMergeEntry), gvrsPackMergeComp);
	gvrsPackMergeEntry* entries[I_N_SYMBOLS];
	memset(entries, 0, sizeof(entries));
	entries[0] = base;


	for (int iDepth = 1; iDepth < maxCodeLength; iDepth++) {
		gvrsPackMergeEntry* ix = entries[iDepth - 1];
		int nPair = nEntry[iDepth - 1] / 2;
		gvrsPackMergeEntry* pair = calloc(nPair, sizeof(gvrsPackMergeEntry)); //  new gvrsPackMergeEntry[nPair];
		if (!pair) {
			return cleanupPackageMergeFail(entries);
		}
		for (int iPair = 0; iPair < nPair; iPair++) {
			int count = ix[iPair * 2].count + ix[iPair * 2 + 1].count;
			pair[iPair].count = count;
			pair[iPair].symbol = -1;
		}

		int k = 0;
		int iBase = 0;
		int nM = nBase + nPair;
		gvrsPackMergeEntry* m = calloc(nM, sizeof(gvrsPackMergeEntry));
		if (!m) {
		    free(pair);
			return cleanupPackageMergeFail(entries);
		}
		for (int iPair = 0; iPair < nPair; iPair++) {
			while (iBase < nBase) {
				if (base[iBase].count <= pair[iPair].count) {
					m[k++] = base[iBase];
					iBase++;
				}
				else {
					break;
				}
			}
			m[k++] = pair[iPair];
		}
		if (base[nBase - 1].count > pair[nPair - 1].count) {
			m[nM - 1] = base[nBase - 1];
		}
		entries[iDepth] = m;
		nEntry[iDepth] = nM;
		free(pair);
	}

	// Phase 2 -------------------------------------------
	// Tabulate the package-merge entries, going from bottom to top.
	int n = nBase * 2 - 2;
	for (int iEntry = maxCodeLength - 1; iEntry >= 0; iEntry--) {
		int nMerged = 0;
		gvrsPackMergeEntry* ix = entries[iEntry];
		for (int i = 0; i < n; i++) {
			int index = ix[i].symbol;
			if (index == -1) {
				// it's a pair
				nMerged++;
			}
			else {
				nBits[index]++;
			}
		}
		n = nMerged * 2;
	}

	// Phase 3 --------------------
	// Transfer the code lengths to the input.
	for (int i = 0; i < nBase; i++) {
		gvrsPackMergeEntry e = base[i];
		int index = e.symbol;
		input[e.symbol].nBitsInCode = nBits[index];
	}

	// Clean up -------------------------
	for (int i = 0; i < maxCodeLength; i++) {
		free(entries[i]);
	}
	return 0;
}

static void gvrsHuffmanBuildCanonicalCodes(GvrsHuffmanBuild* b) {
	GvrsHuffmanSymbol sortCodes[I_N_SYMBOLS];
	memset(sortCodes, 0, sizeof(sortCodes));
	int nSymbol = 0;
	for (int i = 0; i < b->symbolSetSize; i++) {
		if (b->symbolSet[i].count > 0) {
			GvrsHuffmanSymbol* s = b->symbolSet + i;  // source
			GvrsHuffmanSymbol* d = sortCodes + nSymbol; // destination
			d->symbol = s->symbol;
			d->count = s->count;
			d->nBitsInCode = s->nBitsInCode;
			nSymbol++;
		}
	}
	// nSymbol should now equal b->nUniqueSymbols
	qsort(sortCodes, nSymbol, sizeof(GvrsHuffmanSymbol), gvrsCanonicalSymbolComp);
	int code = 0; // sortCodes[0].code already initiailized to zero
	b->symbolSet[sortCodes[0].symbol] = sortCodes[0];
	for (int i = 1; i < nSymbol; i++) {
		code++;
		int n = sortCodes[i].nBitsInCode - sortCodes[i - 1].nBitsInCode;
		if (n > 0) {
			code = code << n;
		}
		sortCodes[i].code = code;
		int index = sortCodes[i].symbol;
		b->symbolSet[index] = sortCodes[i];
	}
}

// The sort in preparation of building the Huffman tree.
static int gvrsHuffmanBuildSymbolComp(const void* a, const void* b) {
	const GvrsSymbolNode** aP = (const GvrsSymbolNode**)a;
	const GvrsSymbolNode** bP = (const GvrsSymbolNode**)b;

	int x = (*aP)->count - (*bP)->count;
	if (x == 0) {
		return (*bP)->symbol - (*aP)->symbol;
	}
	return x;
}

static GvrsSymbolNode* gvrsHuffmanBuildBranch(GvrsHuffmanBuild* b, GvrsSymbolNode* n0, GvrsSymbolNode* n1) {
	// n0 should be the first node from the queue, n1 the second
	GvrsSymbolNode* left;
	GvrsSymbolNode* right;
	if (n0->count == n1->count) {
		if (n0->isLeaf && !n1->isLeaf) {
			left = n0;
			right = n1;
		}
		else if (n1->isLeaf && !n0->isLeaf) {
			left = n1;
			right = n0;
		}
		else {
			// with equal counts, use lexical order
			// smallest symbol to the left
			if (n0->symbol < n1->symbol) {
				left = n0;
				right = n1;
			}
			else {
				left = n1;
				right = n0;
			}
		}
	}
	else {
		// put the smallest count the right
		left = n1;
		right = n0;
	}

	GvrsSymbolNode* node = b->nodes + b->nNodesAllocated;
	b->nNodesAllocated++;
	node->count = left->count + right->count;
	node->symbol = b->nNodesAllocated;  // will create a supplement to lexical order
	n0->parent = node;
	n1->parent = node;
	node->left = left;
	node->right = right;
	left->bit = 0;
	right->bit = 1;
	return node;
}

static void GvrsHuffmanBuildMakeTree(GvrsHuffmanBuild* b) {
	GvrsSymbolNode** queue = b->queue;
	int nNodesInQueue = b->nUniqueSymbols;
	qsort(queue, nNodesInQueue, sizeof(GvrsSymbolNode*), gvrsHuffmanBuildSymbolComp);

	// establish the links for the priority queue
	for (int i = 0; i < nNodesInQueue - 1; i++) {
		queue[i]->next = queue[i + 1];
		queue[i]->isLeaf = 1;
	}
	// note that queue[nNodesInQueue - 1]->next = 0

	// build the tree
	GvrsSymbolNode* firstNode = queue[0];
	while (nNodesInQueue > 2) {
		// extract and remove the first two nodes from the priority queue,
		// reassign the start of the priority queue
		GvrsSymbolNode* n0 = firstNode;
		GvrsSymbolNode* n1 = firstNode->next;
		firstNode = n1->next;
		n0->next = 0;
		n1->next = 0;
		GvrsSymbolNode* branchNode = gvrsHuffmanBuildBranch(b, n0, n1);
		nNodesInQueue--;
		// insert the new node into the priority queue
		if (branchNode->count <= firstNode->count) {
			// new node goes to the head of the queue
			branchNode->next = firstNode;
			firstNode = branchNode;
		}
		else {
			// find the insertion position for the branch node
			// it may be at the end of the list
			GvrsSymbolNode* prior = firstNode;
			GvrsSymbolNode* node = firstNode->next;
			while (node) {
				if (node->count < branchNode->count) {
					prior = node;
					node = node->next;
				}
				else {
					break;
				}
			}
			prior->next = branchNode;
			branchNode->next = node;
		}
	}
	// There are now 2 nodes in the queue.
	GvrsSymbolNode* n0 = firstNode;
	GvrsSymbolNode* n1 = firstNode->next;
	b->rootNode = gvrsHuffmanBuildBranch(b, n0, n1);


	int maxBitsInCode = 0;
	for (int i = 0; i < b->symbolSetSize; i++) {
		GvrsSymbolNode* leaf = b->nodes + i;
		GvrsSymbolNode* node = leaf;
		GvrsHuffmanSymbol* huff = b->symbolSet + i;
		if (node->count > 0) {
			huff->count = node->count;
			int k = 0;
			while (node->parent) {
				k++;
				node = node->parent;
			}
			leaf->nBitsInCode = k;
			huff->nBitsInCode = k;
			if (k > maxBitsInCode) {
				maxBitsInCode = k;
			}
		}
	}
	b->maxBitsInCode = maxBitsInCode;
}


GvrsHuffmanBuild* GvrsHuffmanBuildInt(int nSymbolsInText, int* text) {
	GvrsHuffmanBuild* b = GvrsHuffmanBuildAlloc(I_N_SYMBOLS);
	if (!b) {
		return b;
	}

	GvrsHuffmanBuildCountSymbolsInt(b, nSymbolsInText, text);
	GvrsHuffmanBuildMakeTree(b);

	if (b->maxBitsInCode >= 16) {
		int status = gvrsHuffmanBuildPackageMerge(15, b, b->symbolSetSize, b->symbolSet);
		if (status) {
			// the merge failed. probably a memory error
			return GvrsHuffmanBuildFree(b);
		}
	}

	gvrsHuffmanBuildCanonicalCodes(b);
	gvrsHuffmanBuildPrepSymbolOutput(b);
	return b;
}


GvrsHuffmanBuild* GvrsHuffmanBuildByte(int nSymbolsInText, uint8_t* text) {
	GvrsHuffmanBuild* b = GvrsHuffmanBuildAlloc(256);
	if (!b) {
		return b;
	}

	GvrsHuffmanBuildCountSymbolsByte(b, nSymbolsInText, text);
	GvrsHuffmanBuildMakeTree(b);

	if (b->maxBitsInCode >= 16) {
		gvrsHuffmanBuildPackageMerge(15, b, b->symbolSetSize, b->symbolSet);
	}

	gvrsHuffmanBuildCanonicalCodes(b);
	gvrsHuffmanBuildPrepSymbolOutput(b);
	return b;
}


static GvrsHuffmanBuild* gvrsHuffmanBuildRunLength(int nSymbolsInText, uint8_t* text) {
	GvrsHuffmanBuild* b = GvrsHuffmanBuildAlloc(CL_N_SYMBOLS);
	if (!b) {
		return b;
	}

	// The gridfour implementation expects that the end-of-text code will be included
	// in the tree structure.  This provision is in place because the algorithms used by
	// Gridfour always need to have at least 2 unique symbols in the symbol set.
	// However, when storing the run-length symbol codes, Gridfour does not actually
    // record the end-of-text symbol in the output bit stream.  It is only provided
	// to support the tree-building logic.
	GvrsHuffmanBuildCountSymbolsByte(b, nSymbolsInText, text);
	b->nodes[CL_END_OF_TEXT].count = 1;
	GvrsHuffmanBuildMakeTree(b);

	if (b->maxBitsInCode >= 16) {
		gvrsHuffmanBuildPackageMerge(15, b, b->symbolSetSize, b->symbolSet);
	}

	gvrsHuffmanBuildCanonicalCodes(b);
	gvrsHuffmanBuildPrepSymbolOutput(b);
	return b;
}





void gvrsHuffmanBuildWriteOneSymbol(GvrsHuffmanBuild* b, GvrsBitOutput* output, int index) {
	GvrsHuffmanSymbol* s = b->symbolSet + index;
	int code = s->output;
	int nBit = s->nBitsInCode;
	int nByte = nBit >> 3;
	if (nByte > 0) {
		nBit &= 0x7;
		GvrsBitOutputPutByte(output, code & 0xff);
		code >>= 8;
		nByte--;
		if (nByte > 0) {
			GvrsBitOutputPutByte(output, code & 0xff);
			code >>= 8;
		}
	}
	while (nBit > 0) {
		GvrsBitOutputPutBit(output, code & 1);
		nBit--;
		code >>= 1;
	}
}

static int gvrsHuffmanBuildWriteInt(GvrsHuffmanBuild* b, int nSymbolsInText, int* text, GvrsBitOutput* output) {

	for (int i = 0; i < nSymbolsInText; i++) {
		int symbol = text[i];
		if (-128 <= symbol && symbol <= 127) {
			// the symbol is in the range of standard (one byte) symbols
			int index = symbol + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, index);
		}
		else if (-512 <= symbol && symbol <= 511) {
			//  8+2 bit symbol with one 2-bit b->escape sequence
			int target = (symbol >> 2) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);
			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, symbol & 0x03);
		}
		else if (-2048 <= symbol && symbol <= 2047) {
			//  8+4 bit symbol as 2 sets of 2-bit b->escape sequences
			int target = (symbol >> 4) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, (symbol >> 2) & 0x03);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, symbol & 0x03);
		}
		else if (-8192 <= symbol && symbol <= 8191) {
			//  8+6 bit symbol as 3 sets of 2-bit b->escape sequences
			int target = (symbol >> 6) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, (symbol >> 4) & 0x03);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, (symbol >> 2) & 0x03);


			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_2BITS);
			GvrsBitOutputPutMultiBits(output, 2, symbol & 0x03);
		}
		else if (-32768 <= symbol && symbol <= 32767) {
			// 8+8 bit symbol with one 8-bit b->escape sequence
			int target = (symbol >> 8) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, symbol & 0xff);
		}
		else if (symbol == INT_MIN) {
			gvrsHuffmanBuildWriteOneSymbol(b, output, I_NULL_DATA_CODE);
		}
		else if (-8388608 <= symbol && symbol <= 8388607) {
			int target = (symbol >> 16) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, (symbol >> 8) & 0xff);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, symbol & 0xff);
		}
		else {
			int target = (symbol >> 24) + 128;
			gvrsHuffmanBuildWriteOneSymbol(b, output, target);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, (symbol >> 16) & 0xff);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, (symbol >> 8) & 0xff);

			gvrsHuffmanBuildWriteOneSymbol(b, output, I_ESCAPE_1BYTE);
			GvrsBitOutputPutByte(output, symbol & 0xff);
		}
	}

	// The end of text is always necessary because it is the only way the read operation
	// will know that there isn't an escape sequence still pending after the last
	// integer value is read.
	gvrsHuffmanBuildWriteOneSymbol(b, output, I_END_OF_TEXT);
	return 0;
}


 

static int gvrsCanonicalRunLengthEncode(int n, int* codeLen, uint8_t* countCode, int* runLength) {
	memset(runLength, 0, n * sizeof(int));
	int prior = -1;
	int i;
	int nCountCode = 0;
	for (int iCodeLen = 0; iCodeLen < n; iCodeLen++) {
		if (codeLen[iCodeLen] > CL_MAX_STANDARD) {
			return -1; // invalid code length
		}
		if (codeLen[iCodeLen] == 0) {
			prior = 0;
			for (i = iCodeLen + 1; i < n; i++) {
				if (codeLen[i] != 0) {
					break;
				}
			}
			int nZero = i - iCodeLen;
			if (nZero == 1) {
				countCode[nCountCode++] = 0;
			}
			else if (nZero == 2) {
				countCode[nCountCode++] = 0;
				countCode[nCountCode++] = 0;
				iCodeLen++; // skip the repeat
			}
			else if (nZero <= 10) {
				// in the range 3 to 10
				countCode[nCountCode] = CL_REPEAT_ZERO_3BITS;
				runLength[nCountCode] = nZero - 3;
				nCountCode++;
				iCodeLen = i - 1; // -1 because loop control will add one
			}
			else {
				if (nZero > 138) {
					nZero = 138; // 138 is the maximum we can store
				}
				countCode[nCountCode] = CL_REPEAT_ZERO_7BITS;
				runLength[nCountCode] = nZero - 11;
				nCountCode++;
				iCodeLen += (nZero - 1); // -1 because loop control will add one
			}
		}
		else {
			// non-zero
			if (codeLen[iCodeLen] == prior) {
				// The code length matches the prior.  If there are three in a row,
				// we can use the code CL_REPEAT_PREV (2-bits).  But if there's only one or
				// two, we just store a literal.
				for (i = iCodeLen + 1; i < n; i++) {
					if (codeLen[i] != prior) {
						break;
					}
				}
				int nPrior = i - iCodeLen;
				// nPrior will be at least 1.
				switch (nPrior) {
				case 1:
					countCode[nCountCode++] = prior;
					break;
				case 2:
					// not enough to use this repeat code
					countCode[nCountCode++] = prior;
					countCode[nCountCode++] = prior;
					iCodeLen = i - 1; // -1 because loop control will add one
					break;
				default:
					if (nPrior > 6) {
						nPrior = 6;  // 6 is the maximum we can store
					}
					countCode[nCountCode] = CL_REPEAT_PREV_2BITS;
					runLength[nCountCode] = nPrior - 3;
					nCountCode++;
					iCodeLen += (nPrior - 1); // -1 because loop control will add one
					break;
				}
			}
			else {
				prior = codeLen[iCodeLen];
				countCode[nCountCode++] = prior;
			}
		}
	}



	return nCountCode;


}

int GvrsCanonicalHuffmanWriteInt(int nSymbolsInText, int* text, GvrsBitOutput* output) {
	GvrsHuffmanBuild* bText = GvrsHuffmanBuildInt(nSymbolsInText, text);
	if (!bText) {
		return GVRSERR_NOMEM;
	}

	int tCodeLen[I_N_SYMBOLS];
	for (int i = 0; i < I_N_SYMBOLS; i++) {
		tCodeLen[i] = bText->symbolSet[i].nBitsInCode;
	}

	uint8_t tCountCode[I_N_SYMBOLS];
	int tRunLength[I_N_SYMBOLS];
	int tCodeN = gvrsCanonicalRunLengthEncode(I_N_SYMBOLS, tCodeLen, tCountCode, tRunLength);

	GvrsHuffmanBuild* bCount = gvrsHuffmanBuildRunLength(tCodeN, tCountCode);
	// count codes are in the range 0 to 19 (at this time), with an end-of-text symbol of 20.


	int kCodeLen[CL_N_SYMBOLS];
	uint8_t kCountCode[CL_N_SYMBOLS];
	int kRunLength[CL_N_SYMBOLS];
	for (int i = 0; i < CL_N_SYMBOLS; i++) {
		kCodeLen[i] = bCount->symbolSet[i].nBitsInCode;
	}
	int kCodeN = gvrsCanonicalRunLengthEncode(CL_N_SYMBOLS, kCodeLen, kCountCode, kRunLength);

	// now write:
	//    one bit reserved
	//    run-length encoding for pre-amble
	//    hybrid huffman/run-length code sequence for Huffman tree
	//    huffman code for text
	//    CL_REPEAT_PREV_2BITS  16   // 2 bits, 3 to 7 times
	//    CL_REPEAT_ZERO_3BITS  17   // 3 bits, 3 to 10 times
	//    CL_REPEAT_ZERO_7BITS  18   // 7 bits, 11 to 138 times
	GvrsBitOutputPutBit(output, 0);  // version code: reserved for future use.
	for (int i = 0; i < kCodeN; i++) {
		GvrsBitOutputPutMultiBits(output, 5, kCountCode[i]);
		switch (kCountCode[i]) {
		case 16:
			GvrsBitOutputPutMultiBits(output, 2, kRunLength[i]);
			break;
		case 17:
			GvrsBitOutputPutMultiBits(output, 3, kRunLength[i]);
			break;
		case 18:
			GvrsBitOutputPutMultiBits(output, 7, kRunLength[i]);
			break;
		default:
			break;
		}
	}
 
	for (int i = 0; i < tCodeN; i++) {
		gvrsHuffmanBuildWriteOneSymbol(bCount, output, tCountCode[i]);
		switch (tCountCode[i]) {
		case 16:
			GvrsBitOutputPutMultiBits(output, 2, tRunLength[i]);
			break;
		case 17:
			GvrsBitOutputPutMultiBits(output, 3, tRunLength[i]);
			break;
		case 18:
			GvrsBitOutputPutMultiBits(output, 7, tRunLength[i]);
			break;
		default:
			break;
		}
	}


    int status =  gvrsHuffmanBuildWriteInt(bText, nSymbolsInText, text, output);
	GvrsHuffmanBuildFree(bCount);
	GvrsHuffmanBuildFree(bText);
	return status;
}
