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
#include "GvrsCompressHuffman.h"

GvrsCodec* GvrsCodecHuffmanAlloc();

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

static GvrsCodec* allocateCodecHuffman(struct GvrsCodecTag* codec) {
	return GvrsCodecHuffmanAlloc();
}
 
int GvrsHuffmanDecodeTree(GvrsBitInput* input,  int *indexSize, GvrsInt** nodeIndexReference) {
	if (!input || !indexSize || !nodeIndexReference) {
		return GVRSERR_NULL_ARGUMENT;
	}

	*indexSize = 0;
	*nodeIndexReference = 0;

	int errCode = 0;
	int nLeafsToDecode = GvrsBitInputGetByte(input, &errCode) + 1;
	int rootBit = GvrsBitInputGetBit(input, &errCode);
	int* nodeIndex;

	if (rootBit == 1) {
		// This is the special case where a non-zero root
		// bit indicates that there is only one symbol in the whole
		// encoding.   There is not a proper tree, only a single root node.
		nodeIndex = (GvrsInt*)malloc(sizeof(GvrsInt));
		if (nodeIndex == 0) {
			return GVRSERR_NOMEM;
		}
		nodeIndex[0] =  GvrsBitInputGetByte(input, &errCode);
		*indexSize = 1;
		return 0;
	}

	// This particular implementation follows the lead of the original Java
	// code in that it manages the Huffman tree using an integer array.
	// The array based representation of the Huffman tree
	// is laid out as triplets of integer values each
	// representing a node
	//     [filePos+0] symbol code (or -1 for a branch node)
	//     [filePos+1] index to left child node (zero if leaf)
	//     [filePos+2] index to right child node (zero if leaf)
	// The maximum number of symbols is 256.  The number of nodes in
	// a Huffman tree is always 2*n-1.  We allocate 3 integers per
	// node, or 2*n*3.
	int nodeIndexSize = nLeafsToDecode * 6;
	nodeIndex = calloc(nodeIndexSize, sizeof(GvrsInt));
	if (!nodeIndex) {
		return  GVRSERR_NOMEM;
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
		return GVRSERR_NOMEM;
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
		// on the stack.  If the left-side node is already populates (filePos+1),
		// we will store the reference as the right-side child node (filePos+2).
		if (nodeIndex[offset + 1] == 0) {
			nodeIndex[offset + 1] = nodeIndexCount;
		}
		else {
			nodeIndex[offset + 2] = nodeIndexCount;
		}

		int bit = GvrsBitInputGetBit(input, &errCode);  
		if (bit == 1) {
			if (iStack >= stackSize || nodeIndexCount + 3 >= nodeIndexSize) {
				free(stack);
				free(nodeIndex);
				return GVRSERR_BAD_COMPRESSION_FORMAT;;
			}
			// leaf node
			nLeafsDecoded++;
			nodeIndex[nodeIndexCount++] = GvrsBitInputGetByte(input, &errCode);   
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
				return GVRSERR_BAD_COMPRESSION_FORMAT;
			}
			stack[iStack] = nodeIndexCount;
			nodeIndex[nodeIndexCount++] = -1;
			nodeIndex[nodeIndexCount++] = 0; // left node not populated
			nodeIndex[nodeIndexCount++] = 0; // right node not populated
		}
	}

	free(stack);
	*indexSize = nodeIndexCount;
	*nodeIndexReference = nodeIndex;
	return 0;
}

