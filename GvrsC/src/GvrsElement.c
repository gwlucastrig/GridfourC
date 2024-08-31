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


// Development Note:
//   The "Read" functions are expected to be used in situations where they are
//   called many times in rapid succession.  As such, even small changes in their
//   code could affect run-times.  At present, the two functions do include some
//   redundant code.  When investigating ways of consolidating this code, be sure
//   to test for performance under expected operational conditions.
// 
//  Because valid row and column specifications are always positive integers, this
//  read and write functions use unsigned arithmetic to reduce the number of comparisons
//  they must make when bounds checking.   Recall that when a negative signed integer is cast
//  to an unsigned integer, it becomes a very large value. Thus the code below compactly tests
//  for row/column values being in range.   To make this work, the tc->nRowsInRaster and tc->nColsInRaster
//  are both declared as type unsigned int.
// 
// 	  (unsigned int)gridRow >= tc->nRowsInRaster || (unsigned int)gridColumn >= tc->nColsInRaster)
// 
// When reading a large, non-compressed data set under windows, this approach yielded
// a 4 percent overall improvement over the the conventional range-test shown below: 
// 
//      gridRow<0 || gridRow >= tc->nRowsInRaster [etc.]
//

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"
#include "GvrsPrimaryIo.h"
#include "Gvrs.h"
#include "GvrsInternal.h"
#include "GvrsError.h"
#include <math.h>
 

 
int GvrsElementReadInt(GvrsElement* element, int gridRow, int gridColumn, GvrsInt *value) {
	if (!element) {
		return GVRSERR_NULL_ARGUMENT;
	}

	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;

	if ((unsigned int)gridRow >= tc->nRowsInRaster || (unsigned int)gridColumn >= tc->nColsInRaster){
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}
	tc->nRasterReads++;

	int nRowsInTile = tc->nRowsInTile;
	int nColsInTile = tc->nColsInTile;
	int nColsOfTiles = tc->nColsOfTiles;
	int tileRow = gridRow / nRowsInTile;
	int tileCol = gridColumn / nColsInTile;
	int tileIndex = tileRow * nColsOfTiles + tileCol;
	int rowInTile = gridRow - tileRow * nRowsInTile;
	int colInTile = gridColumn - tileCol * nColsInTile;
	int indexInTile = rowInTile * nColsInTile + colInTile;

	int errCode;
	GvrsTile* tile;
	if (tc->firstTileIndex == tileIndex) {
		tile = tc->firstTile;
	}
	else {
		 tile = GvrsTileCacheFetchTile(tc,tileIndex, &errCode);
		 if (!tile) {
			 // The tile reference is null. Usually, a null indicate that the grid cell
			// for the input row and column is not-populated (or populated with fill values).
			// In such cases, the errCode will be zero.  The appropriate action is to
			// populate the *value argument with the integer fill value.
			// In the uncommon event of an error, the error code will be set to a non-zero value
			// an the appropriate action is to just return it to the calling function.
			 *value = element->fillValueInt;
			 return errCode;
		 }
	}
 
		GvrsByte* data = tile->data + element->dataOffset;
		switch (element->elementType) {
		case GvrsElementTypeInt:
			*value = ((int*)data)[indexInTile];
			return 0;
		case GvrsElementTypeIntCodedFloat:
			*value = ((int*)data)[indexInTile];
			return 0;
		case GvrsElementTypeFloat:
			*value = (int)(((float*)data)[indexInTile]);
			return 0;
		case GvrsElementTypeShort:
			*value = (int)(((short*)data)[indexInTile]);
			return 0;
		default:
			*value = element->fillValueInt;
			return GVRSERR_FILE_ERROR;
		}
}


