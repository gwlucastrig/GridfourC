#pragma once

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

#include "Gvrs.h"
#include "GvrsInternal.h"
#include "GvrsBuilder.h"
#include "GvrsPrimaryIo.h"


#ifndef GVRS_BUILDER_H
#define GVRS_BUILDER_H

#ifdef __cplusplus
extern "C"
{
#endif



	/**
	* Provides run-time data and specifications for a GVRS element structure.
	*/
	typedef struct GvrsElementSpecTag {
		GvrsElementType elementType;
		int             continuous;
		int             typeSize;

		char  name[GVRS_ELEMENT_NAME_SZ + 4];
		char* label;
		char* description;
		char* unitOfMeasure;
		union {
			GvrsElementSpecInt intSpec;
			GvrsElementSpecIntCodedFloat intFloatSpec;
			GvrsElementSpecFloat floatSpec;
			GvrsElementSpecShort shortSpec;
		}elementSpec;

		int fillValueInt;
		float fillValueFloat;
 
		// The unit-to-meters conversion factor is optional.  It is intended to support
		// cases where an interpolator calculates first derivatives for a surface
		// given in geographic coordinates.  Calculations of that type often require that
		// the dependent variable (the element value) is isotropic to the underlying
		// (model) coordinates.  In the "does not apply" cases, just set this value to 1.
		double unitsToMeters;

		void* builder;
	}GvrsElementSpec;



	typedef struct GvrsBuilderTag {
		int errorCode;


		GvrsLong uuidLow;
		GvrsLong uuidHigh;

		GvrsLong modTimeMS;
		time_t   modTimeSec;

		GvrsLong timeOpenedForWritingMS;

		GvrsLong filePosFileSpaceDirectory;
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
		GvrsInt nCellsInTile;

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


		int nElementSpecs;
		GvrsElementSpec** elementSpecs;

		GvrsInt nDataCompressionCodecs;
		GvrsCodec** dataCompressionCodecs;

	}GvrsBuilder;


	/**
	* Allocates memory for a GvrsBuilder structure and initializes data elements for
	* a grid of the specified dimensions.
	* @param builder a pointer to a pointer variable giving the address of the builder structure.
	* @param nRows number of rows in the grid, greater than or equal to 2.
	* @param nColumns number of columns in the grid, greater than or equal to 2.
	* @return if successful, zero; otherwise an integer value indicating an error condition.
	*/
	int GvrsBuilderInit(GvrsBuilder** builder, int nRows, int nColumns);
	/**
	* Sets the tile size for the grid.  The tile size is not required to be an integral divisor of the
	* grid size, though doing so provides the most storage-efficient configuration.
	* @param a valid pointer to memory that was initialized using the GvrsBuilderInit function.
	* @param nRowsInTile number of rows in a tile, greater than or equal to 2 and less than or equal to the
	* number of rows in the grid.
	* @param nColumnsInTile number of columns in a tile, greater than or equal to 2 and less than or equal to the
	* number of columns in the grid
	* @return if successful, zero; otherwise an integer value indicating an error condition.
	*/
	int GvrsBuilderSetTileSize(GvrsBuilder* builder, int nRowsInTile, int nColumnsInTile);

	/**
	* Frees all memory resources associated with the specified builder. Any GVRS instances created using
	* the builder are independent and will not be affected by this call.
	* @param builder a valid pointer to memory allocated by the GvrsBuilderInit function.
	* @return a null pointer.
	*/
	GvrsBuilder* GvrsBuilderFree(GvrsBuilder* builder);


	/**
	* Attaches a Cartesian coordinate system to the grid. The coordinates are allowed to be either
	* increasing or decreasing in the direction of the grid.
	* @param builder a valid pointer to a builder structure.
	* @param x0 the Y coordinate for the center point of the grid cell in the first column of the grid.
	* @param y0 the X coordinate for the center point of the grid cell in the first row of the grid.
 	* @param x1 the Y coordinate for the center point of the grid cell in the last column of the grid.
	* @param y1 the X coordinate for the center point of the grid cell in the last row of the grid.
	* @return if successful, a value of zero; otherwise an non-zero value indicating the error.
	*/
	int GvrsBuilderSetCartesianCoordinates(GvrsBuilder* builder, double x0, double y0, double x1, double y1);

	/**
	* Attaches a geographic coordinate system to the grid.  Latitudes are given in degrees with positive values
	* to the north of the Equator and negative valules to the source.  Longitudes are given in degrees
	* with positive values to the east of the Prime Meridian and negative values to the west.
	* Due to the cyclic nature of longitude coordinates,
	* this function must make certain assumptions about the coordinates.  In particular, it assumes that
	* longitudes are configured so that columns in the grid are arranged from west to east (left to right).
	* Longitudes are usually given in the range -180 &le; longitude &lt 180.  Usually, the values
	* lon0 is less than lon1 (because longitude is increasing to the east). In the case where the grid
	* crosses the -180 degree meridian (the International Date Line), it is possible that the value for
	* lon0 may be greater than lon1.   If both longitude specifications are the same, it is assumed that the grid
	* covers a full 360 degree range.
	* @param builder a valid pointer to a builder structure.
	* @param lat0 the latitude for the center point of the grid cell in the first row of the grid.
	* @param lon0 the longitude for the center point of the grid cell in the first column of the grid.
	* @param lat1 the latitude for the center point of the grid cell in the last row of the grid.
	* @param lon1 the longitude for the center point of the grid cell in the last column of the grid.
	* @return if successful, a value of zero; otherwise an non-zero value indicating the error.
	*/
	int GvrsBuilderSetGeographicCoordinates(GvrsBuilder* builder, double lat0, double lon0, double lat1, double lon1);

	/**
	* Sets a flag indicating whether checksums are enabled or disabled.  If enabled, checksum values
	* are computed each time a GVRS record (tile, metadata, etc.) is written to a file. Checksums
	* provide a tool for detecting transcription errors in data files stored using the GVRS API.
	* Computing checksums adds overhead to write operations and will increase the time required
	* to process data.
	* @param builder a valid pointer to a builder structure.
	* @param checksumEnabled zero to disable checksums, non-zero to enable.
	*/
	void GvrsBuilderSetChecksumEnabled(GvrsBuilder* builder, int checksumEnabled);


	int GvrsBuilderAddElementShort(GvrsBuilder* builder, const char* name, GvrsElementSpec** spec);
	int GvrsBuilderAddElementInt(GvrsBuilder* builder, const char* name, GvrsElementSpec** spec);
	int GvrsBuilderAddElementFloat(GvrsBuilder* builder, const char* name, GvrsElementSpec** spec);
	int GvrsBuilderAddElementIntCodedFloat(GvrsBuilder* builder, const char* name, GvrsFloat scale, GvrsFloat offset, GvrsElementSpec** spec);


	int GvrsElementSpecSetRangeFloat(GvrsElementSpec* eSpec, float min, float max);
	int GvrsElementSpecSetRangeInt(GvrsElementSpec* eSpec, GvrsInt iMin, GvrsInt iMax);
	int GvrsElementSpecSetFillValueFloat(GvrsElementSpec* eSpec, float fillValue);
	int GvrsElementSpecSetFillValueInt(GvrsElementSpec* eSpec, GvrsInt iFillValue);

	/**
	* Sets a flag indicating whether the data for the raster represents a continous surface
	* (a mathematical field).
	* @param eSpec a valid element specification.
	* @param continuous if continuous, a non-zero value; otherwise, a zero.
	* @return if successful, zero; otherwise a non-zero error code.
	*/
	int GvrsElementSpecSetContinuous(GvrsElementSpec* eSpec, int continuous);

	/**
	* Set a human-readable description to be added to the element specification.
	* @param eSpec a valid element specification.
	* @param description an arbitrary string describing the element.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsElementSpecSetDescription(GvrsElementSpec* eSpec, char* description);
	/**
	* Set a human-readable label to be added to the element specification.
	* A label is a short string used to identify the element in reports and user interfaces.
	* Unlike the element name, the label does not need to be unique or follow any syntax restrictions.
	* @param eSpec a valid element specification.
	* @param label an arbitrary string for labeling the element.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsElementSpecSetLabel(GvrsElementSpec* eSpec, char* label);
	/**
	* Set a unit of measure to be added to the element specification.
	* The unit of measure should be a valid, internationally accepted standard string
	* for a unit of measure.  At this time, the GVRS API does not perform any validation
	* or syntax checking on this value.   The element specification includes a separate field
	* called unitsToMeters which is not currently used, but is intended for future development.
	* @param eSpec a valid element specification.
	* @param unitOfMeasure a international-standard string giving the unit of measure for the element.
	* @return zero if successful; otherwise, an error code.
	*/
	int GvrsElementSpecSetUnitOfMeasure(GvrsElementSpec* eSpec, char* unitOfMeasure);


	/**
	* Instructs the builder to enable data compression and configure the initial set
	* of compressors to the standard codecs.  Any previously established compressors will be de-allocated.
	* Note that all compressors stored in a builder are treated as under the management
	* of the builder and should not be modified.
	* @param builder a valid reference to a builder.
	* @return if successful, a zero; otherwise an error code.
	*/
	int GvrsBuilderRegisterStandardDataCompressionCodecs(GvrsBuilder* builder);

	/**
	* Stores the specified codec in the builders configuration.  Any previously registered
	* codecs with the same identification string as the input codec will be replaced.
	* Note that all compressors stored in a builder are treated as under the management
	* of the builder and should not be modified.
	* @param builder a reference to a builder.  
	* @param codec a valid reference to a codec.
	* @return if successful, a zero; otherwise an error code.
	*/
	int GvrsBuilderRegisterDataCompressionCodec(GvrsBuilder* builder, GvrsCodec* codec);


	/**
	* Creates a new file-backed virtual raster and opens it for write access.
	* The raster is based on parameters set for the builder instance.
	* A single builder may be used to create multiple rasters.
	* <p>
	* If there was an error in a specification set for the builder, it will be disabled.
	* Calls to this function will return a null pointer.
	* @param builder a valid instance populated with settings for the desired raster.
	* @param path the path to the file to be used for the raster.
	* @param gvrs a pointer to a pointer variable to receive the address of the memory allocated when
    * the GVRS data store is opened.
	* @return zero if successful; otherwise an error code indicating the cause of the failure.
	*/
	int GvrsBuilderOpenNewGvrs(GvrsBuilder* builder, const char* path, Gvrs** gvrs);




	


#ifdef __cplusplus
}
#endif

#endif