static int decodeInt(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsInt* data, void *appInfo) {
	int i;
	int errCode = 0;
	GvrsByte* output = 0;
	GvrsBitInput* input = 0;
	GvrsM32* m32 = 0;
	int* nodeIndex = 0;
	huffmanAppInfo* hInfo = (huffmanAppInfo*)appInfo;

	// int compressorIndex = (int)packing[0];
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

	input = GvrsBitInputAlloc(packing + 10, (size_t)(packingLength - 10), &errCode);
	if (!input) {
		cleanUp(output, input, m32, nodeIndex);
		return GVRSERR_NOMEM;
	}
 
	int indexSize;
   
	errCode = GvrsHuffmanDecodeTree(input, &indexSize, &nodeIndex);
	// GvrsInt nBitsInTree = GvrsBitInputGetPosition(input);
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
		status =   GvrsM32Alloc(output, nM32, &m32);
		if (status) {
			cleanUp(output, input, m32, nodeIndex);
			return status; // probably a memory error
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
			// for branch nodes, nodeIndex[filePos] will be -1.  when nodeIndex[filePos] > -1,
			// the traversal has reached a terminal node and is complete.
			// We know that the root node is always a branch node, so we have a shortcut.
			// We don't have to check to see if nodeIndex[0] == -1 
			int offset = nodeIndex[1 + GvrsBitInputGetBit(input, &errCode)]; // start from the root node
			while (nodeIndex[offset] == -1) {
				offset = nodeIndex[offset + 1 + GvrsBitInputGetBit(input, &errCode)];
			}
			output[i] = (GvrsByte)nodeIndex[offset];
		}

		int pos1 = GvrsBitInputGetPosition(input);
		hInfo->nBitsInDecodeBody += (GvrsLong)pos1 - (GvrsLong)pos0;
		status = GvrsM32Alloc(output, nM32, &m32);
		if (status) {
			cleanUp(output, input, m32, nodeIndex);
			return status;
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


typedef struct SymbolNodeTag {
	int symbol;
	int count;

	int isLeaf;
	int bit;
	int codeOffset;
	int nBitsInCode;
	GvrsByte* code;

	struct SymbolNodeTag* parent;
	struct SymbolNodeTag* left;
	struct SymbolNodeTag* right;
}SymbolNode;

static int symbolNodeComp(const void* a, const void* b) {
	const SymbolNode** aP = (const SymbolNode **)a;
	const SymbolNode** bP = (const SymbolNode **)b;

	int x = (*bP)->count - (*aP)->count;
	if (x == 0) {
		return (*aP)->symbol - (*bP)->symbol;
	}
	return x;
}

static SymbolNode* makeBranch(SymbolNode* left, SymbolNode* right, int* nBaseNodesAssigned, SymbolNode* baseNodes) {
	SymbolNode* node = baseNodes + (*nBaseNodesAssigned);
	(*nBaseNodesAssigned)++;
	node->count = left->count + right->count;
	left->bit = 0;
	right->bit = 1;
	node->left = left;
	node->right = right;
	left->parent = node;
	right->parent = node;
	return node;
}

static int encodeTree(GvrsBitOutput *output, SymbolNode* root, int nLeafNodes, GvrsByte** codeSequenceReference) {
	if (!output || !root || !codeSequenceReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*codeSequenceReference = 0;

	// write the number of leaf nodes (symbols) to the output.  This value will
	// be in the range 1 to 256.  Since it will never be zero, we subtract one from
	// it. This offset puts the count into the range 0 to 255, allowing it
	// to fit within a single byte.
	GvrsBitOutputPutByte(output, (GvrsByte)(nLeafNodes - 1));
	
	// Traverse the tree and record it to the bit output.  The simplest implementation
	// of this action would probably be accomplished by using recursion.  Unfortunately,
	// the depth of the resulting stack (max 256 levels) would be too much for many environments.
	// Therefore, a stack is used to indicate the state of the traversal at each iteration:
	//    
	//   TO DO: explain iPath
	
	int iPath[256]; // to track the traversal
	memset(iPath, 0, sizeof(iPath));
	SymbolNode* leafRefs[256];
	memset(leafRefs, 0, sizeof(leafRefs));
	int nLeafRef = 0;

	int status;
	GvrsBitOutput* codeSequence;
	status = GvrsBitOutputAlloc(&codeSequence);
	if (status) {
		return status;
	}
	SymbolNode* node = root;
	int depth = 0;
	while (depth>=0) {
		int state = iPath[depth];
			// traversal just arrived at the node and the code has not evaluated it
		if (node->isLeaf) {
			// To expedite encoding of the source symbols, we add the bit sequence for each
			// symbol to the leaf nodes.  To build these sequences, we use a bit-output instance.
			// There are two special considerations:
			//   1. The bit-output may re-alloc its internal memory as we add content, so we can't depend
			//      on that being stable until we are done.
			//   2. The leaf nodes have a pointer to an element named "code" with type byte.  Naturally,
			//      the bit sequence for each node must start on an even byte boundary, but the length
			//      of the Huffman code for each symbol will usually not be a multiple of eight.
			//      Thus we make calls to the bit-output "flush" function to ensure that
			//      the bit sequence starts on an even byte boundary.  
	

			int i;
			int iSeq0 = GvrsBitOutputGetBitCount(codeSequence);
			for (i = 0; i < depth; i++) {
				GvrsBitOutputPutBit(codeSequence, iPath[i]);
			}
			int iSeq1 = GvrsBitOutputGetBitCount(codeSequence);
			int nSeq = iSeq1 - iSeq0;
			GvrsBitOutputFlush(codeSequence);
			node->codeOffset = iSeq0/8;
			node->nBitsInCode = nSeq;
			leafRefs[nLeafRef++] = node;
			GvrsBitOutputPutBit(output, 1); // 1 indicates terminal
			GvrsBitOutputPutByte(output, node->symbol);
			//
			// Since the code reached a terminal node, it will now work its way back up the tree
			// until it finds branch that has only been traversed to the left.
			// Also, note that the root node can never been a leaf.
			//   
			// depth will never be less than 1, but Visual Studio can't be convinced of that.
			// so we include this unnecessary test.
			if (depth == 0) {
				break;
			}
			depth--;
			node = node->parent;
			while (iPath[depth] == 1) {
				depth--;
				node = node->parent;
				if (depth < 0) {
					// The branch node is the root and the entire tree has been
					// traversed.  No further traversal is required.
					int nBytesInText;
					GvrsByte* t = 0;
					status = GvrsBitOutputGetText(codeSequence, &nBytesInText, &t);
					GvrsBitOutputFree(codeSequence);
					if (status) {
						return GVRSERR_NOMEM;
					}
					for (i = 0; i < nLeafRef; i++) {
						SymbolNode* r = leafRefs[i];
						r->code = t + r->codeOffset;
					}
					*codeSequenceReference = t;
					return 0;
				}
			}
			iPath[depth] = 1; // this will signal the next traversal to branch to the right.
		}
		else {
			// Branch node
			if (state == 0) {
				// This is the first time the code has visited this node
				// store a bit value of zero indicating that it is a branch
				GvrsBitOutputPutBit(output, 0); 
				// traverse down the left side
				node = node->left;
			}
			else {
				// State is 1, this node has been visited before.
				// Traverse down the right side
				node = node->right;
			}
			depth++;
			iPath[depth] = 0;
		}
	}
	
	return 0;
}



int GvrsHuffmanCompress(int nSymbols, GvrsByte* symbols,  GvrsBitOutput *output) {
	if (nSymbols <= 0 || !symbols || !output) {
		return GVRSERR_NOMEM;
	}
 
	int status = 0;
	int i, j;
	SymbolNode* baseNodes = calloc(512, sizeof(SymbolNode)); // the max number of nodes for N symbols is 2*N-1
	if (!baseNodes) {
		return GVRSERR_NOMEM;
	}
 
	for (i = 0; i < 256; i++) {
		baseNodes[i].symbol = i;
		baseNodes[i].isLeaf = 1;
	}
	for (i = 256; i < 512; i++) {
		baseNodes[i].symbol = i;  // used as a diagnostic
	}
	for (i = 0; i < nSymbols; i++) {
		baseNodes[symbols[i]].count++;
	}

	// A priority queue is implemented as a simple array of pointers
	// into the baseNodes.  The queue is populated with base nodes
	// that have a non-zero count and then sorted by count (primary key, largest to smallest)
	// and symbol (secondary key).
	SymbolNode* queue[256];
	memset(queue, 0, sizeof(queue));

	int nLeafNodes = 0;
	for (i = 0; i < 256; i++) {
		if (baseNodes[i].count) {
			queue[nLeafNodes++] = baseNodes + i;
		}
	}

	if (nLeafNodes == 1) {
		// only one unique symbol in the source data.
	// this is a special case in which a Huffman tree would be incomplete
	// and we code it as such
		return 0;
	}



	// sort nodes by count in descending order; use the symbol value as a secondary sort key
	qsort(queue, nLeafNodes, sizeof(SymbolNode*), symbolNodeComp);

	// build the tree using Huffman's algorithm.  
	int nNodesInQueue = nLeafNodes;
	int nBaseNodesAssigned = 256; // used to allocate nodes from baseNodes.

	SymbolNode* root = 0;
	while (1) {
		SymbolNode* left = queue[nNodesInQueue-2];
		SymbolNode* right = queue[nNodesInQueue-1];
		SymbolNode* node = makeBranch(left, right, &nBaseNodesAssigned, baseNodes);
		if (nNodesInQueue == 2) {
			root = node;
			break;
		}

		// Remove the last two nodes from the priority queue,
		// insert the new node in the proper position.
		// The total number of nodes in the queue will be reduced by 1.
		// The following loop simultaneously searches for the insertion index and, if necessary,
		// shifts up nodes to make room for the insertion 
		nNodesInQueue -= 2;   // in effect, remove last two nodes
		int insertionIndex = nNodesInQueue;  // position at end of remaining queue
		for (i = nNodesInQueue - 1; i >= 0; i--) {
			if (queue[i]->count >= node->count) {
				break;
			}
			insertionIndex = i;
			queue[i + 1] = queue[i];
		}
		nNodesInQueue++;
		queue[insertionIndex] = node;
		queue[nNodesInQueue] = 0; // a diagnostic
	}

	// the tree is complete
	// Diagnostic print of the tree
	//printf("root %d\n", root->symbol);
	//for (i = 256; i < nBaseNodesAssigned; i++) {
	//	SymbolNode* left = baseNodes[i].left;
	//	SymbolNode* right = baseNodes[i].right;
	//	SymbolNode* parent = baseNodes[i].parent;
	//	int parentSymbol = -1;
	//	if (parent) {
	//		parentSymbol = parent->symbol;
	//	}
	//	printf("branch %3d %4d    L %3d  R %3d    P %3d\n", 
	//		baseNodes[i].symbol, baseNodes[i].count, left->symbol,  right->symbol, parentSymbol);
	//}
	 
	
	
	GvrsByte* codeSequenceReference;  // not accessed directly, should be freed at end of processing
	encodeTree(output, root, nLeafNodes, &codeSequenceReference);
	 
	// Add the Huffman codes for the symbols to the output
	for (i = 0; i < nSymbols; i++) {
		SymbolNode* s = baseNodes + (symbols[i]);
		int nWholeBytesInCode = s->nBitsInCode / 8;  // the number of whole bytes
		int nBitsRemaining = s->nBitsInCode & 7;
		for (j = 0; j < nWholeBytesInCode; j++) {
			status = GvrsBitOutputPutByte(output, s->code[j]);
		}
		if (nBitsRemaining) {
			int scratch = s->code[nWholeBytesInCode];
			for (j = 0; j < nBitsRemaining; j++) {
				status = GvrsBitOutputPutBit(output, scratch);
				scratch >>= 1;
			}
		}
	}

	if (status) {
		return status;
	}


	// Diagnostic, decode the tree just encoded.

	//int nBytesInText;
	//GvrsByte* text;
	//status = GvrsBitOutputGetText(output, &nBytesInText, &text);
	//GvrsBitInput* input =	GvrsBitInputAlloc(text+reservedIntroBytes, nBytesInText-reservedIntroText, &status);
	//int indexSize;
	//GvrsInt* nodeIndex = decodeTree(input, &indexSize, &status);
	//printf("Tree status %d, indexSize %d\n", status, indexSize);
	//for (int i = 0; i < nSymbols; i++) {
	//	// start from the root node at nodeIndex[0]
	//	// for branch nodes, nodeIndex[filePos] will be -1.  when nodeIndex[filePos] > -1,
	//	// the traversal has reached a terminal node and is complete.
	//	// We know that the root node is always a branch node, so we have a shortcut.
	//	// We don't have to check to see if nodeIndex[0] == -1 
	//	int offset = nodeIndex[1 + GvrsBitInputGetBit(input, &status)]; // start from the root node
	//	while (nodeIndex[offset] == -1) {
	//		offset = nodeIndex[offset + 1 + GvrsBitInputGetBit(input, &status)];
	//	}
	//	int testValue = (GvrsByte)nodeIndex[offset];
	//	printf("test sym,val:  %d,%d\n", symbols[i], testValue);
	//}

 //


 
	// the code sequence was used to store the encoding bits for each symbol.
	// it is no longer needed.
	free(codeSequenceReference);
	free(baseNodes);
	return status;
}


static int encodeInt(int nRow, int nColumn,
	GvrsInt* data,
	int index,
	int* packingLengthReference,
	GvrsByte** packingReference,
	void* appInfo) {
	if (!data || !packingLengthReference || !packingReference) {
		return GVRSERR_NULL_ARGUMENT;
	}

	*packingLengthReference = 0;
	*packingReference = 0;

	int packingLength = 0;
	GvrsByte* packing = 0;
	GvrsInt seed;

	int status;
	for (int iPack = 1; iPack <= 3; iPack++) {
		GvrsM32* m32 = 0;
		if (iPack == 1) {
			status = GvrsPredictor1encode(nRow, nColumn, data, &seed, &m32);
		}
		else if (iPack == 2) {
			status = GvrsPredictor2encode(nRow, nColumn, data, &seed, &m32);
		}
		else {
			status = GvrsPredictor3encode(nRow, nColumn, data, &seed, &m32);
		}
		if (status) {
			GvrsM32Free(m32);
			if (packing) {
				free(packing);
			}
			return status;
		}

		GvrsBitOutput* bitOutput;
		status = GvrsBitOutputAlloc(&bitOutput);
		if (status) {
			if (packing) {
				free(packing);
			}
			return status;
		}
		int nBytesToCompress = m32->offset;
		GvrsByte* bytesToCompress = m32->buffer;

		GvrsByte* h;
		status = GvrsBitOutputReserveBytes(bitOutput, 10, &h);
		if (status) {
			if (packing) {
				free(packing);
				return status;
			}
		}
		h[0] = (GvrsByte)index;
		h[1] = (GvrsByte)(iPack);
		h[2] = (GvrsByte)(seed & 0xff);
		h[3] = (GvrsByte)((seed >> 8) & 0xff);
		h[4] = (GvrsByte)((seed >> 16) & 0xff);
		h[5] = (GvrsByte)((seed >> 24) & 0xff);
		h[6] = (GvrsByte)((nBytesToCompress & 0xff));
		h[7] = (GvrsByte)((nBytesToCompress >> 8) & 0xff);
		h[8] = (GvrsByte)((nBytesToCompress >> 16) & 0xff);
		h[9] = (GvrsByte)((nBytesToCompress >> 24) & 0xff);


		GvrsByte* b = 0;;
		int bLen = 0;
	
		status = GvrsHuffmanCompress(nBytesToCompress, bytesToCompress, bitOutput);
		GvrsM32Free(m32);
		if (status) {
			GvrsBitOutputFree(bitOutput);
			if (packing) {
				free(packing);
			}
			return status;
		}

		// If the packing is defined, we will only accept the newer results if the output text size
		// is smaller than the previous results.
		if (packing) {
			int n = (GvrsBitOutputGetBitCount(bitOutput) + 7) / 8;
			if (n >= packingLength) {
				GvrsBitOutputFree(bitOutput);
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

		status = GvrsBitOutputGetText(bitOutput, &bLen, &b);
		GvrsBitOutputFree(bitOutput);
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
	codec->encodeInt = encodeInt;
	codec->destroyCodec = destroyCodecHuffman;
	codec->allocateNewCodec = allocateCodecHuffman;
	return codec;
}
