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
	int inflatedLength = zs.total_out;
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
	GvrsBitInput* bitInput = GvrsBitInputAlloc(signBytes, nSignBytes);
	for (i = 0; i < nCellsInTile; i++) {
		rawInt[i] = GvrsBitInputGetBit(bitInput) << 31;
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



GvrsCodec* GvrsCodecFloatAlloc() {
	GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
	codec->description = GVRS_STRDUP(description);
	codec->decodeFloat = decodeFloat;
	codec->destroyCodec = destroyCodecFloat;
	return codec;
}

