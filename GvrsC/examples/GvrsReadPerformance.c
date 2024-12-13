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
#include "GvrsError.h"

const char* usage[] = {

	"Test of Read Performance for GVRS file",
	"",
	"Usage:  GvrsReadPerformance <input file>  [n repeats]",
	"",
	"This program conducts 4 separate tests that can be used to judge different",
	"aspects of GVRS file access performance.",
	"",
	"Tile Access",
	"The tile-access test reads a single data value from each tile in the source",
	"file. Tiles are read in row-major order.  This test evaluates the time",
	"required to read tiles from the source file. If the source file",
	"features data compression, this test will also include the overhead",
	"for decompressing the data. The number of operations",
	"will correspond to the number of tiles in the file.",
	"",
	"Tile Block Access",
	"The tile-block access test loops on tiles, reading values for",
	"every data cell in the tile. The total number of value-read",
	"operations is equivalent to the number of cells in the raster,",
	"but each tile is read only once.  Also, each value is taken from the",
	"first tile in the tile cache.  This approach eliminates the overhead",
	"for tile - cache searches.  Taking the difference between the time",
	"for this test and the time for the tile-access test",
	"gives an indication of how much overhead is contributed by",
	"evaluating row and column inputs and transferring data values",
	"from tile instances.",
	"",
	"Row Major Order",
	"The row-major test reads each cell in the raster one at a time",
	"in row-major order.  Taking the time difference between this test and",
	"the tile-access test gives an indication of how much overhead",
	"is contributed by tile-cache searches.",
	"",
	"Column Major Order",
	"The column-major test reads each cell in the raster one at a time",
	"in column-major order. Its main purpose when contrasted with the row-major",
	"results is to indicate whether read performance is affected by pattern-of-access.",
	"Because many of the Gridfour packaging programs store the tiles",
	"for a file in row-major order, this test may take somewhat",
	"longer than the row-major test.",
	0
};

static const char* testName[] = { "Tile Access", "Tile Block Access", "Row Major Order", "Column Major Order" };
typedef enum {
	TimeForTileAccess = 0,
	TimeForTileBlockAccess,
	TimeForRowMajorRead,
	TimeForColumnMajorRead
} TimeTest;

typedef struct TestResultsTag {
	long elapsedTime;
	long nOperations;
}TestResults;



static int performTimeTest(const char* path, TimeTest testType, TestResults *results);


 /**
 * A test program that reads a GVRS file using different patterns of access
 * as a way of assessing performance.  
 * @param argc the number of command-line arguments, including command vector, always one or greater.
 * @param argv the command vector, argv[1] giving the path to the input file for processing.
 * @return zero on successful completion; otherwise, an error code.
 */
int main(int argc, char* argv[]) {

	if (argc < 2) {
		const char** p = usage;
		while (*p) {
			printf("%s\n", *p);
			p++;
		}
		exit(0);
	}
	int nRepeats = 1;
	if (argc >= 3) {
		int k = atoi(argv[2]);
		if (k > 1) {
			nRepeats = k;
		}
	}

	TestResults results;
	int i, j;

	printf("Read Performance for file %s\n", argv[1]);
	printf("Test                     Time (sec)    Operations    Operations/sec\n");
	for (i = 0; i < 4; i++) {
		for (j = 0; j < nRepeats; j++) {
			int status = performTimeTest(argv[1], (TimeTest)i, &results);
			if (status) {
				printf("test failed on error %d for test %s\n", status, testName[i]);
				exit(-1);
			}
			double elapsedTime = results.elapsedTime / 1000.0;
			double rate = results.nOperations / elapsedTime;
			printf("%-20.20s    %8.3f     %12ld  %12.1f\n", testName[i], elapsedTime, results.nOperations, rate);
		}
	}

	exit(0);
}


