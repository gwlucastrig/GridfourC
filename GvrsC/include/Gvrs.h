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
#include "GvrsCrossPlatform.h"
#include "GvrsCodec.h"
#include "GvrsMetadata.h"
 

#ifndef GVRS_H
#define GVRS_H

#ifdef __cplusplus
extern "C"
{
#endif


#define GVRS_VERSION 1
#define GVRS_SUB_VERSION 4
/**
* Defines parametrs for a 2D affine transform using a 2-by-3 matrix
*/
typedef struct GvrsAffineTransformTag {
	double a00;
	double a01;
	double a02;
	double a10;
	double a11;
	double a12;
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
	int32_t minValue;
	int32_t maxValue;
	int32_t fillValue;
}GvrsElementSpecInt;


/**
* Provides range of values and default (fill) for a GVRS integer-coded floating-point  (ICF) element.
* In the ICF format, floating-point values are scaled to integers using a scale (multiplicative) and
* filePos value.
*/
typedef struct GvrsElmSpecIntCodedFloatTag {
	float minValue;
	float maxValue;
	float fillValue;
	float scale;
	float offset;
	int32_t iMinValue;
	int32_t iMaxValue;
	int32_t iFillValue;
}GvrsElementSpecIntCodedFloat;

/**
* Provides range of values and default (fill) for a GVRS floating-point element.
*/
typedef struct GvrsElmSpecFloatTag {
	float minValue;
	float maxValue;
	float fillValue;
}GvrsElementSpecFloat;


/**
* Provides range of values and default (fill) for a GVRS short (two-byte integer) element.
*/
typedef struct GvrsElmSpecShortTag {
	int16_t minValue;
	int16_t maxValue;
	int16_t fillValue;
	char pad[2];  // TO DO: is this actually needed?
}GvrsElementSpecShort;


/**
* Provides run-time data and specifications for a GVRS element structure.
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
	int dataOffset;    // byte filePos for start of element data
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

	int64_t offsetToContent;  // file position of first record in file

	int64_t uuidLow;
	int64_t uuidHigh;

	int64_t modTimeMS;
	time_t   modTimeSec;   

	int64_t timeOpenedForWritingMS;

	int64_t filePosFileSpaceDirectory;
	int64_t filePosMetadataDirectory;
	int64_t filePosTileDirectory;

	int checksumEnabled;

	// grid and coordinate system definition

	int32_t rasterSpaceCode;
	int32_t geographicCoordinates;
	int32_t geoWrapsLongitude;
	int32_t geoBracketsLongitude;
	int32_t nRowsInRaster;
	int32_t nColsInRaster;
	int32_t nRowsInTile;
	int32_t nColsInTile;
	int32_t nRowsOfTiles;
	int32_t nColsOfTiles;
	int32_t nCellsInTile;

	double x0;
	double y0;
	double x1;
	double y1;
	double cellSizeX;
	double cellSizeY;
	double xCenter;       // xCenter and xCenterGrid are used by the Geographic coordinate
	double xCenterGrid;   // transforms to accound for longitude wrapping.


	GvrsAffineTransform m2r;
	GvrsAffineTransform r2m;

	int32_t nElementsInTupple;
	int32_t nBytesForTileData;
	GvrsElement** elements;

	int32_t nDataCompressionCodecs;
	GvrsCodec** dataCompressionCodecs;
	

	char* productLabel;

	GvrsTileCacheSizeType tileCacheSize;
	void* tileDirectory;
	void* tileCache;

	void* metadataDirectory;

	void* fileSpaceManager;

	int deleteOnClose;

} Gvrs;


/**
 * Open an existing GVRS file as a virtual raster store.
 * 
 * This function creates a virtual raster store that includes allocated memory
 * and an open file pointer.  When a program is done using the virtual raster store,
 * it should clean up the resources using a call to GvrsClose.
 * If an error is encountered while processing the file, the Gvrs reference will be
 * set to null and an appropriate status value will be returned.
 * <p>
 * Although the access-mode resembles the traditional specification for the C-language's
 * fopen() function, it is limited to either "read-only" or "read-write". The GvrsOpen
 * function cannot be used to create a new GVRS data file (to do so, use the GvrsBuilder
 * functions).  A file that is opened for writing must already exist and must not currently
 * be opened by another program or application thread (writing requires exclusive access).
 * However, a file that is opened for read-only can be accessed by muliple processes simultaneously.
 * When a file is opened for writing it may be either read or write.
 * @param gvrs a pointer to a pointer varaible to receive the address of the memory allocated when
 * the GVRS data store is opened.
 * @param path the file specification.
 * @param accessMode  the mode of access; r for read; w for write.
 * @return if successful, a value of zero; otherwise an error code indicating the cause of the failure.
 */
int GvrsOpen(Gvrs** gvrs, const char* path, const char* accessMode);

/**
* Disposes of a GVRS virtual raster store, frees all associated memory,
* and closes the associated file.
* <p>
* The return status is set only when a file is opened for write-access to indicates whether
* the completion operations are successful.  When a file is opened for read-only mode,
* the return status value is not meaningful and is set to zero.
* @param gvrs a pointer to a valid raster file store; null pointers will be ignored.
* @return zero for success, otherwise non-zero.
*/
int GvrsClose(Gvrs *gvrs);
 
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
* @param element a valid reference to an element
* @return non-zero (true) if integral; zero (false) if not.
*/
int GvrsElementIsIntegral(GvrsElement* element);

/**
* Indicates whether an element is of a floating-point type.
* Includes types Float and IntCodedFloat.
* @param element a valid reference to an element
* @return non-zero (true) if floating-point; zero (false) if not.
*/
int GvrsElementIsFloat(GvrsElement* element);


/**
* Indicates whether an element represents a continous surface (a mathematical field).
* @param element a valid reference to an element
* @return non-zero (true) if floating-point; zero (false) if not.
*/
int GvrsElementIsContinuous(GvrsElement* element);



/**
* Reads an integer value from GVRS. May access the file associated with the specified element.
* For integer-coded-float elements, this method will access the integer value directly rather than
* converting it. To obtain the corresponding floating-point value, use GvrsElementReadFloat.
* @param element a valid instance associated with an open GVRS file.
* @param row the row index for a grid cell within the GVRS file.
* @param column the column index for a grid cell within the GVRS file.
* @param value a pointer to an integer variable to accept the result from the read operation.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementReadInt(GvrsElement* element, int row, int column, int32_t* value);

/**
* Reads a floating-point value from GVRS.  May access file associated with the specified element.
* @param element a valid instance associated with an open GVRS file.
* @param row the row index for a grid cell within the GVRS file.
* @param column the column index for a grid cell within the GVRS file.
* @param value a pointer to a floating-point variable to accept the result from the read operation.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementReadFloat(GvrsElement* element, int row, int column, float* value);

/**
* Writes an integer value to the GVRS store. May access the file associated with the specified
* element.   For integer-coded-float elements, this method will stored the specified integer
* value directly.  If will be converted to its corresponding floating-point value when accessed
* via the GvrsElementReadFloat routine.
* @param element a valid instance associated with an open GVRS file.
* @param gridRow the row index for a grid cell within the GVRS file.
* @param gridColumn the column index for a grid cell within the GVRS file.
* @param value an interger value to be stored in the file
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementWriteInt(GvrsElement* element, int gridRow, int gridColumn, int32_t value);
int GvrsElementWriteFloat(GvrsElement* element, int gridRow, int gridColumn, float value);

/**
* Uses the element as a counter, reads the existing value at the cell, increments it by one,
* and stores it in the raster. This operation is defined elements having a data type of either
* Integer or Short.  It is not defined for floating-point elements.
* This function can handle counts up to and not exceeding the maximum value of a signed integer.
* If the count would exceed the value of a signed integer, an error code is returned.
* @param element a valid instance associated with an GVRS file opened for write access.
* @param gridRow the row index for a grid cell within the GVRS file.
* @param gridColumn the column index for a grid cell within the GVRS file.
* @param count a pointer to an integer variable to receive the resulting count for the specified grid cell.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsElementCount(GvrsElement* element, int gridRow, int gridColumn, int32_t* count);


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
* is a pointer to a metadata result set.  The result set is detached from the GVRS data source and will
* not be modified by any subsequent actions performed on the GVRS instance.
* <p>
* <strong>Memory management:</strong>The memory for the result set is assumed to be under the management
* of the calling application (see GvrsMetadataResultSetFree).
* @param gvrs a valid instance of a GVRS data store.
* @param name the name of the metadata element to be read from the GVRS data store.
* @param recordID the numeric record ID.
* @param resultSet a pointer to a pointer for a variable to receive the result set address.
* @return if successful, zero; otherwise an integer value indicating an error condition.
*/
int GvrsReadMetadataByNameAndID(Gvrs* gvrs, const char* name, int recordID, GvrsMetadataResultSet** resultSet);


/**
* Read the metadata elements that match the specified name. The name may refer to a specific metadata
* record or may act as a wildcard operator. The return value from this method
* is a pointer to a metadata result set.  The result set is detached from the GVRS data source and will
* not be modified by any subsequent actions performed on the GVRS instance.
* <p>
* <strong>Memory management:</strong>The memory for the result set is assumed to be under the management
* of the calling application (see GvrsMetadataResultSetFree).
* @param gvrs a valid instance of a GVRS data store.
* @param name the name of the metadata element to be read from the GVRS data store, or an asterisk if
* the specification is intended to act as a wildcard operation.
* @param resultSet a pointer to a pointer for a variable to receive the result set address.
* @return if successful, zero; otherwise an integer value indicating an error condition.
*/
int GvrsReadMetadataByName(Gvrs* gvrs, const char* name, GvrsMetadataResultSet** resultSet);

/**
* Writes a metadata structure to the GVRS instance.  The metadata element is uniquely identified by its
* name and record ID elements.  If a metadata element already exists with the specified identification,
* it is replaced.
* @param gvrs a valid reference to a GVRS instance opened for writing.
* @param metadata a valid reference.
* @return if successful, zero; otherwise an integer value indicating an error condition.
*/
int GvrsMetadataWrite(Gvrs* gvrs, GvrsMetadata* metadata);

/**
* Removes the metadata record identified by the name and recordID.  If no matching record
* exists, the file is unchanged.
* @param gvrs a valid reference to a GVRS instance opened for writing.
* @param name a valid identifier indicating the name of the metadata record to be deleted.
* @param recordID the integer ID associated with the metadata record to be deleted.
* @return if successful, zero; otherwise an integer value indicating an error condition.
* 
*/
int GvrsMetadataDelete(Gvrs* gvrs, const char* name, int recordID);


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
unsigned long  GvrsChecksumUpdateArray(uint8_t* b, int off, int len, unsigned long pcrc);

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
unsigned long  GvrsChecksumUpdateValue(uint8_t b, unsigned long pcrc);

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


GvrsCodec* GvrsGetCodecByName(Gvrs* gvrs, const char* name);

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
* @return if successful, zero; otherwise, an error code.
*/
int GvrsSummarizeAccessStatistics(Gvrs* gvrs, FILE* fp);

/**
* Writes a line showing progress for a potentially time-consuming process.  The
* process is assumed to consist of a distinct number of parts that are processed
* at an approximately uniform rate. Logic is implemented to estimate
* how much time is remaining in the process and at what time it will be complete.
* If sufficient information is available, the output will include time-related
* information.
* @param fp A valid output stream (file, standard output, etc).
* @param time0 The time at which the process started or zero if unavailable.
* @param partName A label for the part to be output (i&#46;e&#46; row, block, segment, part, etc).
* @param part The number of parts completed so far.
* @param nParts The total number of parts to be completed.
* @return if successful, the number of bytes written to output; otherwise, an error code.
*/
int GvrsSummarizeProgress(FILE* fp, int64_t time0, const char* partName, int part, int nParts);

/**
* Indicates whether the specified tile is populated.
* @param gvrs A valid GVRS data store.
* @param tileIndex a positive value giving the index of the tile of interest.
* @ return 1 if populated; otherwise, zero.
*/
int GvrsIsTilePopulated(Gvrs* gvrs, int tileIndex);

/**
* Sets an option controlling whether a GVRS file will be deleted when
* the GvrsClose() operation is explicitly invoked.
* This operation applies only for files opened with write access.
* @param gvrs A valid GVRS data store.
* @param deleteOnClose non-zero if the file is to be deleted when closed; zero to suppress the deletion function.
*/
void GvrsSetDeleteOnClose(Gvrs* gvrs, int deleteOnClose);

#ifdef __cplusplus
}
#endif
 
#endif