int  GvrsElementReadFloat(GvrsElement* element, int gridRow, int gridColumn, GvrsFloat* value) {
	if (!element) {
		return GVRSERR_NULL_ARGUMENT;
	}

	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;
	if ((unsigned int)gridRow >= tc->nRowsInRaster || (unsigned int)gridColumn >= tc->nColsInRaster) {
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}
	tc->nRasterReads++;

	int nRowsInTile = tc->nRowsInTile;
	int nColsInTile = tc->nColsInTile;
	int nColsOfTiles = tc->nColsOfTiles;
	int tileRow = gridRow / nRowsInTile;
	int tileCol = gridColumn / nColsInTile;
	int tileIndex = tileRow * nColsOfTiles + tileCol;
	int rowInTile = gridRow - tileRow * nRowsInTile;
	int colInTile = gridColumn - tileCol * nColsInTile;
	int indexInTile = rowInTile * nColsInTile + colInTile;

	int errCode;
	GvrsTile* tile;
	if (tc->firstTileIndex == tileIndex) {
		tile = tc->firstTile;
	}
	else {
		tile = GvrsTileCacheFetchTile(tc,  tileIndex, &errCode);
		if (!tile) {
			// The tile reference is null. Usually, a null indicate that the grid cell
		   // for the input row and column is not-populated (or populated with fill values).
		   // In such cases, the errCode will be zero.  The appropriate action is to
		   // populate the *value argument with the integer fill value.
		   // In the uncommon event of an error, the error code will be set to a non-zero value
		   // an the appropriate action is to just return it to the calling function.
			*value = element->fillValueFloat;
			return errCode;
		}
	}

	GvrsByte* data = tile->data + element->dataOffset;
	switch (element->elementType) {
	case GvrsElementTypeInt:
		*value = (float)(((int*)data)[indexInTile]);
		return 0;
	case GvrsElementTypeIntCodedFloat:
	{
		GvrsElementSpecIntCodedFloat s = element->elementSpec.intFloatSpec;
		int i = ((int*)data)[indexInTile];
		if (i == s.iFillValue) {
			*value = s.fillValue;
		}
		else {
			*value = i / s.scale + s.offset;
		}
	}
	return 0;
	case GvrsElementTypeFloat:
		*value = ((float*)data)[indexInTile];
		return 0;
	case GvrsElementTypeShort:
		*value = (float)(((short*)data)[indexInTile]);
		return 0;
	default:
		*value = element->fillValueFloat;
		return GVRSERR_FILE_ERROR;
	}
}

int GvrsElementIsFloat(GvrsElement* element) {
	return element->elementType == GvrsElementTypeFloat;
}

int GvrsElementIsContinuous(GvrsElement* element)
{
	return element->continuous;
}

int GvrsElementIsIntegral(GvrsElement * element) {
	switch (element->elementType) {
	case GvrsElementTypeInt:
		return 1;
	case GvrsElementTypeIntCodedFloat:
		return 1;
	case GvrsElementTypeShort:
		return 1;
	default:
		return 0;
	}
}


void
GvrsElementFillData(GvrsElement* element, GvrsByte* data, int nCells) {
	//GvrsByte* data = tile->data + element->dataOffset;
	int i;
	switch (element->elementType) {
	case GvrsElementTypeInt: {
		int iFillValue = element->elementSpec.intSpec.fillValue;
		int* iData = (int*)data;
		for (i = 0; i < nCells; i++) {
			iData[i] = iFillValue;
		}
		return;
	}
	case GvrsElementTypeIntCodedFloat: {
		int iFillValue = element->elementSpec.intFloatSpec.iFillValue;
		int* iData = (int*)data;
		for (i = 0; i < nCells; i++) {
			iData[i] = iFillValue;
		}
		return;
	}
	case GvrsElementTypeFloat: {
		float fFillValue = element->elementSpec.floatSpec.fillValue;
		float* fData = (float*)data;
		for (i = 0; i < nCells; i++) {
			fData[i] = fFillValue;
		}
		return;
	}
	case GvrsElementTypeShort: {
		short sFillValue = element->elementSpec.shortSpec.fillValue;
		short* sData = (short*)data;
		for (i = 0; i < nCells; i++) {
			sData[i] = sFillValue;
		}
		return;
	}
	default:
		return; // we should never get here!
	}
}
 



