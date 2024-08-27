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
#include "GvrsError.h"
#include "Gvrs.h"
#include "GvrsInternal.h"


static GvrsTileDirectory* readFailed(GvrsTileDirectory* td, int status, int* errCode) {
	GvrsTileDirectoryFree(td);
	*errCode = status;
	return 0;
}

GvrsTileDirectory* GvrsTileDirectoryAllocEmpty(int nRowsOfTiles, int nColsOfTiles, int *errCode) {
	GvrsTileDirectory* td = calloc(1, sizeof(GvrsTileDirectory));
	if (!td) {
		return readFailed(td, GVRSERR_NOMEM, errCode);
	}
	td->nRowsOfTiles = nRowsOfTiles;
	td->nColsOfTiles = nColsOfTiles;
	return td;
}

GvrsTileDirectory* GvrsTileDirectoryRead(Gvrs* gvrs, GvrsLong filePosTileDirectory, int* errCode) {
	FILE* fp = gvrs->fp;
	*errCode = 0;
	if (filePosTileDirectory == 0) {
		return GvrsTileDirectoryAllocEmpty(gvrs->nRowsOfTiles, gvrs->nColsOfTiles, errCode);
	}

	GvrsTileDirectory* td = 0;
	int status = GvrsSetFilePosition(fp, filePosTileDirectory);
	if (status) {
		return readFailed(td, status, errCode);
	}
	// the first element in the tile directory is a set of 8 bytes:
	//    0:        directory format, currently always set to zero
	//    1:        boolean indicating if extended file offsets are used
	//    2 to 7:   Reservd for future use

	GvrsByte tileDirectoryFormat;
	GvrsBoolean useExtendedFileOffset;
	GvrsReadByte(fp, &tileDirectoryFormat);
	GvrsReadBoolean(fp, &useExtendedFileOffset);
	if (tileDirectoryFormat != 0) {
		*errCode = GVRSERR_INVALID_FILE;
		return 0;
	}
	GvrsSkipBytes(fp, 6); // reserved for future use
	td = calloc(1, sizeof(GvrsTileDirectory));
	if (!td) {
		return readFailed(td, GVRSERR_NOMEM, errCode);
	}

	td->nRowsOfTiles = gvrs->nRowsOfTiles;
	td->nColsOfTiles = gvrs->nColsOfTiles;

	GvrsReadInt(fp, &td->row0);
	GvrsReadInt(fp, &td->col0);
	GvrsReadInt(fp, &td->nRows);
	GvrsReadInt(fp, &td->nCols);
	td->row1 = td->row0 + td->nRows - 1;
	td->col1 = td->col0 + td->nCols - 1;
	int nTilesInTable = td->nRows * td->nCols;

	if (useExtendedFileOffset) {
		td->lOffsets = calloc(nTilesInTable, sizeof(GvrsLong));
		if (!td->iOffsets) {
			return readFailed(td, GVRSERR_NOMEM, errCode);
		}
		status = GvrsReadLongArray(fp, nTilesInTable, td->lOffsets);
	}
	else {
		td->iOffsets = calloc(nTilesInTable, sizeof(GvrsUnsignedInt));
		if (!td->iOffsets) {
			return readFailed(td, GVRSERR_NOMEM, errCode);
		}
		status = GvrsReadUnsignedIntArray(fp, nTilesInTable, td->iOffsets);
	}

	return td;
}


GvrsTileDirectory* GvrsTileDirectoryFree(GvrsTileDirectory* tileDirectory) {
	if (tileDirectory) {
		if (tileDirectory->iOffsets) {
			free(tileDirectory->iOffsets);
			tileDirectory->iOffsets = 0;
		}
		else if (tileDirectory->lOffsets) {
			free(tileDirectory->lOffsets);
			tileDirectory->lOffsets = 0;
		}
		free(tileDirectory);
	}
	return 0;
}



