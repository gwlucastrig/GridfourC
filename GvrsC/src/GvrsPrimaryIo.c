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

#include "GvrsPrimaryIo.h"
#include "GvrsError.h"
 

static int errCode(FILE* fp) {
	if (feof(fp)) {
		return GVRSERR_EOF;
	}
	return GVRSERR_FILE_ACCESS;  // other file error
}

int GvrsReadASCII(FILE* fp, size_t n, size_t bufferSize, char * buffer)
{
	if (bufferSize < n) {
		size_t k = 0;
		if (bufferSize > 0) {
			k = fread(buffer, 1, bufferSize, fp);
			if (k < bufferSize) {
				return -1;
			}
			buffer[bufferSize - 1] = 0;
			// skip bytes as necessary to advance file position by n bytes
			if (fseek(fp, (long)(n - k), SEEK_CUR)) {
				return errCode(fp);
			}
		}
		return 0;
	}

	if (fread(buffer, 1, n, fp) < n) {
		return errCode(fp);
	}
	if (n < bufferSize) {
		buffer[n] = 0;
	}
	else {
		buffer[bufferSize - 1] = 0;
	}
		
	return 0;
}


int  GvrsReadByte(FILE * fp, GvrsByte * value)
{
	return fread(value, 1, 1, fp) < 1 ? errCode(fp) : 0;
}

int  GvrsReadByteArray(FILE* fp, int nValues, GvrsByte * values)
{
	size_t status = fread(values, 1, nValues, fp);
	return status < (size_t)nValues ? errCode(fp) : 0;
}


int  GvrsReadShort(FILE* fp, GvrsShort *value)
{
	return fread(value, 2, 1, fp) < 1 ? errCode(fp) : 0;
}

