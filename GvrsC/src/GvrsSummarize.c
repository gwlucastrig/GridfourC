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
 

int
GvrsGetStringUUID(Gvrs* gvrs, size_t uuidStringSize, char* uuidString) {
	GvrsLong uuidLow = gvrs->uuidLow;
	GvrsLong uuidHigh = gvrs->uuidHigh;
	return snprintf(uuidString, uuidStringSize,
		"%07llx-%04llx-%04llx-%04llx-%012llx",
		(uuidHigh >> 32) & 0xfffffffLL,
		(uuidHigh >> 16) & 0xffffLL,
		uuidHigh & 0xffffLL,
		(uuidLow >> 48) & 0xffffLL,
		uuidLow & 0xffffffffffffLL);
}

static const char* elementTypeStr[] = { "Integer", "Integer-Coded Float", "Float", "Short" };

static const char* tileCacheSizeStr[] = { "Small", "Medium", "Large", "Extra Large" };
 
static const char* strspec(const char* s) {
	if (s && *s) {
		return s;
	}
	return "Not specified";
}


int
GvrsSummarize(Gvrs* gvrs, FILE* fp) {
	int i;

	if (!gvrs || !fp) {
		return GVRSERR_FILE_ERROR;
	}
	char uuidString[40]; // UUID is 35 char's long
	GvrsGetStringUUID(gvrs, sizeof(uuidString), uuidString);
	char modTimeStr[64];
	struct tm modTM;
	GVRS_GMTIME(&modTM, &gvrs->modTimeSec);
	strftime(modTimeStr, sizeof(modTimeStr), "%Y-%m-%d %H:%M:%S", &modTM);

	fprintf(fp, "\n");
	fprintf(fp, "GVRS file:       %s\n", gvrs->path);
	fprintf(fp, "UUID:            %s\n", uuidString);
	fprintf(fp, "Identification:  %s\n", strspec(gvrs->productLabel));
	fprintf(fp, "Last modified:   %s (UTC)\n", modTimeStr);
	fprintf(fp, "\n");
	long long nCellsInRaster = (long long)gvrs->nRowsInRaster * (long long)gvrs->nColsInRaster;
	long nCellsInTile = (long)gvrs->nRowsInTile * (long)gvrs->nColsInTile;
	long nTilesInRaster = (long)gvrs->nRowsOfTiles * (long)gvrs->nColsOfTiles;
	fprintf(fp, "Rows in raster:     %12d\n", gvrs->nRowsInRaster);
	fprintf(fp, "Columns in raster:  %12d\n", gvrs->nColsInRaster);
	fprintf(fp, "Rows in tile:       %12d\n", gvrs->nRowsInTile);
	fprintf(fp, "Columns in tile:    %12d\n", gvrs->nColsInTile);
	fprintf(fp, "Rows of tiles:      %12d\n", gvrs->nRowsOfTiles);
	fprintf(fp, "Columns of tiles:   %12d\n", gvrs->nColsOfTiles);

	fprintf(fp, "Cells in raster:    %12lld\n", nCellsInRaster);
	fprintf(fp, "Cells in tile:      %12ld\n", nCellsInTile);
	fprintf(fp, "Tiles in raster:    %12ld\n", nTilesInRaster);
	fprintf(fp, "\n");
	fprintf(fp, "Checksums:         %s\n", gvrs->checksumEnabled ? "Enabled" : "Disabled");
	fprintf(fp, "\n");
	fprintf(fp, "Coordinate system: %s\n",	gvrs->geographicCoordinates ? "Geographic" : "Cartesian");
	
	fprintf(fp, "Range of Values, Cell Center\n");
	fprintf(fp, "   x values:      %11.6f, %11.6f, (%f)\n", 
		gvrs->x0, gvrs->x1, gvrs->x1 - gvrs->x0);
	fprintf(fp, "   y values:      %11.6f, %11.6f, (%f)\n",
		gvrs->y0, gvrs->y1, gvrs->y1 - gvrs->y0);

	fprintf(fp, "Range of Values, Full Domain\n");
	fprintf(fp, "   x values:      %11.6f, %11.6f, (%f)\n",
		gvrs->x0 - gvrs->cellSizeX / 2,
		gvrs->x1 + gvrs->cellSizeX / 2,
		gvrs->x1 - gvrs->x0 + gvrs->cellSizeX);
	fprintf(fp, "   y values:      %11.6f, %11.6f, (%f)\n",
		gvrs->y0 - gvrs->cellSizeY / 2, 
		gvrs->y1 + gvrs->cellSizeY / 2, 
		gvrs->y1 - gvrs->y0 + gvrs->cellSizeY);
	
	fprintf(fp, "\nElements ----------------------------------------\n");
	int nElements;
	GvrsElement** elements = GvrsGetElements(gvrs, &nElements);
	for (i = 0; i < nElements; i++) {
		GvrsElement* e = elements[i];
		int eType = (int)e->elementType;
		fprintf(fp, "%-2d  Name:   %s\n", i, e->name);
		fprintf(fp, "    Type:   %s\n", elementTypeStr[eType]);
		fprintf(fp, "    Label:  %s\n", strspec(e->label));
		fprintf(fp, "    Description: %s\n", strspec(e->description));
		fprintf(fp, "    Units:       %s\n", strspec(e->unitOfMeasure));
		fprintf(fp, "    Values\n");
		switch (e->elementType) {
		case GvrsElementTypeInt:
			fprintf(fp, "        Minimum: %9d\n", e->elementSpec.intSpec.minValue);
			fprintf(fp, "        Maximum: %9d\n", e->elementSpec.intSpec.maxValue);
			fprintf(fp, "        Fill:    %9d\n", e->elementSpec.intSpec.fillValue);
			break;
		case GvrsElementTypeIntCodedFloat:
			fprintf(fp, "        Minimum: %f\n", e->elementSpec.intFloatSpec.minValue);
			fprintf(fp, "        Maximum: %f\n", e->elementSpec.intFloatSpec.maxValue);
			fprintf(fp, "        Fill:    %f\n", e->elementSpec.intFloatSpec.fillValue);
			break;
		case GvrsElementTypeFloat:
			fprintf(fp, "        Minimum: %f\n", e->elementSpec.floatSpec.minValue);
			fprintf(fp, "        Maximum: %f\n", e->elementSpec.floatSpec.maxValue);
			fprintf(fp, "        Fill:    %f\n", e->elementSpec.floatSpec.fillValue);
			break;
		case GvrsElementTypeShort:
			fprintf(fp, "        Minimum: %9hd\n", e->elementSpec.shortSpec.minValue);
			fprintf(fp, "        Maximum: %9hd\n", e->elementSpec.shortSpec.maxValue);
			fprintf(fp, "        Fill:    %9hd\n", e->elementSpec.shortSpec.fillValue);
			break;
		}
		fprintf(fp, "\n");
	}

	fprintf(fp, "\n");

	if (gvrs->nDataCompressionCodecs) {
		fprintf(fp, "\n");
		fprintf(fp, "Data compression:  Enabled\n");
		fprintf(fp, "Identification            Read Int    Write Int     Read Float     Write Flt\n");
		for (i = 0; i < gvrs->nDataCompressionCodecs; i++) {
			GvrsCodec* codec = gvrs->dataCompressionCodecs[i];
			const char* rdInt = codec->decodeInt ? "Yes" : "No ";
			const char* wrInt = codec->encodeInt ? "Yes" : "No ";
			const char* rdFlt = codec->decodeFloat ? "Yes" : "No ";
			const char* wrFlt = codec->encodeFloat ? "Yes" : "No ";
			fprintf(fp, "    %-16.16s      %s         %s           %s            %s\n", 
				codec->identification, rdInt, wrInt, rdFlt, wrFlt);
		}
	}
	else {
		fprintf(fp, "Data Compression:  Disabled\n");
	}

	fprintf(fp, "\n");
	GvrsTileCache* tc = gvrs->tileCache;
	long maxTileCacheAllocation = (long)tc->maxTileCacheSize * (long)gvrs->nBytesForTileData;

	fprintf(fp, "----------------------------------------\n");
	fprintf(fp, "Tile cache size: %s,  %4.1f MiB,    bytes per tile: %ld\n",
		tileCacheSizeStr[(int)gvrs->tileCacheSize], 
		maxTileCacheAllocation/1048576.0,
		(long)gvrs->nBytesForTileData);
	fprintf(fp,"Options for standard cache sizes\n");
	fprintf(fp, "    Size              Max Tiles      Max Memory (MiB)\n");
	for (i = 0; i < 4; i++) {
		long n = GvrsTileCacheComputeStandardSize(gvrs->nRowsOfTiles, gvrs->nColsOfTiles, (GvrsTileCacheSizeType)i);
		maxTileCacheAllocation = n * (long)gvrs->nBytesForTileData;
		fprintf(fp, "    %-12.12s           %4ld            %9.1f\n",
			tileCacheSizeStr[i],
			n,
			maxTileCacheAllocation / 1048576.0);
	}
	
 

	fprintf(fp, "\n");

	fprintf(fp, "Metadata ----------------------------------------\n");
	fprintf(fp, "     Name                           Record ID    Type\n");
	GvrsMetadataDirectory* md = gvrs->metadataDirectory;
	if (md) {
		for (i = 0; i < md->nMetadataRecords; i++) {
			GvrsMetadataReference* m = md->records + i;
			const char* typeName = GvrsMetadataGetTypeName(m->metadataType);
			printf("%2d.  %-32.32s  %6d    %-12.12s\n", i, m->name, m->recordID, typeName);
		}
	}
	fprintf(fp, "\n");
	return 0;
}