GvrsLong GvrsTileDirectoryGetFilePositionByRowColumn(GvrsTileDirectory* tileDir, int tileRow, int tileCol) {
	if (tileRow < tileDir->row0 || tileCol < tileDir->col0) {
		return 0;
	}
	int iRow = tileRow - tileDir->row0;
	int iCol = tileCol - tileDir->col0;
	if (iRow >= tileDir->nRows || iCol >= tileDir->nCols) {
		return 0;
	}
	int tileIndex = iRow * tileDir->nCols + iCol;
	if (tileDir->iOffsets) {
		GvrsLong t = (GvrsLong)(tileDir->iOffsets[tileIndex]);
		return t << 3;
	}
	else if (tileDir->lOffsets) {
		return tileDir->lOffsets[tileIndex];
	}else{
		// This is a new file and the tile directory has not yet been populated
		return 0;
	}
}



GvrsLong GvrsTileDirectoryGetFilePosition(GvrsTileDirectory* tileDir, int tileIndex) {
	int tileRow = tileIndex / tileDir->nColsOfTiles;
	int tileCol = tileIndex % tileDir->nColsOfTiles;

	if (tileRow < tileDir->row0 || tileCol < tileDir->col0) {
		return 0;
	}
	int iRow = tileRow - tileDir->row0;
	int iCol = tileCol - tileDir->col0;
	if (iRow >= tileDir->nRows || iCol >= tileDir->nCols) {
		return 0;
	}
	if (tileDir->iOffsets) {
		GvrsLong t = (GvrsLong)(tileDir->iOffsets[tileIndex]);
		return t << 3;
	}
	else if (tileDir->lOffsets) {
		return tileDir->lOffsets[tileIndex];
	}
	else {
		// We should never get here.  Perhaps this is a new file
		// and the tile directory has not yet been populated
		return 0;
	}
}


GvrsLong GvrsTileDirectoryWrite(Gvrs* gvrs, int* errorCode) {
	//GvrsInt row0;
	//GvrsInt col0;
	//GvrsInt row1;
	//GvrsInt col1;
	//GvrsInt nRows;
	//GvrsInt nCols;
	//GvrsInt nRowsOfTiles;
	//// one of the following pointers should be set.  iOffsets when
	//// compact references are used.  lOffsets for extended references.
	//GvrsUnsignedInt* iOffsets;
	// GvrsLong* lOffsets;

	int iRow, iCol;
	*errorCode = 0;

	FILE* fp = gvrs->fp;
	GvrsTileDirectory* td = gvrs->tileDirectory;
	int nTileCells = td->nRows * td->nCols;
	int cellSize;
	GvrsBoolean extendedAddressSpace;
	if (td->iOffsets) {
		extendedAddressSpace = 0;
		cellSize = 4;
	}
	else {
		extendedAddressSpace = 1;
		cellSize = 8;
	}
	int sizeTileDirectory = 8 + 16 + nTileCells * cellSize;
	int status;
	GvrsLong posToStore;
	status = GvrsFileSpaceAlloc(gvrs->fileSpaceManager, GvrsRecordTypeTileDir, sizeTileDirectory, &posToStore);
	if (status) {
		*errorCode = status;
		return 0;
	}
	GvrsWriteByte(fp, 0);  // Version of tile directory, currently only zero is implemented
	GvrsWriteBoolean(fp, extendedAddressSpace); // extended address space
	GvrsWriteZeroes(fp, 6); // reserved for future use

	GvrsWriteInt(fp, td->row0);
	GvrsWriteInt(fp, td->col0);
	GvrsWriteInt(fp, td->nRows);
	GvrsWriteInt(fp, td->nCols);
	if (td->iOffsets) {
		int k = 0;
		for (iRow = 0; iRow < td->nRows; iRow++) {
			for (iCol = 0; iCol < td->nCols; iCol++) {
				GvrsWriteInt(fp, td->iOffsets[k++]);
			}
		}
	}
	else if (td->lOffsets) {
		int k = 0;
		for (iRow = 0; iRow < td->nRows; iRow++) {
			for (iCol = 0; iCol < td->nCols; iCol++) {
				GvrsWriteLong(fp, td->lOffsets[k++]);
			}
		}
	}

	if (GvrsFileSpaceFinish(gvrs->fileSpaceManager, posToStore)) {
		*errorCode = GVRSERR_FILE_ERROR;
		return 0;
	}

	return posToStore;
}


