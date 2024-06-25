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

#ifndef GVRS_PRIMARY_IO_H
#define GVRS_PRIMARY_IO_H

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"


#ifdef __cplusplus
extern "C"
{
#endif

/**
* Reads the specified number of characters from the source file or stream, ensuring that
* the result is null-terminated.  If the buffer size is not sufficient
* to store the specified number of characters, it will be truncated.  If the function completes successfully,
* the file position is always set forward by n characters.
* @param fp a valid pointer to a file.
* @param n the number of characters to be read from the file
* @param bufferSize the size of a buffer (array of characters) to receive the text
* @param buffer an array of at least buffer-size, intended to receive the text
* @return if successful, zero; otherwise, an error code.
*/
int GvrsReadASCII( FILE *fp, size_t n, size_t bufferSize, char* buffer);

int GvrsReadByte(     FILE *fp, GvrsByte *value);
int GvrsReadByteArray(FILE* fp, int nValues, GvrsByte* values);

int GvrsReadShort(        FILE *fp, GvrsShort *value);
//int GvrsReadUnsignedShort(FILE *fp, GvrsUnsignedShort *value);
int GvrsReadShortArray(FILE* fp, int n, GvrsShort* values);

int GvrsReadInt(        FILE *fp, int32_t *value);
int GvrsReadUnsignedInt(FILE* fp, uint16_t *value);
int GvrsReadUnsignedIntArray(FILE* fp, int n, GvrsUnsignedInt* values);

int GvrsReadLong(        FILE *fp, int64_t*          value);
int GvrsReadUnsignedLong(FILE* fp, GvrsUnsignedLong* value);
int GvrsReadLongArray(   FILE* fp, int n, GvrsLong*  values);
 
int GvrsReadFloat( FILE *fp, float *value);
int GvrsReadDouble(FILE *fp, double* value);

int GvrsReadBoolean(FILE* fp, GvrsBoolean *value);

int GvrsSkipBytes(FILE* fp, int  n);
int GvrsSetFilePosition(FILE* fp, GvrsLong fileOffset);

/**
* Reads GVRS-formatted string of arbitrary length from a source file or stream.
* Memory for the string is allocated dynamically and returned by this function as a pointer.
* It is assumed that the calling function will accept management of the memory.
* In GVRS, strings are stored using a two-byte length element followed by a corresponding
* number of bytes. Therefore the return string may be of length zero to 65535.
* If the function completes successfully,
* the file position is always set to the end of the string.
* @param fp a valid pointer to a file
* @param errCode a pointer to an integer to store the function status; zero if successful, otherwise an error code.
* @return if successful, a pointer to a valid memory address; otherwise, a null.
*/
char* GvrsReadString(FILE* fp, int *errCode);

/**
* Reads a GVRS-formatted identifier from the source file or stream, ensuring that
* the result is null-terminated.  If the buffer size is not sufficient
* to store the whole identifier, it will be truncated.  In GVRS, identifiers are
* stored using the same format a other strings: a length indicator followed by a
* corresponding number of bytes.  But GVRS identifiers are always ASCII-coded strings
* of a maximum length as specified by the GVRS format.  If the function completes successfully,
* the file position is always set to the end of the string. 
* file position is always advanced to the end
* @param fp a valid pointer to a file.
* @param bufferSize the size of a buffer (array of characters) to receive the text
* @param buffer an array of at least buffer-size, intended to receive the text
* @return zero if successful; otherwise, an error code.
*/
int GvrsReadIdentifier(FILE* fp, size_t bufferSize, char* buffer);

#ifdef __cplusplus
}
#endif


#endif
