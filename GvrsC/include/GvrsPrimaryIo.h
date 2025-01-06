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

int GvrsReadByte(     FILE *fp, uint8_t* value);
int GvrsReadByteArray(FILE* fp, int nValues, uint8_t* values);

int GvrsReadShort(        FILE *fp, int16_t* value);
//int GvrsReadUnsignedShort(FILE *fp, uint16_t* value);
int GvrsReadShortArray(FILE* fp, int n, int16_t* values);

int GvrsReadInt(        FILE *fp, int32_t* value);
int GvrsReadUnsignedInt(FILE* fp, uint32_t* value);
int GvrsReadUnsignedIntArray(FILE* fp, int n, uint32_t* values);

int GvrsReadLong(        FILE *fp, int64_t*          value);
int GvrsReadUnsignedLong(FILE* fp, uint64_t* value);
int GvrsReadLongArray(   FILE* fp, int n, int64_t*  values);
 
int GvrsReadFloat( FILE *fp, float *value);
int GvrsReadDouble(FILE *fp, double* value);

int GvrsReadBoolean(FILE* fp, int *value);

int GvrsSkipBytes(FILE* fp, int  n);
int GvrsSetFilePosition(FILE* fp, int64_t fileOffset);
int64_t GvrsGetFilePosition(FILE* fp);
int64_t GvrsFindFileEnd(FILE* fp);

/**
* Reads GVRS-formatted string of arbitrary length from a source file or stream.
* Memory for the string is allocated dynamically and the stringReference argument
* is populated with its address.  If the read operation is unsuccessful, the
* stringReference is set to a null pointer (zero address).
* It is assumed that the calling function will accept management of the memory.
* In GVRS, strings are stored using a two-byte length element followed by a corresponding
* number of bytes. Therefore the return string may be of length zero to 65535.
* If the function completes successfully, the file position is always set to the end of the string.
* @param fp a valid pointer to a file
* @param stringReference a valid pointer to a pointer of type char to receive the result.
* @return  zero if successful, otherwise an error code.
*/
int GvrsReadString(FILE* fp, char **stringReference);

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


/**
* Write the specified number of bytes to the file.  The buffer is not required
* to include a null terminator, though it is good practice to include one.
* Note that this call is different from GvrsReadString and GvrsWriteString in that
* it does not begin the output with a string-length value.
* @param fp a valid output stream or file.
* @param bufferSize the number of characters to be written
* @param buffer an array containing the characters to be written.
* @return if successful, a zero; otherwise, an error code.
*/
int GvrsWriteASCII(FILE* fp, size_t bufferSize, const char* buffer);

int GvrsWriteBoolean(FILE* fp, int  value);

int  GvrsWriteByte(FILE* fp, uint8_t value);

int GvrsWriteByteArray(FILE* fp, int nValues, uint8_t* values);

int GvrsWriteDouble(FILE* fp, double value);

int GvrsWriteFloat(FILE* fp, float value);

int GvrsWriteInt(FILE* fp, int32_t value);

int GvrsWriteLong(FILE* fp, int64_t value);

int GvrsWriteShort(FILE* fp, int16_t value);

int GvrsWriteString(FILE* fp, const char* string);

int GvrsWriteUnsignedShort(FILE* fp, uint16_t value);

int GvrsWriteZeroes(FILE* fp, int nZeroes);

#ifdef __cplusplus
}
#endif


#endif