int GvrsTileDirectoryRegisterFilePosition(GvrsTileDirectory* td, GvrsInt tileIndex, GvrsLong filePosition) {
	// TO DO: test for offset greater than 32 GB threshold that is to big for iOffset and requires lOffset  
	int row = tileIndex / td->nColsOfTiles;
	int col = tileIndex - row * td->nColsOfTiles;
	if (td->nCols == 0) {
		// the tile directory is empty
		td->nRows = 1;
		td->nCols = 1;
		td->row0 = row;
		td->col0 = col;
		td->row1 = row;
		td->iOffsets = calloc(1, sizeof(GvrsUnsignedInt));
		td->iOffsets[0] = (GvrsUnsignedInt)(filePosition >> 3);
	}
	else {
		int adjustmentNeeded = 0;
		int nRows = td->nRows;;
		int nCols = td->nCols;
		int col0 = td->col0;
		int row0 = td->row0;
		int nRowsX = td->nRows;;
		int nColsX = td->nCols;
		int col0X = td->col0;
		int row0X = td->row0;
		int row1X = td->row0 + td->nRows - 1;
		int col1X = td->col0 + td->nCols - 1;
	 


		if (row < row0X) {
			adjustmentNeeded = 1;
			row0X= row;
		} if (row >= row1X) {
			adjustmentNeeded = 1;
			row1X = row;
		}
		if (col < col0X) {
			adjustmentNeeded = 1;
			col0X = col;
		} if (col >= col1X) {
			adjustmentNeeded = 1;
			col1X = col;
		}

	

		if (adjustmentNeeded) {
			nRowsX = row1X - row0X + 1;
			nColsX = col1X - col0X + 1;
			int n = nRowsX * nColsX;
			if (td->iOffsets) {
				GvrsUnsignedInt* iOffsets = td->iOffsets;
				GvrsUnsignedInt* xOffsets = calloc(n, sizeof(GvrsUnsignedInt));
				if (!xOffsets) {
					return GVRSERR_NOMEM;
				}
				// TO DO:  replace col loop with memmove?  memcpy?
				for (int iRow = 0; iRow < nRows; iRow++) {
					int iRowOffset = iRow * nCols; // position of tile position in original grid
					int xRowOffset = (iRow + row0 - row0X) * nColsX+(col0-col0X); // position in extended grid
					for (int iCol = 0; iCol < nCols; iCol++) {
						xOffsets[xRowOffset + iCol] = iOffsets[iRowOffset + iCol];
					}
				}
				free(td->iOffsets);
				td->iOffsets = xOffsets;
			}
			else {
				GvrsLong* lOffsets = td->lOffsets;
				GvrsLong* xOffsets = calloc(n, sizeof(GvrsLong));
				if (!xOffsets) {
					return GVRSERR_NOMEM;
				}
				// TO DO:  replace col loop with memmove?  memcpy?  
				for (int iRow = 0; iRow < nRows; iRow++) {
					int iRowOffset = iRow * nCols; // position of tile position in original grid
					int xRowOffset = (iRow + row0 - row0X) * nColsX + (col0 - col0X); // position in extended grid
					for (int iCol = 0; iCol < nCols; iCol++) {
						xOffsets[xRowOffset + iCol] = lOffsets[iRowOffset + iCol];
					}
				}
				free(td->lOffsets);
				td->lOffsets = xOffsets;
			}
			td->row0 = row0X;
			td->col0 = col0X;
			td->row1 = row1X;
			td->col1 = col1X;
			td->nRows = nRowsX;
			td->nCols = nColsX;
		}
		int index = (row - td->row0) * td->nCols + (col - td->col0);
		if (td->iOffsets) {
			td->iOffsets[index] = (GvrsUnsignedInt)(filePosition >> 3);
		}
		else {
			td->lOffsets[index] = filePosition;
		}
	}

	return 0;

}
