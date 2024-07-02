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


#pragma once

// Header file for the GVRS API internal functions and data structures.


#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"
#include "GvrsCodec.h"
#include "GvrsMetadata.h"

#ifndef GVRS_INTERNAL_H
#define GVRS_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

// Macro to adjust an integer value to be a multiple of 4
#define GVRS_MULTI_4(A)  (((A) + 3) & 0x7ffffffc)

#define GVRS_RECORD_TYPE_COUNT  7


	typedef enum {
		GvrsRecordTypeFreespace = 0,
		GvrsRecordTypeMetadata = 1,
		GvrsRecordTypeTile = 2,
		GvrsRecordTypeFilespaceDir = 3,
		GvrsRecordTypeMetadataDir = 4,
		GvrsRecordTypeTileDir = 5,
		GvrsRecordTypeHeader = 6,
		GvrsRecordTypeUndefined = 7
	} GvrsRecordType;

	typedef struct GvrsRecordTag {
		uint32_t  length;  /*  the record length, in bytes. negative for free space */
		GvrsByte  recType; /* the record type */
		GvrsByte  reservedA[3];
	} GvrsRecord;

	 

	typedef struct GvrsTileTag {
		struct GvrsTileTag* next;
		struct GvrsTileTag* prior;
		int tileIndex;
		int referenceArrayIndex;
		GvrsByte* data;   // these bytes are "typeless" until type cast using a GvrsElement.
	} GvrsTile;

	typedef struct GvrsTileDirectoryTag {
		GvrsInt row0;
		GvrsInt col0;
		GvrsInt row1;
		GvrsInt col1;
		GvrsInt nRows;
		GvrsInt nCols;
		GvrsInt nRowsOfTiles;
		// one of the following pointers should be set.  iOffsets when
		// compact references are used.  lOffsets for extended references.
		GvrsUnsignedInt* iOffsets;
		GvrsLong* lOffsets;
	}GvrsTileDirectory;


	typedef struct GvrsTileHashEntryTag {
		int tileIndex;
		GvrsTile* tile;
	}GvrsTileHashEntry;

	typedef struct GvrsTileHashBinTag {
		int nEntries;
		int nAllocated;
		GvrsTileHashEntry* entries;
	}GvrsTileHashBin;

	// the hash size must be an integral power of 2
#define GVRS_TILE_HASH_SIZE  256
#define GVRS_TILE_HASH_MASK  (GVRS_TILE_HASH_SIZE-1)
#define GVRS_TILE_HASH_GROWTH_SIZE 1
#define GVRS_TILE_HASH_BLOCK_SIZE (GVRS_TILE_HASH_GROWTH_SIZE*GVRS_TILE_HASH_SIZE)

	typedef struct GvrsTileHashTableTag {
		GvrsTileHashBin bins[GVRS_TILE_HASH_SIZE];
		int nAllocationBlocks;
		int nEntriesUsedInBlock;
		GvrsTileHashEntry** allocationBlocks;
	}GvrsTileHashTable;


	typedef struct GvrsTileCacheTag {
		void* gvrs;
		GvrsInt maxTileCacheSize;
		GvrsInt firstTileIndex;
		GvrsInt lastReadFailure;
		GvrsTile* tileReferenceArray;
		GvrsTile* freeList;
		GvrsTile* head; 
		GvrsTile* tail;
		GvrsTile* firstTile;
		GvrsLong nFetches;
		GvrsLong nTileReads;
		GvrsLong nCacheSearches;
		GvrsLong nNotFound;


		GvrsInt nRowsInRaster;
		GvrsInt nColsInRaster;
		GvrsInt nRowsInTile;
		GvrsInt nColsInTile;
		GvrsInt nRowsOfTiles;
		GvrsInt nColsOfTiles;
		GvrsTileDirectory* tileDirectory;
		GvrsTileHashTable* hashTable;
	}GvrsTileCache;


	typedef struct GvrsMetadataReferenceTag {
		void* gvrs;
		char name[GVRS_METADATA_NAME_SZ + 4];
		GvrsInt recordID;
		GvrsMetadataType metadataType;
		GvrsInt dataSize;
		GvrsLong offset;
	}GvrsMetadataReference;

	typedef struct GvrsMetadataDirectoryTag {
		int nMetadataRecords;
		GvrsMetadataReference* records;
	}GvrsMetadataDirectory;


	const char* GvrsGetRecordTypeName(int index);
	GvrsRecordType  GvrsGetRecordType(int index);
	 
	GvrsTileDirectory* GvrsTileDirectoryRead(FILE *fp, GvrsLong fileOffset, int *errCode);
	GvrsTileDirectory* GvrsTileDirectoryFree(GvrsTileDirectory* tileDirectory);
	 
	/**
	* Computes the standard maximum capacity for a tile cache based on the number
	* of tiles in a source raster and the type of size allocation
	* @param nRowsOfTiles the number of rows of tiles in the raster
	* @param nColsOfTiles the number of columns of tiles in the raster
	* @param cacheSize enumeration giving small, medium, large, and extra large.
	* @return a positive integer
	*/
	int GvrsTileCacheComputeStandardSize(int nRowsOfTiles, int nColsOfTiles, GvrsTileCacheSizeType cacheSize);

	GvrsTileCache* GvrsTileCacheAlloc(void* gvrs, int maxTileCacheSize);
	GvrsTileCache* GvrsTileCacheFree(GvrsTileCache* cache);

	/**
	* Fetches a tile from the tile cache, if available.  This function is intended to support
	* data queries from the calling application.  It is generally invoked by a GVRS element
	* function. If the tile of interest is not currently in the cache,
	* but is included in the source GVRS file, this function reads the tile from the source file
	* and stores it in the cache. The tile of interest is identified by the grid row and column.
	* If a tile is available then the index into its array(s) of data values is computed
	* from the grid row and column values.
	* @param tc a pointer to a valid tile cache instance
	* @param gridRow the row within the overall grid for the raster data cell of interest.
	* @param gridColumn the column within the overall grid for the raster data cell of interest.
	* @param indexInTile a pointer to a storage location to receive the computed index within the
	* tile's data array(s) for the raster data value of interest.
	* @param errCode a pointer to a storage location to receive the error code in case of a failure
	* to obtain a tile.
	* @return if successful, a pointer to a storage location for the tile of interest; otherwise, a null.
	*/
	GvrsTile* GvrsTileCacheFetchTile(GvrsTileCache* tc, int gridRow, int gridColumn, int* indexInTile, int* errCode);

	GvrsMetadataDirectory* GvrsMetadataDirectoryRead(FILE *fp, GvrsLong filePosMetadataDir, int *errCode);
	GvrsMetadataDirectory* GvrsMetadataDirectoryFree(GvrsMetadataDirectory* dir);



#ifdef __cplusplus
}
#endif

#endif
