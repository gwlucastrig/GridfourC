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
#include "zlib.h"

// case-sensitive name of codec
static const char* identification = "GvrsFloat";
static const char* description = "Implements the standard GVRS compression for floating-point data";



static GvrsCodec* destroyCodecFloat(struct GvrsCodecTag* codec) {
	if (codec) {
		if (codec->description) {
			free(codec->description);
			codec->description = 0;
		}
		free(codec);
	}
	return 0;
}

static GvrsCodec* allocateCodecFloat(struct GvrsCodecTag* codec) {
	return GvrsCodecFloatAlloc();
}

static GvrsInt unpackInteger(GvrsByte input[], int offset) {
	return (input[offset] & 0xff)
		| ((input[offset + 1] & 0xff) << 8)
		| ((input[offset + 2] & 0xff) << 16)
		| ((input[offset + 3] & 0xff) << 24);
}

static GvrsByte* doInflate(GvrsByte* input, int inputLength, int outputLength, int* errCode) {
	*errCode = 0;
	unsigned char* output = (unsigned char*)malloc(outputLength);
	if (!output) {
		*errCode = GVRSERR_NOMEM;
		return 0;
	}

	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = inputLength;
	zs.next_in = (Bytef*)input;
	zs.avail_out = (uInt)outputLength;
	zs.next_out = (Bytef*)output;

	inflateInit2(&zs, 15);
	int status = inflate(&zs, Z_FINISH);
	inflateEnd(&zs);
	if (status != Z_STREAM_END) {
		free(output);
		*errCode =  GVRSERR_BAD_COMPRESSION_FORMAT;
		return 0;
	}
	return output;
}



static void decodeDeltas(GvrsByte* scratch, int nRows, int nColumns) {
	int iRow, iCol;
	int prior = 0;
	int k = 0;
	for (iRow = 0; iRow < nRows; iRow++) {
		for (iCol = 0; iCol < nColumns; iCol++) {
			prior += scratch[k];
			scratch[k++] = (GvrsByte)prior;
		}
		prior = scratch[iRow * nColumns];
	}
}


 
static int decodeFloat(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsFloat* data, void *appData) {

	// the packing layout:
	//    packing[0]   compression index (can be used as a diagnostic)
	//    packing[1]   predictor (not used)
	//    packing[2..5]  length of compressed sign-bytes
	//    packing[6..*]  packed sign-bytes

	GvrsUnsignedInt* rawInt = (GvrsUnsignedInt*)data;
	int i;
	int errCode = 0;
	int nCellsInTile = nRow * nColumn;
	int nSignBytes = (nCellsInTile + 7) / 8;
	int offset = 2;
	int lengthSignPacking = unpackInteger(packing, offset);
	offset += 4;
	GvrsByte* signBytes = doInflate(packing + offset, lengthSignPacking, nSignBytes, &errCode);
	if (!signBytes) {
		return errCode;
	}
	offset += lengthSignPacking;
	GvrsBitInput* bitInput = GvrsBitInputAlloc(signBytes, nSignBytes, &errCode);
	if (errCode) {
		return errCode;
	}
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] = GvrsBitInputGetBit(bitInput, &errCode) << 31;
	}
	free(signBytes);
	bitInput = GvrsBitInputFree(bitInput);

	int lengthExponentPacking = unpackInteger(packing, offset);
	offset += 4;
	GvrsByte* exponentBytes = doInflate(packing + offset, lengthExponentPacking, nCellsInTile, &errCode);
	if (!exponentBytes) {
		return errCode;
	}
	offset += lengthExponentPacking;
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] |= (exponentBytes[i] & 0xff) << 23;   // TO DO: is the & 0xff actually needed?
	}
	free(exponentBytes);
	exponentBytes = 0;

	int lengthMan0Packing = unpackInteger(packing, offset);
	offset += 4;
	GvrsByte* man0Bytes = doInflate(packing + offset, lengthMan0Packing, nCellsInTile, &errCode);
	if (!man0Bytes) {
		return errCode;
	}
	offset += lengthMan0Packing;
	decodeDeltas(man0Bytes, nRow, nColumn);
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] |= (man0Bytes[i] & 0x7f) << 16;   // TO DO: is the & 0x7f actually needed?
	}
	free(man0Bytes);
	man0Bytes = 0;



	int lengthMan1Packing = unpackInteger(packing, offset);
	offset += 4;
	GvrsByte* man1Bytes = doInflate(packing + offset, lengthMan1Packing, nCellsInTile, &errCode);
	if (!man1Bytes) {
		return errCode;
	}
	offset += lengthMan1Packing;
	decodeDeltas(man1Bytes, nRow, nColumn);
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] |= (man1Bytes[i] & 0xff) << 8; 
	}
	free(man1Bytes);
	man1Bytes = 0;


	int lengthMan2Packing = unpackInteger(packing, offset);
	offset += 4;
	GvrsByte* man2Bytes = doInflate(packing + offset, lengthMan2Packing, nCellsInTile, &errCode);
	if (!man2Bytes) {
		return errCode;
	}
	offset += lengthMan2Packing;
	decodeDeltas(man2Bytes, nRow, nColumn);
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] |= (man2Bytes[i] & 0xff);
	}
	free(man2Bytes);
 
	return 0;
}



