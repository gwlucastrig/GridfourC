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
#include <time.h>
#include <string.h>


/**
* A simple test program that reads a GVRS file, summarizes its content
* and reads the entire raster one cell at a time in row-major order.
* @param argc the number of command-line arguments, including command vector, always one or greater.
* @param argv the command vector, argv[1] giving the path to the input file for processing.
* @return zero on successful completion; otherwise, an error code.
*/
int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "\nUsage:  GvrsRead <input_file>\n");
		exit(0);
	}
	const char* target = argv[1];
	printf("\nReading input file: %s\n", target);

    int status;
	Gvrs* gvrs = GvrsOpen(target, "r", &status);
	if (!gvrs) {
		printf("Unable to open GVRS file, error code %d\n", status);
		exit(1);
	}

	// Because this operation loops over the entire grid row-by-row, we wish
	// to retain an entire row of tiles in the cache so that they only need
	// to be read from the source data once.  To do so, we set the tile-cache size
	// to large.
	GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeLarge);


	// Write a summary of the source file's structure and data definitions
	// to standard output.  In practice, any valid output stream (FILE pointer)
	// can be used.
	GvrsSummarize(gvrs, stdout);


	printf("\nPerforming exhaustive read operation on input file\n");
	int iRow, iCol, nRows, nCols;
	nRows = gvrs->nRowsInRaster;
	nCols = gvrs->nColsInRaster;
	int iElement, nElements;
	GvrsElement** elements = GvrsGetElements(gvrs, &nElements);
	for (iElement = 0; iElement < nElements; iElement++) {
		GvrsElement* e = elements[iElement];
		// The loops below use either the Integer or Float read methods depending
		// on the type of the source data.
		if (e->elementType == GvrsElementTypeInt || e->elementType == GvrsElementTypeShort) {
			long long sumValue = 0;
			long      nGood = 0;
			GvrsInt iValue;
			GvrsLong time0 = GvrsTimeMS();
			for (iRow = 0; iRow < nRows; iRow++) {
				for (iCol = 0; iCol < nCols; iCol++) {
					int status = GvrsElementReadInt(e, iRow, iCol, &iValue);
					if (status) {
						// non-zero indicates an error
						if (status == GVRSERR_COMPRESSION_NOT_IMPLEMENTED) {
							printf("Read test failed due to non-implemented compressor\n");
						}
						else {
							printf("Read test failed on error %d\n", status);
						}
						exit(status);
					}
					else {
						nGood++;
						sumValue += iValue;
					}
				}
			}
			GvrsLong time1 = GvrsTimeMS();
			printf("Processing completed in %lld ms\n", (long long)(time1-time0));
			if (nGood) {
				double avgValue = (double)sumValue / (double)nGood;
				printf("Average value %f on %ld successful queries\n", avgValue, nGood);
			}
		}
		else {
			double     sumValue = 0;
			long       nGood = 0;
			GvrsFloat fValue;
			GvrsLong time0 = GvrsTimeMS();
			for (iRow = 0; iRow < nRows; iRow++) {
				for (iCol = 0; iCol < nCols; iCol++) {
					int status = GvrsElementReadFloat(e, iRow, iCol, &fValue);
					if (status) {
						// non-zero indicates an error
						if (status) {
							// non-zero indicates an error
							if (status == GVRSERR_COMPRESSION_NOT_IMPLEMENTED) {
								printf("Read test failed due to non-implemented compressor\n");
							}
							else {
								printf("Read test failed on error %d\n", status);
							}
							exit(status);
						}
					}
					else {
						nGood++;
						sumValue += fValue;
					}
				}
			}
			GvrsLong time1 = GvrsTimeMS();
			printf("Processing completed in %lld ms\n", (long long)(time1-time0));
			if (nGood) {
				double avgValue = (double)sumValue / (double)nGood;
				printf("Average value %f on %ld successful queries\n", avgValue, nGood);
			}
		}
	}

	GvrsSummarizeAccessStatistics(gvrs, stdout);
	gvrs = GvrsClose(gvrs, &status);
	exit(0);
}
