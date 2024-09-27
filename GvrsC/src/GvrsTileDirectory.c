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


static int readFailed(GvrsTileDirectory* td, int status) {
	GvrsTileDirectoryFree(td);
	return status;
}

int
GvrsTileDirectoryAllocEmpty(int nRowsOfTiles, int nColsOfTiles, GvrsTileDirectory** tileDirectoryReference) {
	if (!tileDirectoryReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*tileDirectoryReference = 0;
	GvrsTileDirectory* td = calloc(1, sizeof(GvrsTileDirectory));
	if (!td) {
		return GVRSERR_NOMEM;
	}
	td->nRowsOfTiles = nRowsOfTiles;
	td->nColsOfTiles = nColsOfTiles;
	*tileDirectoryReference = td;
	return 0;
}

int
GvrsTileDirectoryRead(Gvrs* gvrs, GvrsLong filePosTileDirectory, GvrsTileDirectory** tileDirectoryReference) {
	if (!gvrs || !tileDirectoryReference) {
		return GVRSERR_NULL_ARGUMENT;
	}

	int status;
	FILE* fp = gvrs->fp;
	if (!fp) {
		return GVRSERR_FILE_ERROR;
	}

	if (filePosTileDirectory == 0) {
		return GvrsTileDirectoryAllocEmpty(gvrs->nRowsOfTiles, gvrs->nColsOfTiles, tileDirectoryReference);
	}

	*tileDirectoryReference = 0;
	GvrsTileDirectory* td = 0;
	status = GvrsSetFilePosition(fp, filePosTileDirectory);
	if (status) {
		return readFailed(td, status);
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
		return GVRSERR_INVALID_FILE;
	}

	GvrsSkipBytes(fp, 6); // reserved for future use
	td = calloc(1, sizeof(GvrsTileDirectory));
	if (!td) {
		return readFailed(td, GVRSERR_NOMEM);
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
	if (nTilesInTable == 0) {
		// the table is empty, do not allocate memory for the offsets
		// return with successful completion
		*tileDirectoryReference = td;
		return 0;
	}

	if (useExtendedFileOffset) {
		td->lOffsets = calloc(nTilesInTable, sizeof(GvrsLong));
		if (!td->iOffsets) {
			return readFailed(td, GVRSERR_NOMEM);
		}
		status = GvrsReadLongArray(fp, nTilesInTable, td->lOffsets);
	}
	else {
		td->iOffsets = calloc(nTilesInTable, sizeof(GvrsUnsignedInt));
		if (!td->iOffsets) {
			return readFailed(td, GVRSERR_NOMEM);
		}
		status = GvrsReadUnsignedIntArray(fp, nTilesInTable, td->iOffsets);
	}
	if (status) {
		return readFailed(td, status);
	}
	*tileDirectoryReference = td;
	return 0;
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
	int offsetTableIndex = iRow * tileDir->nCols + iCol;
	if (tileDir->iOffsets) {
		GvrsLong t = (GvrsLong)(tileDir->iOffsets[offsetTableIndex]);
		return t << 3;
	}
	else if (tileDir->lOffsets) {
		return tileDir->lOffsets[offsetTableIndex];
	}
	else {
		// We should never get here.  Perhaps this is a new file
		// and the tile directory has not yet been populated
		return 0;
	}
}


int GvrsTileDirectoryWrite(Gvrs* gvrs, GvrsLong* tileDirectoryPos) {
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

	if (!tileDirectoryPos) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*tileDirectoryPos = 0;

	int iRow, iCol;

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
		return status;
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
		return  GVRSERR_FILE_ERROR;
	}

	*tileDirectoryPos = posToStore;
	return 0;
}


int GvrsTileDirectoryRegisterFilePosition(GvrsTileDirectory* td, GvrsInt tileIndex, GvrsLong filePosition) {
	// Test for filePos greater than 32 GB threshold that is too big for iOffset and requires lOffset 
	if (filePosition >= (1LL << 35) && td->iOffsets) {
		// The "compact" representation stores file position in a 4-byte unsigned integer.
		// Four bytes can represent a value up to 4 GB (32 bits, unsigned). But GVRS file records
		// always start on a file position that is a multiple of 8. So a file position of up to 32 GB (1<<(32+3))
		// can be stored by dividing its value by 8.  If the input file position is lardger than that,
		// the API must switch to the 8-byte representation
		int i;
		int n = td->nRows * td->nCols;
		td->lOffsets = (GvrsLong *)malloc(n);
		if (!td->lOffsets) {
			return GVRSERR_NOMEM;
		}
		for (i = 0; i < n; i++) {
			td->lOffsets[i] = ((GvrsLong)td->iOffsets[i]) << 3;
		}
		free(td->iOffsets);
		td->iOffsets = 0;
	}
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
				int iRow, iCol;
				GvrsUnsignedInt* iOffsets = td->iOffsets;
				GvrsUnsignedInt* xOffsets = calloc(n, sizeof(GvrsUnsignedInt));
				if (!xOffsets) {
					return GVRSERR_NOMEM;
				}
				// TO DO:  replace col loop with memmove?  memcpy?
				for (iRow = 0; iRow < nRows; iRow++) {
					int iRowOffset = iRow * nCols; // position of tile position in original grid
					int xRowOffset = (iRow + row0 - row0X) * nColsX+(col0-col0X); // position in extended grid
					for (iCol = 0; iCol < nCols; iCol++) {
						xOffsets[xRowOffset + iCol] = iOffsets[iRowOffset + iCol];
					}
				}
				free(td->iOffsets);
				td->iOffsets = xOffsets;
			}
			else {
				int iRow, iCol;
				GvrsLong* lOffsets = td->lOffsets;
				GvrsLong* xOffsets = calloc(n, sizeof(GvrsLong));
				if (!xOffsets) {
					return GVRSERR_NOMEM;
				}
				// TO DO:  replace col loop with memmove?  memcpy?  
				for (iRow = 0; iRow < nRows; iRow++) {
					int iRowOffset = iRow * nCols; // position of tile position in original grid
					int xRowOffset = (iRow + row0 - row0X) * nColsX + (col0 - col0X); // position in extended grid
					for (iCol = 0; iCol < nCols; iCol++) {
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
