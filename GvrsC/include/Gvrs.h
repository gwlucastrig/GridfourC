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
#include "GvrsCodec.h"
#include "GvrsMetadata.h"
 

#ifndef GVRS_H
#define GVRS_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
* Defines parametrs for a 2D affine transform using a 2-by-3 matrix
*/
typedef struct GvrsAffineTransformTag {
	GvrsDouble a00;
	GvrsDouble a01;
	GvrsDouble a02;
	GvrsDouble a10;
	GvrsDouble a11;
	GvrsDouble a12;
}GvrsAffineTransform;


/**
* Defines specifications for setting the size of the memory-use by the tile cache.
*/

typedef enum {
	GvrsTileCacheSizeSmall = 0,
	GvrsTileCacheSizeMedium = 1,
	GvrsTileCacheSizeLarge = 2,
	GvrsTileCacheSizeExtraLarge = 3,
} GvrsTileCacheSizeType;


/**
* Defines specifications for indicating the data type for a GVRS element.
*/
typedef enum {
	GvrsElementTypeInt           = 0,
	GvrsElementTypeIntCodedFloat = 1,
	GvrsElementTypeFloat         = 2,
	GvrsElementTypeShort         = 3
}GvrsElementType;

/**
* Provides range of values and default (fill) for a GVRS integer element.
*/
typedef struct {
	GvrsInt minValue;
	GvrsInt maxValue;
	GvrsInt fillValue;
}GvrsElementSpecInt;


/**
* Provides range of values and default (fill) for a GVRS integer-coded floating-point  (ICF) element.
* In the ICF format, floating-point values are scaled to integers using a scale (multiplicative) and
* offset value.
*/
typedef struct GvrsElmSpecIntCodedFloatTag {
	GvrsFloat minValue;
	GvrsFloat maxValue;
	GvrsFloat fillValue;
	GvrsFloat scale;
	GvrsFloat offset;
	GvrsInt iMinValue;
	GvrsInt iMaxValue;
	GvrsInt iFillValue;
}GvrsElementSpecIntCodedFloat;

/**
* Provides range of values and default (fill) for a GVRS floating-point element.
*/
typedef struct GvrsElmSpecFloatTag {
	GvrsFloat minValue;
	GvrsFloat maxValue;
	GvrsFloat fillValue;
}GvrsElementSpecFloat;


/**
* Provides range of values and default (fill) for a GVRS short (two-byte integer) element.
*/
typedef struct GvrsElmSpecShortTag {
	GvrsShort minValue;
	GvrsShort maxValue;
	GvrsShort fillValue;
	char pad[2];  // TO DO: is this actually needed?
}GvrsElementSpecShort;


/**
* Provides specifications for a GVRS element.
*/
typedef struct GvrsElementTag {
	GvrsElementType elementType;
	int             continuous;
	char  name[GVRS_ELEMENT_NAME_SZ+4];
	char* label;
	char* description;
	char* unitOfMeasure;
	union {
		GvrsElementSpecInt intSpec;
		GvrsElementSpecIntCodedFloat intFloatSpec;
		GvrsElementSpecFloat floatSpec;
		GvrsElementSpecShort shortSpec;
	}elementSpec;
	int typeSize;      // bytes per value of type
	int elementIndex;  // index for element from specification
	int dataOffset;    // byte offset for start of element data
	int dataSize;      // the total bytes used for the element data when uncompressed.

	int fillValueInt;
	float fillValueFloat;

	void* tileCache;
	void* gvrs;

	// The unit-to-meters conversion factor is optional.  It is intended to support
	// cases where an interpolator calculates first derivatives for a surface
	// given in geographic coordinates.  Calculations of that type often require that
	// the dependent variable (the element value) is isotropic to the underlying
	// (model) coordinates.  In the "does not apply" cases, just set this value to 1.
	double unitsToMeters;
}GvrsElement;


