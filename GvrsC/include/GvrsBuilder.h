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
	int GvrsBuilderSetTileSize(GvrsBuilder* builder, int nRowsInTile, int nColumnsInTile);
	GvrsBuilder* GvrsBuilderFree(GvrsBuilder* builder);

	int GvrsBuilderSetCartesianCoordinates(GvrsBuilder* builder, double x0, double y0, double x1, double y1);
	int GvrsBuilderSetGeographicCoordinates(GvrsBuilder* builder, double lat0, double lon0, double lat1, double lon1);

	void GvrsBuilderSetChecksumEnabled(GvrsBuilder* builder, int checksumEnabled);


	GvrsElementSpec* GvrsBuilderAddElementShort(GvrsBuilder* builder, const char* name);
	GvrsElementSpec* GvrsBuilderAddElementInt(GvrsBuilder* builder, const char* name);
	GvrsElementSpec* GvrsBuilderAddElementFloat(GvrsBuilder* builder, const char* name);
	GvrsElementSpec* GvrsBuilderAddElementIntCodedFloat(GvrsBuilder* builder, const char* name, GvrsFloat scale, GvrsFloat offset);


	int GvrsElementSetRangeInt(GvrsElementSpec* eSpec, GvrsInt iMin, GvrsInt iMax);
	int GvrsElementSetFillInt(GvrsElementSpec* eSpec, GvrsInt iFill);

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