static void encodeDeltas(GvrsByte* scratch, int nRows, int nColumns) {
	int prior0 = 0;
	int test;
	int k = 0;
	for (int iRow = 0; iRow < nRows; iRow++) {
		int prior = prior0;
		prior0 = scratch[k];
		for (int iCol = 0; iCol < nColumns; iCol++) {
			test = scratch[k];
			scratch[k++] = (GvrsByte)(test - prior);
			prior = test;
		}
	}
}


static int doDeflate(int nBytesAvailable, GvrsByte *output, int inputLength, GvrsByte *input, int *lengthCompressed){
    
	*lengthCompressed = 0;

	z_stream strm;
	memset(&strm, 0, sizeof(z_stream));
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	int status = deflateInit(&strm, 6);
	if (status != Z_OK) {
		return  GVRSERR_COMPRESSION_FAILURE;
	}

	strm.avail_in = inputLength;
	strm.next_in = input;
	strm.avail_out = nBytesAvailable-4;
	strm.next_out = output+4;
	status = deflate(&strm, Z_FINISH);
	if (status == Z_STREAM_ERROR) {
		return  GVRSERR_COMPRESSION_FAILURE;
	}
	if (status != Z_STREAM_END || (int)strm.total_out >= inputLength) {
		// The packing wasn't large enough to store the full compression
		// or this compressed format was larger than the input.
		// This would happen if the data was essentially non-compressible.
		return GVRSERR_COMPRESSION_FAILURE;
	}

	status = deflateEnd(&strm);
	if (status != Z_OK) {
		return  GVRSERR_COMPRESSION_FAILURE;
	}

	int lenOut = strm.total_out;
	output[0] = (GvrsByte)(lenOut & 0xff);
	output[1] = (GvrsByte)((lenOut >> 8) & 0xff);
	output[2] = (GvrsByte)((lenOut >> 16) & 0xff);
	output[3] = (GvrsByte)((lenOut >> 24) & 0xff);
	*lengthCompressed += (4 + lenOut);

	return 0;
}
 
static int cleanUp(int status, GvrsByte* iData, GvrsByte* packing, GvrsByte *sBits) {
	if (iData) {
		free(iData);
	}
	if (packing) {
		free(packing);
	}
	if (sBits) {
		free(sBits);
	}
	return status;
}


#define BitsFromF(A, B)   *((unsigned int*)( (A)+(B) ))

