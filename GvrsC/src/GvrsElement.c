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

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"
#include "GvrsPrimaryIo.h"
#include "Gvrs.h"
#include "GvrsInternal.h"
#include "GvrsError.h"
 

 
int GvrsElementReadInt(GvrsElement* element, int gridRow, int gridColumn, GvrsInt *value) {
	if (!element) {
		GvrsError = GVRSERR_NULL_POINTER;
		return GVRSERR_NULL_POINTER;
	}

	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;
	int nRowsInRaster = tc->nRowsInRaster;
	int nColsInRaster = tc->nColsInRaster;

	if (gridRow < 0 || gridRow >= nRowsInRaster || gridColumn < 0 || gridColumn >= nColsInRaster) {
		GvrsError = GVRSERR_COORDINATE_OUT_OF_BOUNDS;
		return GvrsError;
	}

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
		 tile = GvrsTileCacheFetchTile(tc, tileRow, tileCol, tileIndex, &errCode);
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
		GvrsError = GVRSERR_NULL_POINTER;
		return GVRSERR_NULL_POINTER;
	}

	GvrsTileCache* tc = (GvrsTileCache*)element->tileCache;
	int nRowsInRaster = tc->nRowsInRaster;
	int nColsInRaster = tc->nColsInRaster;

	if (gridRow < 0 || gridRow >= nRowsInRaster || gridColumn < 0 || gridColumn >= nColsInRaster) {
		GvrsError = GVRSERR_COORDINATE_OUT_OF_BOUNDS;
		return GvrsError;
	}

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
		tile = GvrsTileCacheFetchTile(tc, tileRow, tileCol, tileIndex, &errCode);
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
 