/**
* Provides the main data structure for access to a GVRS data store.
*/
typedef struct GvrsTag {
	char* path;
	FILE* fp;

	GvrsLong offsetToContent;  // file position of first record in file

	GvrsLong uuidLow;
	GvrsLong uuidHigh;

	GvrsLong modTimeMS;
	time_t   modTimeSec;   

	GvrsLong timeOpenedForWritingMS;

	GvrsLong filePosFreeSpaceDirectory;
	GvrsLong filePosMetadataDirectory;
	GvrsLong filePosTileDirectory;

	GvrsBoolean checksumEnabled;

	// grid and coordinate system definition

	GvrsInt rasterSpaceCode;
	GvrsInt geographicCoordinates;
	GvrsInt geoWrapsLongitude;
	GvrsInt geoBracketsLongitude;
	GvrsInt nRowsInRaster;
	GvrsInt nColsInRaster;
	GvrsInt nRowsInTile;
	GvrsInt nColsInTile;
	GvrsInt nRowsOfTiles;
	GvrsInt nColsOfTiles;

	GvrsDouble x0;
	GvrsDouble y0;
	GvrsDouble x1;
	GvrsDouble y1;
	GvrsDouble cellSizeX;
	GvrsDouble cellSizeY;
	GvrsDouble xCenter;       // xCenter and xCenterGrid are used by the Geographic coordinate
	GvrsDouble xCenterGrid;   // transforms to accound for longitude wrapping.


	GvrsAffineTransform m2r;
	GvrsAffineTransform r2m;

	GvrsInt nElementsInTupple;
	GvrsInt nBytesForTileData;
	GvrsElement** elements;

	GvrsInt nDataCompressionCodecs;
	GvrsCodec** dataCompressionCodecs;
	

	char* productLabel;

	GvrsTileCacheSizeType tileCacheSize;
	void* tileDirectory;
	void* tileCache;

	void* metadataDirectory;

} Gvrs;


/**
 * Open an existing GVRS file as a virtual raster store.
 *
 * Write access is not yet implemented.
 * 
 * This function creates a virtual raster store that includes allocated memory
 * and an open file pointer.  When a program is done using the virtual raster store,
 * it should clean up the resources using a call to GvrsClose.
 * If an error is encountered while processing the file, a null pointer will
 * be returned and the global GvrsError variable will be set with an appropriate
 * integer error code.
 * @param path the file specification.
 * @param accessMode  the mode of access; r for read; w for write; rw for both.
 * @return if successful, a pointer to a valid virtual raster store; otherwise, a null.
 */
Gvrs* GvrsOpen(const char* path, const char* accessMode);

/**
* Disposes of a GVRS virtual raster store, frees all associated memory,
* and closes the associated file.
* @param gvrs a pointer to a valid raster file store; null pointers will be ignored.
* @return a null pointer
*/
Gvrs* GvrsClose(Gvrs *gvrs);
 
/**
* Sets the size of the GVRS file cache.  Deletes the current cache and replaces
* it with one of the specified size.  The cache size of <i>Large</i> is generally
* prefered when iterating over a GVRS data set in row-major order.  It allocates
* sufficient memory to represent an entire row of tiles.  The <i>Small</i> and
* <i>Medium</i> settings allocate less memory and are suitable for localized processing.
* @param gvrs a pointer to a valid raster file store.
* @param cacheSize an enumerated type giving the size of the cache.
*/
int   GvrsSetTileCacheSize(Gvrs* gvrs, GvrsTileCacheSizeType cacheSize);

/**
* Searches a GVRS instance for an element with the specified name.
* @param gvrs a valid instance.
* @param name the name of the target instance (case sensitive).
* @return a matching element, if found; otherwise a null pointer.
*/
GvrsElement*  GvrsGetElementByName(Gvrs* gvrs, const char* name);
 
/**
* Gets the element with the matching index from the current GVRS instance.
* @param gvrs a valid instance.
* @param index a positive integer, starting from zero for the first element in the instance.
* @return a valid pointer if there is an element matching the index; otherwise a null pointer.
*/
GvrsElement*  GvrsGetElementByIndex(Gvrs* gvrs, int index);

