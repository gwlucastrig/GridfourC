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

#include "GvrsCodec.h"
#include "GvrsMetadata.h"

#ifndef GVRS_INTERNAL_H
#define GVRS_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif




static const long FILEPOS_OFFSET_TO_HEADER_RECORD = 16;
static const long FILEPOS_MODIFICATION_TIME = 40;
static const long FILEPOS_OPENED_FOR_WRITING_TIME = 48;
static const long FILEPOS_OFFSET_TO_FILESPACE_DIR = 56;
static const long FILEPOS_OFFSET_TO_METADATA_DIR = 64;
static const long FILEPOS_OFFSET_TO_TILE_DIR = 80;



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
		uint8_t  recType; /* the record type */
		uint8_t  reservedA[3];
	} GvrsRecord;

	 

	typedef struct GvrsTileTag {
		struct GvrsTileTag* next;
		struct GvrsTileTag* prior;
		int tileIndex;
		int referenceArrayIndex;

		int writePending;
		int32_t fileRecordContentSize;
		int64_t filePosition;  // zero if not written to file

		uint8_t* data;   // these bytes are "typeless" until type cast using a GvrsElement.
	} GvrsTile;

	typedef struct GvrsTileDirectoryTag {
		int32_t row0;
		int32_t col0;
		int32_t row1;
		int32_t col1;
		int32_t nRows;
		int32_t nCols;
		int32_t nRowsOfTiles;
		int32_t nColsOfTiles;
		// one of the following pointers should be set.  iOffsets when
		// compact references are used.  lOffsets for extended references.
		uint32_t* iOffsets;
		int64_t* lOffsets;
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



	typedef struct GvrsTileOutputBlockTag {
		int compressed;
		int nBytesInOutput;
		uint8_t* output;
	}GvrsTileOutputBlock;

	typedef struct GvrsTileCacheTag {
		void* gvrs;
		int32_t maxTileCacheSize;
		int32_t firstTileIndex;
		int32_t lastReadFailure;
		GvrsTile* tileReferenceArray;
		GvrsTile* freeList;
		GvrsTile* head; 
		GvrsTile* tail;
		GvrsTile* firstTile;
		int64_t nRasterReads;
		int64_t nRasterWrites;
		int64_t nTileReads;
		int64_t nTileWrites;
		int64_t nCacheSearches;
		int64_t nNotFound;

		// The rows/columns counts for the raster are stored as unsigned integers
		// to facilitate range checking in GvrsElement Read/Write functions.
		unsigned int nRowsInRaster;
		unsigned int nColsInRaster;
		int32_t nRowsInTile;
		int32_t nColsInTile;
		int32_t nRowsOfTiles;
		int32_t nColsOfTiles;
		int32_t nCellsInTile;

		GvrsTileDirectory* tileDirectory;
		GvrsTileHashTable* hashTable;

		int nElementsInTupple;
		GvrsTileOutputBlock* outputBlocks;
	}GvrsTileCache;


	typedef struct GvrsMetadataReferenceTag {
		void* gvrs;
		char name[GVRS_METADATA_NAME_SZ + 4];
		int32_t recordID;
		GvrsMetadataType metadataType;
		int32_t dataSize;
		int64_t filePos;
	}GvrsMetadataReference;

	typedef struct GvrsMetadataDirectoryTag {
		int writePending;
		int nMetadataReferences;
		int64_t filePosMetadataDirectory;
		GvrsMetadataReference* references;
	}GvrsMetadataDirectory;


	typedef struct GvrsFileSpaceNodeTag {
		struct GvrsFileSpaceNodeTag* next;
		int32_t blockSize;
		int64_t filePos;
	}GvrsFileSpaceNode;

	typedef struct GvrsFileSpaceManagerTag {
		int64_t expectedFileSize;
		int64_t lastRecordPosition;
		int64_t recentRecordPosition;
		int64_t recentStartOfContent;
		int32_t  recentRecordSize;
		GvrsRecordType recentRecordType;

		GvrsFileSpaceNode* freeList;
		FILE* fp;
		int checksumEnabled;

		int64_t nAllocations;
		int64_t nDeallocations;
		int64_t nFinish;
	}GvrsFileSpaceManager;

	const char* GvrsGetRecordTypeName(int index);
	GvrsRecordType  GvrsGetRecordType(int index);
	 
	int GvrsTileDirectoryAllocEmpty(int nRowsOfTiles, int nColsOfTiles, GvrsTileDirectory** tileDirectoryReference);
	int GvrsTileDirectoryRead(Gvrs* gvrs, int64_t fileOffset, GvrsTileDirectory** tileDirectoryReference);
	int GvrsTileDirectoryWrite(Gvrs* gvrs, int64_t* tileDirectoryPos);
	GvrsTileDirectory* GvrsTileDirectoryFree(GvrsTileDirectory* tileDirectory);
	int64_t GvrsTileDirectoryGetFilePosition(GvrsTileDirectory* tileDir, int tileIndex);


	/**
	* Sets the file position for a tile in the GVRS tile directory. The GVRS specification requires that
	* file positions for all references (including tile references) always be a multiple of 8. This approach
	* allows file positions for files of size up to 32 GiB to be stored in 4-byte unsigned integers.
	* @param td a pointer to a valid tile directory structure
	* @param tileIndex a positive integer
	* @param filePosition the filePos in the file at which the tile is stored.
	*/
	int GvrsTileDirectoryRegisterFilePosition(GvrsTileDirectory* td, int32_t tileIndex, int64_t filePosition);


	int GvrsFileSpaceAlloc(GvrsFileSpaceManager* manager, GvrsRecordType recordType, int sizeOfContent, int64_t* filePos);
	int GvrsFileSpaceDealloc(GvrsFileSpaceManager* manager, int64_t contentPosition);
	/**
	* Computes the standard maximum capacity for a tile cache based on the number
	* of tiles in a source raster and the type of size allocation
	* @param nRowsOfTiles the number of rows of tiles in the raster
	* @param nColsOfTiles the number of columns of tiles in the raster
	* @param cacheSize enumeration giving small, medium, large, and extra large.
	* @return a positive integer giving the computed size.
	*/
	int GvrsTileCacheComputeStandardSize(int nRowsOfTiles, int nColsOfTiles, GvrsTileCacheSizeType cacheSize);

	int GvrsTileCacheAlloc(void* gvrspointer, int maxTileCacheSize, GvrsTileCache** tileCacheRefrence);
	GvrsTileCache* GvrsTileCacheFree(GvrsTileCache* cache);
	int GvrsTileCacheWritePendingTiles(GvrsTileCache* tc);

	/**
	* Fetches a tile from the tile cache, if available.  This function is intended to support
	* data queries from the calling application.  It is generally invoked by a GVRS element
	* function. If the tile of interest is not currently in the cache,
	* but is included in the source GVRS file, this function reads the tile from the source file
	* and stores it in the cache.
	* @param tc a pointer to a valid tile cache instance.
	* @param tileIndex the index for the tile of interest. 
	* @param errCode a pointer to a storage location to receive the error code in case of a failure
	* to obtain a tile.
	* @return if successful, a pointer to a storage location for the tile of interest; otherwise, a null.
	*/
	GvrsTile* GvrsTileCacheFetchTile(GvrsTileCache* tc, int tileIndex, int* errCode);

	/**
	* Initializes a tile in the tile cache assigning it the specified tile index.
	* Intended for writing data to a GVRS file.  If the tile cache is full, the oldest
	* tile in the cache will be removed and potentially written.
	* @param tc a pointer to a valid tile cache instance.
	* @param tileIndex the index for the tile of interest. 
	* @param errCode a pointer to a storage location to receive the error code in case of a failure
	* to obtain a tile.
	* @return if successful, a pointer to a storage location for the tile of interest; otherwise, a null.
	*/
	GvrsTile* GvrsTileCacheStartNewTile(GvrsTileCache* tc,  int tileIndex, int* errCode);

	int GvrsMetadataDirectoryAllocEmpty(Gvrs* gvrs, GvrsMetadataDirectory** directory);
	int GvrsMetadataDirectoryRead(FILE *fp, int64_t filePosMetadataDir, GvrsMetadataDirectory** directory);
	int GvrsMetadataDirectoryWrite(void* gvrsReference, int64_t* filePosMetadataDirectory);
	int GvrsMetadataRead(FILE* fp, GvrsMetadata**);
	GvrsMetadataDirectory* GvrsMetadataDirectoryFree(GvrsMetadataDirectory* dir);
 



	void GvrsElementFillData(GvrsElement* element, uint8_t* data, int nCells);
	int  GvrsFileSpaceFinish(GvrsFileSpaceManager* manager, int64_t contentPos);
	GvrsFileSpaceManager* GvrsFileSpaceManagerAlloc(FILE *fp);
	GvrsFileSpaceManager* GvrsFileSpaceManagerFree(GvrsFileSpaceManager*);
	int GvrsFileSpaceDirectoryRead(Gvrs* gvrs, int64_t freeSpaceDirectoryPosition, GvrsFileSpaceManager** managerRef);
	int GvrsFileSpaceDirectoryWrite(Gvrs* gvrs, int64_t* filePosition);

	Gvrs* GvrsDisposeOfResources(Gvrs* gvrs);


#ifdef __cplusplus
}
#endif

#endif