int
GvrsSummarizeAccessStatistics(Gvrs* gvrs, FILE* fp) {
 
	if (!gvrs || !fp) {
		return GVRSERR_FILE_ERROR;
	}

	GvrsTileCache* tc = gvrs->tileCache;
	GvrsLong nReadsAndWrites = tc->nRasterReads + tc->nRasterWrites;
	GvrsLong nAccessToCurrentTile = nReadsAndWrites - tc->nCacheSearches;
	fprintf(fp, "\n");
	fprintf(fp, "Access statistics ------------------------------\n");
	fprintf(fp, "Number of Reads:        %12lld\n", (long long)tc->nRasterReads);
	fprintf(fp, "Number of Writes:       %12lld\n", (long long)tc->nRasterWrites);
	fprintf(fp, "Met by current tile:    %12lld\n", (long long)nAccessToCurrentTile);
	fprintf(fp, "Cache searches:         %12lld\n", (long long)tc->nCacheSearches);
	fprintf(fp, "Number not-found:       %12lld\n", (long long)tc->nNotFound);
	fprintf(fp, "Number of tile reads:   %12lld\n", (long long)tc->nTileReads);
	fprintf(fp, "Number of tile writes:  %12lld\n", (long long)tc->nTileWrites);

	if (gvrs->fileSpaceManager) {
		GvrsFileSpaceManager* fsm = gvrs->fileSpaceManager;
		GvrsInt nFreeRecords = 0;
		GvrsLong sizeFreeRecords = 0;
		GvrsFileSpaceNode* node = fsm->freeList;
		while (node) {
			nFreeRecords++;
			sizeFreeRecords += node->blockSize;
			node = node->next;
		}
		fprintf(fp, "\nFile space management\n");
		fprintf(fp, "    Number of free blocks:   %8d\n", nFreeRecords);
		fprintf(fp, "    Unused file space:       %8lld\n", (long long)sizeFreeRecords);
		fprintf(fp, "    Number of allocations:   %8lld\n", (long long)fsm->nAllocations);
		fprintf(fp, "    Number of finishes:      %8lld\n", (long long)fsm->nFinish);
		fprintf(fp, "    Number of deallocations: %8lld\n", (long long)fsm->nDeallocations);
	}
	return 0;
}