/**
* Get the array of GVRS elements currently managed by the GVRS instance.
* @param gvrs a valid instance.
* @param nElements an integer count, set by the function.
* @return if available, a valid array of pointers to GVRS elements.
*/
GvrsElement** GvrsGetElements(Gvrs* gvrs, int* nElements);

/**
* Indicates whether an element is of an integral type.
* Includes types Int, IntCodedFloat, and Short.
* @param e a valid reference to an element
* @return non-zero (true) if integral; zero (false) if not.
*/
int GvrsElementIsIntegral(GvrsElement* element);

/**
* Reads an integer value from the GVRS file associated with the specified element.
* @param element a valid instance associated with an open GVRS file.
* @param row the row index for a grid cell within the GVRS file.
* @param column the column index for a grid cell within the GVRS file.
* @param value a pointer to an integer variable to accept the result from the read operation.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementReadInt(GvrsElement* element, int row, int column, GvrsInt* value);

/**
* Reads a floating-point value from the GVRS file associated with the specified element.
* @param element a valid instance associated with an open GVRS file.
* @param row the row index for a grid cell within the GVRS file.
* @param column the column index for a grid cell within the GVRS file.
* @param value a pointer to a floating-point variable to accept the result from the read operation.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementReadFloat(GvrsElement* element, int row, int column, GvrsFloat* value);

/**
* Transforms (maps) the specified row and column coordinates to their corresponding
* model coordinates.  The row and column may be non-integral values.
* @param gvrs a valid instance of a GVRS data store.
* @param row the real-valued grid coordinate for the point of interest.
* @param column  the real-valued grid coordinate for the point of interest.
* @param x a pointer to a real-valued model coordinate for the point of interest.
* @param y a pointer to a real-valued model coordinate for the point of interest.
*/
void GvrsMapGridToModel(Gvrs* gvrs, double row, double column, double* x, double* y);

/**
* Transforms (maps) the specified model coordinates to their corresponding
* grid (row and column) coordinates.  The row and column may be non-integral values.
* @param gvrs a valid instance of a GVRS data store.
* @param x a real-valued model coordinate for the point of interest.
* @param y  a real-valued model coordinate for the point of interest.
* @param row a pointer to a real-valued grid coordinate for the point of interest.
* @param column a pointer to a real-valued grid coordinate for the point of interest.
*/
void GvrsMapModelToGrid(Gvrs* gvrs, double x, double y, double* row, double* column);

/**
* Transforms (maps) the specified row and column coordinates to their corresponding
* geographic coordinates.  The row and column may be non-integral values.
* This method is intended for use with GVRS files that are defined with a geographic
* coordinate system.
* @param gvrs a valid instance of a GVRS data store.
* @param row the real-valued grid coordinate for the point of interest.
* @param column  the real-valued grid coordinate for the point of interest.
* @param latitude a pointer to a real-valued geographic coordinate for the point of interest, in degrees.
* @param longitude a pointer to a real-valued geographic coordinate for the point of interest, in degrees.
*/
void GvrsMapGridToGeo(Gvrs* gvrs, double row, double column, double* latitude, double* longitude);

/**
* Transforms (maps) the specified geographic coordinates (latitude and longitude) to their corresponding
* grid coordinates.  The row and column may be non-integral values.
* This method is intended for use with GVRS files that are defined with a geographic
* coordinate system.
* @param gvrs a valid instance of a GVRS data store.
* @param latitude the real-valued geographic coordinate for the point of interest, in degrees.
* @param longitude the real-valued geographic coordinate for the point of interest, in degrees.
* @param row a pointer to a real-valued grid coordinate for the point of interest.
* @param column a pointer to a real-valued grid coordinate for the point of interest.
*/
void GvrsMapGeoToGrid(Gvrs* gvrs, double latitude, double longitude, double* row, double* column);



