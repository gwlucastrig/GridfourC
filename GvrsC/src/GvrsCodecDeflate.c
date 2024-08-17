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

GvrsCodec* GvrsCodecDeflateAlloc();

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
 
static GvrsCodec* allocateCodecDeflate(struct GvrsCodecTag* codec) {
	return GvrsCodecDeflateAlloc();
}

#ifdef GVRS_TEST_UNPACK
static int testUnpack(GvrsByte* packing, int packingLength, GvrsM32* m32) {
	int nM32 = m32->offset;

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

	inflateInit2(&zs, MAX_WBITS);
	int status = inflate(&zs, Z_FINISH);
	int inflatedLength = zs.total_out;
	int status1 = inflateEnd(&zs);

	if (status != Z_STREAM_END || status1!=Z_OK) {
		printf("Internal pack, unpack round-trip failed with status != Z_STREAM_END");
		exit(1);
	}
	for (int i = 0; i < nM32; i++) {
		if (output[i] != (unsigned char)(m32->buffer[i])) {
			printf("Internal pack, unpack round-trip failed to match values");
			exit(1);
		}
	}
	
	free(output);
	return 0;
}
#endif


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

	inflateInit2(&zs, MAX_WBITS);
	int status = inflate(&zs, Z_FINISH);
	if (status != Z_STREAM_END) {
		free(output);
		return GVRSERR_BAD_COMPRESSION_FORMAT;
	}
	int inflatedLength = zs.total_out;
	int status1 = inflateEnd(&zs);
	if (status1!=Z_OK) {
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


static GvrsByte* pack(int codecIndex, int predictorIndex, int seed, GvrsM32* m32, int* packingLength, int* errCode){
	int nBytesToCompress = m32->offset;
	int nBytesForPacking = nBytesToCompress + 1024;
	GvrsByte* packing = malloc(nBytesForPacking*sizeof(GvrsByte));
	if (!packing) {
		*errCode =  GVRSERR_NOMEM;
		return 0;
	}
	
	z_stream strm;
	memset(&strm, 0, sizeof(z_stream));
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	int status = deflateInit(&strm, 6);
	if (status != Z_OK) {
		free(packing);
		 *errCode = GVRSERR_COMPRESSION_FAILED;
		 return 0;
	}

	strm.avail_in = m32->offset;
	strm.next_in = m32->buffer;
	strm.avail_out = nBytesForPacking - 10;
	strm.next_out = packing+10;
	status = deflate(&strm, Z_FINISH);
	if (status == Z_STREAM_ERROR) {
		free(packing);
		*errCode = GVRSERR_COMPRESSION_FAILED;
		return 0;
	}
	if (status != Z_STREAM_END || (int)strm.total_out>=nBytesToCompress) {
		// The packing wasn't large enough to store the full compression
		// or this compressed format was larger than the input.
		// This would happen if the data was essentially non-compressible.
		free(packing);
		*errCode = GVRSERR_COMPRESSION_FAILED;
		return 0;
	}

	status = deflateEnd(&strm);
	if (status != Z_OK) {
		free(packing);
		*errCode = GVRSERR_COMPRESSION_FAILED;
		return 0;
	}
    
	*packingLength = strm.total_out+10;
	packing[0] = (GvrsByte)codecIndex;
	packing[1] = (GvrsByte)predictorIndex;
	packing[2] = (GvrsByte)(seed & 0xff);
	packing[3] = (GvrsByte)((seed >> 8) & 0xff);
	packing[4] = (GvrsByte)((seed >> 16) & 0xff);
	packing[5] = (GvrsByte)((seed >> 24) & 0xff);
	packing[6] = (GvrsByte)((nBytesToCompress & 0xff));
	packing[7] = (GvrsByte)((nBytesToCompress >> 8) & 0xff);
	packing[8] = (GvrsByte)((nBytesToCompress >> 16) & 0xff);
	packing[9] = (GvrsByte)((nBytesToCompress >> 24) & 0xff);
	 

	//int testStatus = testUnpack(packing, *packingLength, m32);
	//if (testStatus != 0) {
	//	printf("back test for unpacking failed\n");
	//	exit(1);
	//}

	return packing;
}


static GvrsByte* encodeInt(int nRow, int nColumn,
	GvrsInt* data,
	int index,
	int* packingLength,
	int* errCode, void* appInfo) {
	*errCode = 0;
	*packingLength = 0;
	GvrsByte* packing = 0;
	GvrsInt seed;


	for (int iPack = 1; iPack <= 3; iPack++) {
		GvrsM32* m32 = 0;
		if (iPack == 1) {
			m32 = GvrsPredictor1encode(nRow, nColumn, data, &seed, errCode);
		}
		else if (iPack == 2) {
			m32 = GvrsPredictor2encode(nRow, nColumn, data, &seed, errCode);
		}
		else {
			m32 = GvrsPredictor3encode(nRow, nColumn, data, &seed, errCode);
		}
		if (*errCode || !m32) {
			GvrsM32Free(m32);
			if (packing) {
				free(packing);
				*packingLength = 0;
			}
			return 0;
		}

		int bLen;
		GvrsByte* b = pack(index, iPack, seed, m32, &bLen, errCode);
		GvrsM32Free(m32);
		if (!b || *errCode) {
			if (packing) {
				free(packing);
				*packingLength = 0;
			}
			return 0;
		}
		if (packing) {
			if (bLen < *packingLength) {
				free(packing);
				packing = b;
				*packingLength = bLen;
			}
			else {
				free(b);
			}
		}
		else {
			packing = b;
			*packingLength = bLen;
		}
	}
	
	return packing;
}


GvrsCodec* GvrsCodecDeflateAlloc() {
    GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
    GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
    codec->description = GVRS_STRDUP(description);
    codec->decodeInt = decodeInt;
	codec->encodeInt = encodeInt;
	codec->destroyCodec = destroyCodecDeflate;
	codec->allocateNewCodec = allocateCodecDeflate;
	return codec;
}