static int performTimeTest(const char* path, TimeTest testType, TestResults* results) {

	memset(results, 0, sizeof(TestResults));

	int status;
	Gvrs* gvrs;
	status = GvrsOpen(&gvrs, path, "r");
	if (!gvrs) {
		return status;
	}
	GvrsElement* e = GvrsGetElementByIndex(gvrs, 0);
	if (!e) {
		return GVRSERR_ELEMENT_NOT_FOUND;
	}

	// for tests below, we wish to use a read operation that matches
	// the data type of the element.  Presumably, this choice reflects
	// the way an actual application would use the API.
	int isElementIntegral = GvrsElementIsIntegral(e);


	int nRowsInRaster = gvrs->nRowsInRaster;
	int nColsInRaster = gvrs->nColsInRaster;
	int nRowsOfTiles = gvrs->nRowsOfTiles;
	int nColsOfTiles = gvrs->nColsOfTiles;
	int nRowsInTile = gvrs->nRowsInTile;
	int nColsInTile = gvrs->nColsInTile;
	long nCellsInRaster = (long)nRowsInRaster * (long)nColsInRaster;
	long nTilesInRaster = (long)nRowsOfTiles * nColsOfTiles;

	int iRow, iCol, tileRow, tileCol;
	int iValue;
	float fValue;

	int64_t time0 = GvrsTimeMS();

	switch (testType) {
	case TimeForTileAccess:
		// Read only one value per tile. The time required
		// for tile access will dominate.  The tile cache does not matter
		// because each tile is accessed only once.
		GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeSmall);
		results->nOperations = nTilesInRaster;
		if (isElementIntegral) {
			for (tileRow = 0; tileRow < nRowsOfTiles; tileRow++) {
				int gridRow = tileRow * nRowsInTile;
				for (tileCol = 0; tileCol < nColsOfTiles; tileCol++) {
					int gridCol = tileCol * nColsInTile;
					int status = GvrsElementReadInt(e, gridRow, gridCol, &iValue);
					if (status) {
						return status;
					}

				}
			}
		}
		else {
			for (tileRow = 0; tileRow < nRowsOfTiles; tileRow++) {
				int gridRow = tileRow * nRowsInTile;
				for (tileCol = 0; tileCol < nColsOfTiles; tileCol++) {
					int gridCol = tileCol * nColsInTile;
					int status = GvrsElementReadFloat(e, gridRow, gridCol, &fValue);
					if (status) {
						return status;
					}
				}
			}
		}
		break;

	case TimeForTileBlockAccess:
		// For each tile, read all cells within that tile.
		// This test will minimize the access time for reading the tile,
		// Each tile will be read only once and the tile supplying the data
		// for the read operation will always be the first tile in the tile cache. 
		// The total run time includes the time required to compute tile indices and copy the
		// data.   This information will help developers separte the relative
		// cost of different operations.
		GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeSmall);
		results->nOperations = nCellsInRaster;
		for (tileRow = 0; tileRow < nRowsOfTiles; tileRow++) {
			int gridRow0 = tileRow * nRowsInTile;
			int gridRow1 = gridRow0 + nRowsInTile;
			if (gridRow1 > nRowsInRaster) {
				// The last tile extends beyond the grid dimensions
				gridRow1 = nRowsInRaster;
			}
			for (tileCol = 0; tileCol < nColsOfTiles; tileCol++) {
				int gridCol0 = tileCol * nColsInTile;
				int gridCol1 = gridCol0 + nColsInTile;
				if (gridCol1 > nColsInRaster) {
					// the last tile extends beyond the grid dimensions
					gridCol1 = nColsInRaster;
				}
				if (isElementIntegral) {
					for (iRow = gridRow0; iRow < gridRow1; iRow++) {
						for (iCol = gridCol0; iCol < gridCol1; iCol++) {
							int status = GvrsElementReadInt(e, iRow, iCol, &iValue);
							if (status) {
								return status;
							}
						}
					}
				}
				else {
					for (iRow = gridRow0; iRow < gridRow1; iRow++) {
						for (iCol = gridCol0; iCol < gridCol1; iCol++) {
							int status = GvrsElementReadFloat(e, iRow, iCol, &fValue);
							if (status) {
								return status;
							}
						}
					}
				}
			}
		}
		break;

	case TimeForRowMajorRead:
		// this test evaluates the time for a row-major read operation
		GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeLarge);
		results->nOperations = nCellsInRaster;
		if (isElementIntegral) {
			for (iRow = 0; iRow < nRowsInRaster; iRow++) {
				for (iCol = 0; iCol < nColsInRaster; iCol++) {
					int status = GvrsElementReadInt(e, iRow, iCol, &iValue);
					if (status) {
						return status;
					}
				}
			}
		}
		else {
			for (iRow = 0; iRow < nRowsInRaster; iRow++) {
				for (iCol = 0; iCol < nColsInRaster; iCol++) {
					int status = GvrsElementReadFloat(e, iRow, iCol, &fValue);
					if (status) {
						return status;
					}
				}
			}
		}
		break;

	case TimeForColumnMajorRead:
		// this test evaluates the time for a row-major read operation
		GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeLarge);
		results->nOperations = nCellsInRaster;
		if (isElementIntegral) {
			for (iCol = 0; iCol < nColsInRaster; iCol++) {
				for (iRow = 0; iRow < nRowsInRaster; iRow++) {
					int status = GvrsElementReadInt(e, iRow, iCol, &iValue);
					if (status) {
						return status;
					}
				}
			}
		}
		else {
			for (iCol = 0; iCol < nColsInRaster; iCol++) {
				for (iRow = 0; iRow < nRowsInRaster; iRow++) {
					int status = GvrsElementReadFloat(e, iRow, iCol, &fValue);
					if (status) {
						return status;
					}
				}
			}
		}
		break;

	default:
		return -1;
	}

	int64_t time1 = GvrsTimeMS();
	results->elapsedTime = (long)(time1 - time0);

	status = GvrsClose(gvrs);

	return 0;
}
