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
	* @param string a pointer to a pointer variable that will receive the address of the string
	* @return zero if successful, otherwise an error code.
	*/
	int GvrsMetadataGetString(GvrsMetadata* metadata, char **string);

	/**
	* Gets the content of a metadata element as a pointer to an array of one or more bytes.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element.
	* @param nValues the size of the data associated with the metadata.
	* @param byte a pointer to a pointer variable that will receive the address of the metadata.
	* @return zero if successful; otherwise, an error code.
	*/
	 int GvrsMetadataGetByteArray(GvrsMetadata* m, int* nValues, GvrsByte** bytes);

	/**
	* Gets the content of a metadata element as a pointer to an array of one or more short integers.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype short.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more short integers.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetShortArray(GvrsMetadata* m, int* nValues, GvrsShort** data);


	/**
	* Gets the content of a metadata element as a pointer to an array of one or more unsigned short integerss.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype unsigned short.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more unsigned short integers.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetUnsignedShortArray(GvrsMetadata* m, int* nValues, GvrsUnsignedShort** data);

	/**
	* Gets the content of a metadata element as a pointer to an array of one or more integers.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype integer.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more integers.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetIntArray(GvrsMetadata* m, int* nValues, GvrsInt** data );


	/**
	* Gets the content of a metadata element as a pointer to an array of one or more unsigned integers.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype unsigned integer.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more unsigned integers.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetUnsignedIntArray(GvrsMetadata* m, int* nValues, GvrsUnsignedInt** data);




	/**
	* Gets the content of a metadata element as a pointer to an array of one or more floats.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype float.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more floats.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetFloatArray(GvrsMetadata* m, int* nValues, GvrsFloat** data);




	/**
	* Gets the content of a metadata element as a pointer to an array of one or more doubles.
	* <p>
	* <strong>Memory management:</strong> A metadata <i>get</i> function returns a pointer to an internal
	* memory location within the metadata element.  When an application calls the metadata-result free function,
	* those pointers become invalid and should not be de-referenced.
	* @param m a pointer to a valid metadata element with datatype double.
	* @param nValues the number of elements associated with the metadata instance.
	* @param data a pointer to a pointer variable that will receive the address of one or more doubles.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsMetadataGetDoubleArray(GvrsMetadata* m, int* nValues, GvrsDouble** data);

	/**
	* Gets a static string indicating the name of the metadata type.  Intended for writing reports
	* and diagnostic purposes.  Do not attempt to free the pointer returned by this method.
	*/
	const char* GvrsMetadataGetTypeName(GvrsMetadataType mtype);

#ifdef __cplusplus
}
#endif

#endif