int  GvrsReadUnsignedShort(FILE* fp, uint16_t* value)
{
	return fread(value, 2, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsReadShortArray(FILE* fp, int n, GvrsShort* values)
{
	size_t status = fread(values, 2, n, fp);
	if (status < (size_t)n) {
		return errCode(fp);
	}
	return 0;
}

int GvrsReadInt(FILE* fp, int32_t *value)
{
	*value = 0;
	return fread(value, 4, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsReadUnsignedInt(FILE *fp, uint16_t* value)
{
	return fread(value, 4, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsReadUnsignedIntArray(FILE* fp, int n, GvrsUnsignedInt* values)
{
	size_t status = fread(values, sizeof(GvrsUnsignedInt), n, fp);
	if (status < (size_t)n) {
		return errCode(fp);
	}
	return 0;
}

int GvrsReadLong(FILE* fp, int64_t *value)
{
	*value = 0;
	return fread(value, 8, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsReadUnsignedLong(FILE* fp, uint64_t* value)
{
	return fread(value, 8, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsReadLongArray(FILE* fp, int n, GvrsLong* values)
{
	size_t status = fread(values, sizeof(GvrsLong), n, fp);
	if (status < (size_t)n) {
		return errCode(fp);
	}
	return 0;
}



int GvrsReadFloat(FILE* fp, float* value)
{
	return fread(value, 4, 1, fp) < 1 ? errCode(fp) : 0;
}

int  GvrsReadDouble(FILE* fp,  double* value)
{
	 return fread(value, 8, 1, fp) < 1 ? errCode(fp) : 0;
}

char* GvrsReadString(FILE* fp, int *errorCode)
{
	*errorCode = 0;
	uint16_t len = 0;
	size_t status = fread(&len, 2, 1, fp);
	if (status < 1) {
		*errorCode = errCode(fp);
		return 0;
	}
	char *string = calloc((size_t)len + 1, 1);
	if (!string) {
		// the allocation failed
		*errorCode =  GVRSERR_NOMEM;
		return 0;
	}

	status = fread(string, 1, len, fp);
	if (status < len) {
		free(string);
		*errorCode = errCode(fp);
		return 0;
	}
	string[len] = 0;
	return string;
}

int GvrsReadBoolean(FILE* fp, GvrsBoolean* value)
{
	uint8_t test;
	size_t status = fread(&test, 1, 1, fp);
	if (status < 1) {
		return errCode(fp);
	}
	*value = (test != 0);
	return 0;
}


int  GvrsSkipBytes(FILE* fp, int  n) {
	return fseek(fp, (long)n, SEEK_CUR);
}

int GvrsReadIdentifier(FILE* fp, size_t bufferSize, char* buffer){
	// GVRS identifiers are stored in the same format as GVRS strings:
	//    The length specification, n, as an unsigned short
	//    n bytes of text (null terminator not guaranteed)
	uint16_t n = 0;
	size_t status = fread(&n, 2, 1, fp);
	if (status < 1) {
		return errCode(fp);
	}
	if (bufferSize < n + 1) {
		return GVRSERR_FILE_ERROR;
	}
	status = GvrsReadASCII(fp, n, bufferSize, buffer);
	if (status) {
		buffer[0] = 0;
	}
	else {
		buffer[bufferSize - 1] = 0;
	}
	return (int)status;
}


int GvrsSetFilePosition(FILE* fp, GvrsLong fileOffset) {
	// This method was introduced because it appears that fseek
	// for large files (bigger than 2 Gigabytes) is not
	// consistently implemented across Windows and Linux. We will have
	// to include some conditionally compiled code when we move forward.
	if (fileOffset < 0) {
		return GVRSERR_FILE_ACCESS;
	}
	return fseek(fp, (long)fileOffset, SEEK_SET);
}
 

int GvrsWriteASCII(FILE* fp, size_t bufferSize, const char* buffer)
{
	size_t k = fwrite(buffer, 1, bufferSize, fp);
	if (k != bufferSize) {
		return k == 0 ? -1 : (int)k;
	}
	return 0;
}


int GvrsWriteBoolean(FILE* fp, GvrsBoolean  value) {
	if (value) {
		return GvrsWriteByte(fp, 1);
	}
	else {
		return GvrsWriteByte(fp, 0);
	}
}

int  GvrsWriteByte(FILE* fp, GvrsByte value)
{
	GvrsByte b = value;
	return fwrite(&b, 1, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteByteArray(FILE* fp, int nValues, GvrsByte *values) {
	return fwrite(values, 1, nValues, fp) < nValues ? errCode(fp) : 0;
}


int GvrsWriteDouble(FILE* fp, GvrsDouble value)
{
	GvrsDouble d = value;
	return fwrite(&d, 8, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteFloat(FILE* fp, GvrsFloat value)
{
	GvrsFloat f = value;
	return fwrite(&f, 4, 1, fp) < 1 ? errCode(fp) : 0;
}



int GvrsWriteInt(FILE* fp, GvrsInt value)
{
	GvrsInt i = value;
	return fwrite(&i, 4, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteLong(FILE* fp, GvrsLong value)
{
	GvrsLong i = value;
	return fwrite(&i, 8, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteShort(FILE* fp, GvrsShort value)
{
	GvrsShort i = value;
	return fwrite(&i, 2, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteUnsignedShort(FILE* fp, GvrsUnsignedShort value)
{
	GvrsUnsignedShort i = value;
	return fwrite(&i, 2, 1, fp) < 1 ? errCode(fp) : 0;
}

int GvrsWriteString(FILE* fp, const char *string)
{
	uint16_t len = 0;
	if (!string || !*string) {
		len = 0;
	}
	else {
		size_t lstr = strlen(string);
		if (lstr >= UINT16_MAX) {
			return GVRSERR_FILE_ACCESS;
		}
		len = (uint16_t)lstr;
	}
	size_t status = fwrite(&len, 2, 1, fp);
	if (status < 1) {
		return errCode(fp);
	}

	status = fwrite(string, 1, len, fp);
	if (status < len) {
		return errCode(fp);
	}
	return 0;
}


int GvrsWriteZeroes(FILE* fp, int nZeroes) {
	unsigned char zeroes[4096];
	int k = 0;
	int n = (int)sizeof(zeroes);
	memset(zeroes, 0, n);
	if (nZeroes > sizeof(zeroes)) {
		int nBlock = nZeroes / n;
		int i, status;
		for (i = 0; i < nBlock; i++) {
			status = (int)fwrite(zeroes, 1, sizeof(zeroes),  fp);
			if (status < sizeof(zeroes)) {
				return errCode(fp);
			}
			k += (int)sizeof(zeroes);
		}
	}
	 
	// writeSize will always be smaller than sizeof(zeroes).  But we check anyway
	// to satisfy Visual Studio's code analysis which will otherwise report an error.
	int writeSize = nZeroes - k;
	if (writeSize > 0 && writeSize<sizeof(zeroes)) {
		int  status = (int)fwrite(zeroes, 1, (size_t)writeSize, fp);
		if (status < writeSize) {
			return errCode(fp);
		}
	}
	return 0;
}