static int encodeFloat(int nRow, int nColumn, GvrsFloat* data, int index, int* packingLength, GvrsByte** packingReference, void* appInfo) {
	if (!data || !packingLength || !packingReference) {
		return GVRSERR_NULL_ARGUMENT;
	}

	*packingReference = 0;

	int i;
	int status;
	int nCellsInTile = nRow * nColumn;
	int nBytesInData = nCellsInTile * 4;

	GvrsBitOutput* bitOutput;
	status = GvrsBitOutputAlloc(&bitOutput);
	if (status) {
		return status;
	}

	GvrsByte* iData = calloc(nBytesInData, sizeof(GvrsByte));
	if (!iData) {
		GvrsBitOutputFree(bitOutput);
		return GVRSERR_NOMEM;
	}

	GvrsByte* sEx = iData; // 8 bits of exponent
	GvrsByte* sM1 = iData + nCellsInTile;   // high 7 bits of mantissa
	GvrsByte* sM2 = sM1 + nCellsInTile;     // middle 8 bits of mantissa
	GvrsByte* sM3 = sM2 + nCellsInTile;     // low 8 bits of mantissa

	// Copy fragments of floating-point bit patterns into arrays
	// for processing
	for (i = 0; i < nCellsInTile; i++) {
		unsigned int bits = BitsFromF(data, i);
		GvrsBitOutputPutBit(bitOutput, bits >> 31);
		sEx[i] = (GvrsByte)((bits >> 23) & 0xff);
		sM1[i] = (GvrsByte)((bits >> 16) & 0x7f);
		sM2[i] = (GvrsByte)((bits >> 8) & 0xff);
		sM3[i] = (GvrsByte)(bits & 0xff);
	}

	int sBitLength;
	GvrsByte* sBits;

	status = GvrsBitOutputGetText(bitOutput, &sBitLength, &sBits);
	if (status) {
		GvrsBitOutputFree(bitOutput);
		free(iData);
		return status;
	}
	GvrsBitOutputFree(bitOutput);
	bitOutput = 0;

	// The exponents are stored as is, but the mantissa fragments are
	// stored using a differencing format.
	encodeDeltas(  sM1, nRow, nColumn);
	encodeDeltas(  sM2, nRow, nColumn);
	encodeDeltas(  sM3, nRow, nColumn);


	int nBytesAvailable = nBytesInData + 2;
	GvrsByte* packing = calloc(nBytesAvailable, sizeof(GvrsByte));
	if (!packing) {
		free(iData);
		return GVRSERR_NOMEM;
	}
	packing[0] = (GvrsByte)index;
	// packing[1] is currently left as zero
	nBytesAvailable -= 2;

	int compLength;
	int nBytesConsumed = 2;
	status = doDeflate(nBytesAvailable, packing + nBytesConsumed, sBitLength, sBits, &compLength);
	if (status) {
		return cleanUp(status, iData, packing, sBits);
	}
	free(sBits);
	sBits = 0;

	nBytesAvailable -= compLength;
	nBytesConsumed += compLength;

	status = doDeflate(nBytesAvailable, packing + nBytesConsumed, nCellsInTile, sEx, &compLength);
	if (status) {
		return cleanUp(status, iData, packing, sBits);
	}
	nBytesAvailable -= compLength;
	nBytesConsumed += compLength;

	status = doDeflate(nBytesAvailable, packing + nBytesConsumed, nCellsInTile, sM1, &compLength);
	if (status) {
		return cleanUp(status, iData, packing, sBits);
	}
	nBytesAvailable -= compLength;
	nBytesConsumed += compLength;

	status = doDeflate(nBytesAvailable, packing + nBytesConsumed, nCellsInTile, sM2, &compLength);
	if (status) {
		return cleanUp(status, iData, packing, sBits);
	}
	nBytesAvailable -= compLength;
	nBytesConsumed += compLength;

	status = doDeflate(nBytesAvailable, packing + nBytesConsumed, nCellsInTile, sM3, &compLength);
	if (status) {
		return cleanUp(status, iData, packing, sBits);
	}
	nBytesAvailable -= compLength;
	nBytesConsumed += compLength;


	free(iData);
	free(sBits);
	*packingLength = nBytesConsumed;
	*packingReference = packing;
	return 0;
}


GvrsCodec* GvrsCodecFloatAlloc() {
	GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
	codec->description = GVRS_STRDUP(description);
	codec->decodeFloat = decodeFloat;
	codec->encodeFloat = encodeFloat;
	codec->destroyCodec = destroyCodecFloat;
	codec->allocateNewCodec = allocateCodecFloat;
	return codec;
}