int GvrsElementWriteInt(GvrsElement* element, int gridRow, int gridColumn, GvrsInt value) {
	if (!element) {
		return GVRSERR_NULL_ARGUMENT;
	}

	Gvrs* gvrs = element->gvrs;
	if (gvrs) {
		if (!gvrs->timeOpenedForWritingMS) {
			return GVRSERR_NOT_OPENED_FOR_WRITING;
		}
	}
	else {
		return GVRSERR_NULL_ARGUMENT;
	}


	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;
	if ((unsigned int)gridRow >= tc->nRowsInRaster || (unsigned int)gridColumn >= tc->nColsInRaster) {
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}
	tc->nRasterWrites++;
	int nRowsInTile = tc->nRowsInTile;
	int nColsInTile = tc->nColsInTile;
	int nColsOfTiles = tc->nColsOfTiles;
	int tileRow = gridRow / nRowsInTile;
	int tileCol = gridColumn / nColsInTile;
	int tileIndex = tileRow * nColsOfTiles + tileCol;
	int rowInTile = gridRow - tileRow * nRowsInTile;
	int colInTile = gridColumn - tileCol * nColsInTile;
	int indexInTile = rowInTile * nColsInTile + colInTile;

	int errCode;
	GvrsTile* tile;
	if (tc->firstTileIndex == tileIndex) {
		tile = tc->firstTile;
	}
	else {
		tile = GvrsTileCacheFetchTile(tc,  tileIndex, &errCode);
		if (!tile) {
			// The tile reference is null. Usually, a null indicate that the grid cell
			// for the input row and column is not-populated (or populated with fill values).
			// In such cases, the errCode will be zero.  The appropriate action is to
			// populate the *value argument with the integer fill value.
			// In the uncommon event of an error, the error code will be set to a non-zero value
			// an the appropriate action is to just return it to the calling function.
			if (errCode != 0) {
				return errCode;
			}
			tile = GvrsTileCacheStartNewTile(tc, tileIndex, &errCode);
			if (errCode) {
				return errCode;
			}

		}
	}

	tile->writePending = 1;
	GvrsByte* data = tile->data + element->dataOffset;
	switch (element->elementType) {
	case GvrsElementTypeInt:
	     ((int*)data)[indexInTile] = value;
		 return 0;
	case GvrsElementTypeIntCodedFloat:
		// NOTE when we write a float, we need to convert it to an integer value,
		// but when we write an integer, we simply replace the integer-coded value
	    ((int*)data)[indexInTile] = value;
		return 0;
	case GvrsElementTypeFloat:
		 ((float*)data)[indexInTile] = (float)value;
		return 0;
	case GvrsElementTypeShort:
		((short*)data)[indexInTile] = (short)value;
		return 0;
	default:
		return GVRSERR_FILE_ERROR;
	}
}



int GvrsElementWriteFloat(GvrsElement* element, int gridRow, int gridColumn, GvrsFloat value) {
	if (!element) {
		return GVRSERR_NULL_ARGUMENT;
	}

	Gvrs* gvrs = element->gvrs;
	if (gvrs) {
		if (!gvrs->timeOpenedForWritingMS) {
			return GVRSERR_NOT_OPENED_FOR_WRITING;
		}
	}
	else {
		return GVRSERR_NULL_ARGUMENT;
	}


	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;

	if ((unsigned int)gridRow >= tc->nRowsInRaster || (unsigned int)gridColumn >= tc->nColsInRaster) {
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}
	tc->nRasterWrites++;

	int nRowsInTile = tc->nRowsInTile;
	int nColsInTile = tc->nColsInTile;
	int nColsOfTiles = tc->nColsOfTiles;
	int tileRow = gridRow / nRowsInTile;
	int tileCol = gridColumn / nColsInTile;
	int tileIndex = tileRow * nColsOfTiles + tileCol;
	int rowInTile = gridRow - tileRow * nRowsInTile;
	int colInTile = gridColumn - tileCol * nColsInTile;
	int indexInTile = rowInTile * nColsInTile + colInTile;

	int errCode;
	GvrsTile* tile;
	if (tc->firstTileIndex == tileIndex) {
		tile = tc->firstTile;
	}
	else {
		tile = GvrsTileCacheFetchTile(tc,  tileIndex, &errCode);
		if (!tile) {
			// The tile reference is null. Usually, a null indicate that the grid cell
			// for the input row and column is not-populated (or populated with fill values).
			// In such cases, the errCode will be zero.  The appropriate action is to
			// populate the *value argument with the integer fill value.
			// In the uncommon event of an error, the error code will be set to a non-zero value
			// an the appropriate action is to just return it to the calling function.
			if (errCode != 0) {
				return errCode;
			}
			tile = GvrsTileCacheStartNewTile(tc, tileIndex, &errCode);
			if (errCode) {
				return errCode;
			}

		}
	}

	tile->writePending = 1;
	GvrsByte* data = tile->data + element->dataOffset;
	switch (element->elementType) {
	case GvrsElementTypeInt:
		((int*)data)[indexInTile] = (int)value;
		return 0;
	case GvrsElementTypeIntCodedFloat:
	{
		int i;
		GvrsElementSpecIntCodedFloat s = element->elementSpec.intFloatSpec;
		if (isnan(value) && isnan(s.fillValue)) {
			i = s.iFillValue;
		}
		else {
			i = (int)(value * s.scale - s.offset);
		}
		((int*)data)[indexInTile] = i;
		return 0;
	}
	case GvrsElementTypeFloat:
		((float*)data)[indexInTile] = value;
		return 0;
	case GvrsElementTypeShort:
		((short*)data)[indexInTile] = (short)value;
		return 0;
	default:
		return GVRSERR_FILE_ERROR;
	}
}

