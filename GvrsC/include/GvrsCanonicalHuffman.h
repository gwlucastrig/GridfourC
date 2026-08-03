

#ifndef GVRS_HUFFMAN_H
#define GVRS_HUFFMAN_H

#include "GvrsFramework.h"
#include "GvrsCodec.h"


#ifdef __cplusplus
extern "C"
{
#endif



 
// The number of symbols defined by the Gridfour implementation of
// canonical Huffman coding for full-ranmge integers. Symbols are assigned values starting
// with zero, giving a range of values from zero to I_N_SYMBOLS-1.
// An array dimensions to I_N_SYMBOLS is sufficient to hold one element
// per symbol.
 
#define  I_N_SYMBOLS 260
#define  I_MAX_STANDARD  256
#define  I_NULL_DATA_CODE  256
#define  I_ESCAPE_1BYTE  257
#define  I_ESCAPE_2BITS  258
#define  I_END_OF_TEXT  259

#define INT4_NULL_CODE -2147483648


// Symbols for run-length encoding 
// There are 19 symbols for the conventional run-length incoding symbols.
// Additionally, the Gridfour implementation defines an end-of-text symbol.

#define CL_N_SYMBOLS          20
#define CL_MAX_STANDARD       15
#define CL_REPEAT_PREV_2BITS  16   // 2 bits, 3 to 7 times
#define CL_REPEAT_ZERO_3BITS  17   // 3 bits, 3 to 10 times
#define CL_REPEAT_ZERO_7BITS  18   // 7 bits, 11 to 138 times
#define CL_END_OF_TEXT        19



	typedef struct GvrsCanonicalHuffmanAppInfoTag {
		int32_t nDecoded;
		int64_t nBitsInPreamble;
		int64_t nBitsInCodeTable;
		int64_t nBitsInEncodedBody;

		int revision;

	}GvrsCanonicalHuffmanAppInfo;


	typedef struct GvrsSymbolNodeTag {
		int symbol;
		int count;

		int isLeaf;
		int bit;
		int nBitsInCode;


		struct GvrsSymbolNodeTag* next;
		struct GvrsSymbolNodeTag* parent;
		struct GvrsSymbolNodeTag* left;
		struct GvrsSymbolNodeTag* right;
	}GvrsSymbolNode;

	typedef struct GvrsHuffmanSymbolTag {
		int symbol;
		int count;
		int nBitsInCode;
		int code;
		int output; // the code with bit order reversed.
	}
	GvrsHuffmanSymbol;

	typedef struct GvrsHuffmanBuildTag {
		int symbolSetSize;  // number of symbols in alphabet
		int nSymbolsInText;
		int nUniqueSymbols;
		int uniformValueText;
		int nNodesAllocated;
		int maxBitsInCode;

		GvrsSymbolNode* rootNode;
		GvrsSymbolNode* nodes;
		GvrsSymbolNode** queue;

		GvrsHuffmanSymbol* symbolSet;

		int count2Bit[4];
		int count8Bit[256];
		int escapeCountBits2;
		int escapeCountBits4;
		int escapeCountBits6;
		int escapeCountBits8;
		int escapeCountBits16;
		int escapeCountBits24;
	}GvrsHuffmanBuild;

	/**
	* Frees memory associated with a Huffman build structure
	* @param build the structure to be freed (null inputs are ignored)
	* @return a null pointer
	*/
	GvrsHuffmanBuild* GvrsHuffmanBuildFree(GvrsHuffmanBuild* build);

	/**
	* Constructs a Huffman build structure for the input text. The text
	* is treated as having the full range of integer values.
	* The text is not actually encoded, but is surveyed in preparation
	* of compression (the number of symbols are counted
	* and the code table is prepared).
	* @param nSymbolsInText the number of symbols in the text array
	* @param text an array of integers giving values to be encoded
	* @return if successful, a valid pointer; otherwise, a null pointer.
	*/
	GvrsHuffmanBuild* GvrsHuffmanBuildInt(int nSymbolsInText, int* text);


	/**
	* Constructs a Huffman build structure for the input text. The text
	* is treated as having the full range of byte values (0 to 256).
	* The text is not actually encoded, but is surveyed in preparation
	* of compression (the number of symbols are counted
	* and the code table is prepared).
	* @param nSymbolsInText the number of symbols in the text array
	* @param text an array of integers giving values to be encoded
	* @return if successful, a valid pointer; otherwise, a null pointer.
	*/
	GvrsHuffmanBuild* GvrsHuffmanBuildByte(int nSymbolsInText, uint8_t* text);



	/**
	* Encodes the specified text using canonical Huffman coding and appends the results to the
	* specified output.  The text is treated as having the full range of integer values.
	* @param nSymbolsInText the number of symbols (integer values) in the text.
	* @param text a set of integer values.
	* @param output a valid reference to a bit-output stream.
	* @return if successful, zero; otherwise an error code
	*/
	int GvrsCanonicalHuffmanWriteInt(int nSymbolsInText, int* text, GvrsBitOutput* output);

	/**
	* Reads the contend of a Huffman coded message from the input bit stream and stores the result in the text.
	* At this time, the symbol count parameter serves as a protection to prevent the logic from reading more
	* entries than are allocated for storage in the text.
	* @param input a valid bit source
	* @param nSymbolsInText the maximum number of symbols that can be read into the text array.
	* @param text a reference to a valid storage location allocated for at least nSymbolsInText entries.
	* @param appInfo a reference to a structure for storing access statistics; or an null if not required.
	* @return if successful, the number of symbols read from input; ortherwise a negative value giving an error code.
	*/
	int GvrsCanonicalHuffmanReadInt(GvrsBitInput* input, int nSymbolsInText, int* text, GvrsCanonicalHuffmanAppInfo* appInfo);


#endif
