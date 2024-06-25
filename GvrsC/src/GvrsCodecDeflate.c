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
static const char* identification = "GvrsDeflate";
static const char* description = "Implements the standard GVRS compression using Deflate";

static GvrsCodec* destroyCodecDeflate(struct GvrsCodecTag* codec) {
	if (codec) {
		if (codec->description) {
			free(codec->description);
			codec->description = 0;
		}
		free(codec);
	}
	return 0;
}
 
static int decodeInt(int nRow, int nColumn, int packingLength, GvrsByte* packing, GvrsInt* data, void *appInfo) {

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

	unsigned char* input = packing + 10;
	unsigned char* output = (unsigned char*)malloc(nM32);
	if (!output) {
		return GVRSERR_NOMEM;
	}
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = packingLength - 10;
	zs.next_in = (Bytef*)input;
	zs.avail_out = (uInt)nM32;
	zs.next_out = (Bytef*)output;

	inflateInit2(&zs, 15);
	int status = inflate(&zs, Z_FINISH);
	int inflatedLength = zs.total_out;
	inflateEnd(&zs);
	if (status != Z_STREAM_END) {
		free(output);
		return GVRSERR_BAD_COMPRESSION_FORMAT;
	}

 

	status = 0;
	GvrsM32* m32 = GvrsM32Alloc(output, inflatedLength);
	switch (predictorIndex) {
	case 0:
		status =  GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
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
	free(output);
	GvrsM32Free(m32);

	return status;
}

GvrsCodec* GvrsCodecDeflateAlloc() {
    GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		GvrsError = GVRSERR_NOMEM;
		return 0;
	}
    GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
    codec->description = GVRS_STRDUP(description);
    codec->decodeInt = decodeInt;
	codec->destroyCodec = destroyCodecDeflate;
	return codec;
}
