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

#pragma once


#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"

#ifndef GVRS_METADATA_H
#define GVRS_METADATA_H

#ifdef __cplusplus
extern "C"
{
#endif


#define GVRS_METADATA_NAME_SZ 32
#define GVRS_ELEMENT_NAME_SZ 32

	typedef enum {
		GvrsMetadataTypeUnspecified = 0,
		GvrsMetadataTypeByte = 1,
		GvrsMetadataTypeShort = 2,
		GvrsMetadataTypeUnsignedShort = 3,
		GvrsMetadataTypeInt = 4,
		GvrsMetadataTypeUnsignedInt = 5,
		GvrsMetadataTypeFloat = 6,
		GvrsMetadataTypeDouble = 7,
		GvrsMetadataTypeString = 8,
		GvrsMetadataTypeAscii = 9
	}GvrsMetadataType;



	typedef struct GvrsMetadataTag {
		char name[GVRS_METADATA_NAME_SZ + 4];  // allow room for null terminator
		GvrsInt recordID;
		GvrsMetadataType metadataType;
		GvrsInt bytesPerValue;
		GvrsInt dataSize;
		GvrsInt nValues;
		GvrsByte* data;
	}GvrsMetadata;

	typedef struct GvrsMetadataResultSetTag {
		int nRecords;
		GvrsMetadata** records;
	}GvrsMetadataResultSet;



	/**
	* Frees the metadata result set.
	* <p>
	* <strong>Memory management:</strong> This function frees all data associated with a result set including
	* any data elements internal to the result set.  If an application has obtained pointers to these
	* internal elements (using a <i>get</i> function), the free operation will result in those pointers
	* becoming invalid.
	*
	*/
	GvrsMetadataResultSet* GvrsMetadataResultSetFree(GvrsMetadataResultSet* rs);


	/**
	* Gets the content of a metadata element as a string.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param metadata a pointer to a valid metadata element with datatype string.
	* @param errCode a pointer to an integer to store the function status; zero if successful, otherwise an error code.
	* @return a pointer to the string assocaated with the metadata.
	*/
	char* GvrsMetadataGetString(GvrsMetadata* metadata, int* errCode);

	/**
	* Gets the byte content of a metadata element.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element.
	* @param nValues the size of the data associated with the metadata.
	* @return a pointer to the byte content associated with the metadata.
	*/
	GvrsByte* GvrsMetadataGetByte(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to shorts.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype short.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of shorts
	*/
	GvrsShort* GvrsMetadataGetShort(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to unsigned shorts.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype unsigned string.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of shorts
	*/
	GvrsUnsignedShort* GvrsMetadataGetUnsignedShort(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to integers.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype integer.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of integers
	*/
	GvrsInt* GvrsMetadataGetInt(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to unsigned integers.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype unsigned integer.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of unsigned integers.
	*/
	GvrsUnsignedInt* GvrsMetadataGetUnsignedInt(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to floats.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype float.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of floats.
	*/
	GvrsFloat* GvrsMetadataGetFloat(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets the content of a metadata element as a pointer to doubles.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype double.
	* @param nValues the number of elements associated with the metadata instance.
	* @return a pointer to an array of doubles.
	*/
	GvrsDouble* GvrsMetadataGetDouble(GvrsMetadata* m, int* nValues, int* errorCode);

	/**
	* Gets a static string indicating the name of the metadata type.  Intended for writing reports
	* and diagnostic purposes.  Do not attempt to free the pointer returned by this method.
	*/
	const char* GvrsMetadataGetTypeName(GvrsMetadataType mtype);

#ifdef __cplusplus
}
#endif

#endif