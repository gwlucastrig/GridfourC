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
#include "GvrsPrimaryIo.h"
#include "Gvrs.h"
#include "GvrsInternal.h"
#include "GvrsError.h"




int GvrsElementReadInt(GvrsElement* element, int row, int column, GvrsInt *value) {
	if (!element) {
		GvrsError = GVRSERR_NULL_POINTER;
		return GVRSERR_NULL_POINTER;
	}
 
	int errCode, indexInTile;
	GvrsTile* tile = GvrsTileCacheFetchTile((GvrsTileCache * )element->tileCache, row, column, &indexInTile, &errCode);

 
	if (tile) {
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
	else {
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



int GvrsElementReadFloat(GvrsElement* element, int row, int column, GvrsFloat* value) {
	if (!element) {
		GvrsError = GVRSERR_NULL_POINTER;
		return GVRSERR_NULL_POINTER;
	}

	int errCode, indexInTile;
	GvrsTile* tile = GvrsTileCacheFetchTile(element->tileCache, row, column, &indexInTile, &errCode);

	if (tile) {
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
	else {
		// The tile reference is null. Usually, a null indicate that the grid cell
		// for the input row and column is not-populated (or populated with fill values).
		// In such cases, the errCode will be zero.  The appropriate action is to
		// populate the *value argument with the floating-point fill value.
		// In the uncommon event of an error, the error code will be set to a non-zero value
		// an the appropriate action is to just return it to the calling function.
		*value = element->fillValueFloat;
		return errCode;
	}

	 
}