/**
* Read the metadata elements that match the specified name and record ID.  The return value from this method
* is a pointer to a metadata result set.
* <p>
* <strong>Memory management:</strong>The memory for the result set is assumed to be under the management
* of the calling application (see GvrsMetadataResultSetFree).
* @param gvrs a valid instance of a GVRS data store.
* @param name the name of the metadata element to be read from the GVRS data store or an asterisk to indicate
* a wildcard operator.
* @param recordID the numeric record ID or an INT32_MIN to indicate a wildcard operator.
* @param errorCode if successful, set to a valid of zero; otherwise an error code.
* @result a pointer to a valid metadata result set.
*/
GvrsMetadataResultSet* GvrsMetadataReadByNameAndID(Gvrs* gvrs, const char* name, int recordID, int* errorCode);

/**
* Updates the CRC-32C checksum with the specified array of bytes. The CRC-32C checksum
* is used internally by the GVRS implementations to detect bit-errors in stored data.
*
* Most calling applications will call this function a single time on an array of bytes.
* In such cases, the input pcrc value is usually zero.  When this method is used multiple
* times over successive inputs, the return value from one call is used as the input to the next.
* @param b a valid array of bytes
* @param off the starting position within the array of bytes
* @param len the number of bytes to be read from the array
* @param pcrc is a cummulative checksum value (usually zero)
* @return the updated checksum value
*/
unsigned long  GvrsChecksumUpdateArray(GvrsByte* b, int off, int len, unsigned long pcrc);

/**
* Updates the CRC-32C checksum with the specified byte value. The CRC-32C checksum
* is used internally by the GVRS implementations to detect bit-errors in stored data.
*
* Most calling applications will call this function multiple times over a series
* of input values.  In such cases, the initial input pcrc value is usually zero.
* For subsequent calls, the return value from the previous call is used as the
* input to the next.
* @param b a single byte value
* @param pcrc is a cummulative checksum value
* @return the updated checksum value
*/
unsigned long  GvrsChecksumUpdateValue(GvrsByte b, unsigned long pcrc);

/**
* Registers a GVRS compression codec with the specified instance.  If the instance already includes
* a codec with the same name, then the previously existing codec will be freed and replaced.
* <p>
* <strong>Memory management:</strong>Management of the codec is assigned to the GVRS instance.
* Calling applications should not perform a free operation on any codec that they supply.
* Instead, the codec structure should include a pointer to a <i>destroyCodec()</i> function.
* The GvrsClose() operation will invoke the destroyCodec() function when it is invoked.
* @param gvrs a valid instance of a GVRS data store.
* @param codec a valid codec
* @return if successful, a zero; otherwise, an error code
*/
int GvrsRegisterCodec(Gvrs* gvrs, GvrsCodec* codec);

/**
* Get a formatted string from the UUID elements stored in the GVRS data store instance.
* @param gvrs a valid instance of a GVRS data store.
* @param uuidStringSize the size of the string to hold the formatted UUID;
* should be at least 36 characters including null terminator.
* @param uuidString storage to hold the formatted UUID.
*/
int GvrsGetStringUUID(Gvrs* gvrs, size_t uuidStringSize, char* uuidString);
/**
* Write a summary of the specification elements from a GVRS data store. Includes grid dimensions,
* data elements, metadata, etc.
* <p>
* To write to the console (standard output), use GvrsSummarize(gvrs, stdout);
* @param gvrs A valid GVRS data store.
* @param fp A valid output stream (file, standard output, etc).
* @return if successful, zero; otherwise an error code.
*/
int GvrsSummarize(Gvrs* gvrs, FILE* fp);

/**
* Write a summary of access statistics from the specified GVRS data store.
* @param gvrs A valid GVRS data store.
* @param fp A valid output stream (file, standard output, etc).
* @return if successful, zero; otherwise an error code.
*/
int GvrsSummarizeAccessStatistics(Gvrs* gvrs, FILE* fp);



#ifdef __cplusplus
}
#endif
 
#endif
