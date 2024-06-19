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

// Hash codes:
//  In development, we experimented with two hash codes.  Thomas Muller's strong hash
//  and Knuth's basic hash.   While Muller's is more powerful in the general case, either works.
//  We observed better behavior from Knuth in the special case where tile-index values
//  were close together (the anticipated pattern of data access)
// 
// Muller's hash
//static uint32_t ihash(uint32_t x) {
//	x = ((x >> 16) ^ x) * 0x45d9f3bU;
//	x = ((x >> 16) ^ x) * 0x45d9f3bU;
//	x = (x >> 16) ^ x;
//	return x;
//}

// Knuth's hash
static uint32_t ihash(uint32_t x) {
	return x * 2654435761U;
}


static GvrsTileHashEntry* allocHashBlock() {
	GvrsTileHashEntry* block = calloc(GVRS_TILE_HASH_BLOCK_SIZE, sizeof(GvrsTileHashEntry));
	if (block) {
		int i;
		for (i = 0; i < GVRS_TILE_HASH_BLOCK_SIZE; i++) {
			block[i].tileIndex = -1;
		}
	}
	return block;
}

static GvrsTileHashTable* hashTableAlloc() {
	int i;
	GvrsTileHashTable* h = calloc(1, sizeof(GvrsTileHashTable));
	if (!h) {
		return 0;
	}
	h->nAllocationBlocks = 1;
	h->nEntriesUsedInBlock = GVRS_TILE_HASH_BLOCK_SIZE;
	h->allocationBlocks = calloc(1, sizeof(GvrsTileHashEntry*));
	if (!h->allocationBlocks) {
		free(h);
		return 0;
	}
	GvrsTileHashEntry* block = allocHashBlock();
	if (!block) {
		free(h->allocationBlocks);
		free(h);
		return 0;
	}
	h->allocationBlocks[0] = block;
 
	for (i = 0; i < GVRS_TILE_HASH_SIZE; i++) {
		h->bins[i].nEntries = 0;
		h->bins[i].nAllocated = GVRS_TILE_HASH_GROWTH_SIZE;
		h->bins[i].entries = block + (i * GVRS_TILE_HASH_GROWTH_SIZE);
	}
	return h;
}

static GvrsTileHashTable* hashTableFree(GvrsTileHashTable* table) {
	if (table) {
		int i;
		for (i = 0; i < table->nAllocationBlocks; i++) {
			free(table->allocationBlocks[i]);
		}
		free(table->allocationBlocks);
		free(table);
	}
	return 0;
}

static GvrsTile* hashTableLookup(GvrsTileCache *tc,  int tileIndex) {
	int i;
	GvrsTileHashBin bin = tc->hashTable->bins[ihash(tileIndex) & GVRS_TILE_HASH_MASK];

	// We optimize based on the assumption that, most of the time:
	//   a.  the target tile will will be in the cache
	//   b.  a populated bin will include only one entry.
	// To expedite searches, we ensure that when a bin is empty, it is populated
	// with an "empty" entry which has a tile index of -1.  Since -1 will
	// never match the target index, the code will pass on to the loop below.
	// This approach avoids the need to check for the "empty bin" condition
	// in the most frequent case (the target is present in the cache).
	if (bin.entries->tileIndex == tileIndex) {
		return bin.entries->tile;
	}
  
	int n = bin.nEntries;
	for (i = 1; i < n; i++) {
		GvrsTileHashEntry entry = bin.entries[i];
		if (entry.tileIndex == tileIndex) {
			return entry.tile;
		}
	}

	return 0;
}

static int hashTablePut(GvrsTileCache* tc, GvrsTile* tile) {
	int tileIndex = tile->tileIndex;
	uint32_t hashCode = ihash(tileIndex);
	GvrsTileHashTable* table = tc->hashTable;
	GvrsTileHashBin* bin = table->bins+(hashCode & GVRS_TILE_HASH_MASK);
	if (bin->nAllocated == bin->nEntries) {
		// the bin is full and it's number of entries must be expanded
		// we will repopulate bin->entries using a section from the allocation block.
		// The number of items we will need from the allocation block is the sum of
		// the number of entries already assigned to the bin, plus the growth factor
		// for extending the size of the bin entries.  Once we assign the memory that will
		// be used as the entries for the bin, we will copy any old entries into the new space,
		// then reassign the bin's entry pointer to the new space.  Note that this approach
		// has the drawback of leaving some dead space where the previously used entry array
		// was stored.  At this time, we believe that the impact of doing so is less than
		// the impact of using separate malloc calls for each bin.
		// 
		
		GvrsTileHashEntry* newEntries;
		// check to see if the existing allocation block has enough room for the new entries
		// if not, allocate a new block.
		int nNeeded = bin->nEntries + GVRS_TILE_HASH_GROWTH_SIZE;
		if (table->nEntriesUsedInBlock + nNeeded < GVRS_TILE_HASH_BLOCK_SIZE) {
			// there is enough unused space in the current allocation block,
			// so we do not need to establish another. set the newEntries pointer
			// to a position at the end of the 
			newEntries = table->allocationBlocks[table->nAllocationBlocks - 1] + table->nEntriesUsedInBlock;
		} else {
			// we need to allocate a new allocation block
			GvrsTileHashEntry** allocationBlocks = (GvrsTileHashEntry**)realloc(
				table->allocationBlocks,
				(table->nAllocationBlocks+1) * sizeof(GvrsTileHashEntry*));
			if (!allocationBlocks) {
				GvrsError = GVRSERR_NOMEM;
				return GVRSERR_NOMEM;
			}
			table->allocationBlocks = allocationBlocks;
			GvrsTileHashEntry* block = allocHashBlock();
			if (!block) {
				GvrsError = GVRSERR_NOMEM;
				return GVRSERR_NOMEM;
			}
			table->allocationBlocks[table->nAllocationBlocks] = block;
			table->nAllocationBlocks++;
			table->nEntriesUsedInBlock = 0;
			newEntries = block;
		}
		memmove(newEntries, bin->entries, bin->nEntries * sizeof(GvrsTileHashEntry));
		bin->entries = newEntries;
		bin->nAllocated = nNeeded;
		table->nEntriesUsedInBlock += nNeeded;
	}

	// Add the tile information to a new entry in the block.
	GvrsTileHashEntry* entry = bin->entries + (bin->nEntries);
	bin->nEntries++;
	entry->tileIndex = tileIndex;
	entry->tile = tile; 
	return 0;
}

static int hashTableRemove(GvrsTileCache* tc, GvrsTile* tile) {
	int i;
	int tileIndex = tile->tileIndex;
	int binIndex =  (int)(ihash(tileIndex) & GVRS_TILE_HASH_MASK);
	GvrsTileHashTable* table = tc->hashTable;
	GvrsTileHashBin* bin = table->bins + binIndex;
	int n1 = bin->nEntries - 1;
	for (i = 0; i < bin->nEntries; i++) {
		if (bin->entries[i].tileIndex == tileIndex) {
			// we're going to remove the entry from the bin.
			// if it is not the last entry in the array of entries,
			// we copy down the last entry over its position
			if (i < n1) {
				bin->entries[i] = bin->entries[n1];
			}
			bin->nEntries=n1;
			bin->entries[n1].tileIndex = -1; // mark entry as invalidated.
			bin->entries[n1].tile = 0;
			return 0;
		}
	}
	return -1;
}


static GvrsTileDirectory* readFailed(GvrsTileDirectory* td, int status, int* errCode) {
	GvrsTileDirectoryFree(td);
	*errCode = status;
	GvrsError = status;
	return 0;
}

GvrsTileDirectory* GvrsTileDirectoryRead(FILE* fp, GvrsLong filePosTileDirectory, int* errCode){
	*errCode = 0;
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


static GvrsLong getFilePositionByRowColumn(GvrsTileDirectory* tileDir, int tileRow, int tileCol) {
	int iRow = tileRow - tileDir->row0;
	int iCol = tileCol - tileDir->col0;
	int tileIndex = iRow * tileDir->nCols + iCol;
	if (tileDir->iOffsets) {
		GvrsLong t = (GvrsLong)(tileDir->iOffsets[tileIndex]);
		return t << 3;
	}
	else {
		return tileDir->lOffsets[tileIndex];
	}

}

static int readAndDecomp(Gvrs *gvrs, GvrsInt n, GvrsElement* element, GvrsByte* data) {
	GvrsByte* packing = (GvrsByte*)malloc(n);
	if (!packing) {
		GvrsError = GVRSERR_NOMEM;
		return GVRSERR_NOMEM;
	}
	int status = GvrsReadByteArray(gvrs->fp, n, packing);
	if (status) {
		free(packing);
		return status;
	}
	int i;
	int nRows = gvrs->nRowsInTile;
	int nCols = gvrs->nColsInTile;
	int nCells = nRows * nCols;
	int compressorIndex = (int)packing[0];
	status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
	if (gvrs->nDataCompressionCodecs > compressorIndex) {
		GvrsCodec* codec = gvrs->dataCompressionCodecs[compressorIndex];
		if (codec) {
			if (element->elementType == GvrsElementTypeInt && codec->decodeInt) {
				status = codec->decodeInt(nRows, nCols, n, packing, (GvrsInt *)data, codec->appInfo);
			}
			else if (element->elementType == GvrsElementTypeShort && codec->decodeInt) {
				GvrsInt* iData = (GvrsInt*)malloc(nCells * sizeof(GvrsInt));
				if (iData) {
					status = codec->decodeInt(nRows, nCols, n, packing, iData, codec->appInfo);
					if (status == 0) {
						for (i = 0; i < nCells; i++) {
							((short*)data)[i] = (short)iData[i];
						}
					}
					free(iData);
				}
			}
			else if (element->elementType == GvrsElementTypeFloat && codec->decodeFloat) {
				status = codec->decodeFloat(nRows, nCols, n, packing, (GvrsFloat *)data, codec->appInfo);
			}else if (element->elementType == GvrsElementTypeIntCodedFloat && codec->decodeInt) {
				status = codec->decodeInt(nRows, nCols, n, packing, (GvrsInt*)data, codec->appInfo);
			}else {
				status = GVRSERR_COMPRESSION_NOT_IMPLEMENTED;
			}
		}
	}
	free(packing);
	return status;

}

static int readTile(Gvrs* gvrs, GvrsLong tileOffset, GvrsTile*tile) {
	int i;
	FILE* fp = gvrs->fp;
	GvrsTileDirectory* tileDir = gvrs->tileDirectory;

	if (tileOffset == 0) {
		GvrsError = GVRSERR_FILE_ACCESS;
		return GVRSERR_FILE_ERROR;
	}
	GvrsSetFilePosition(fp, tileOffset);
	GvrsInt tileIndexFromFile;
	int status = GvrsReadInt(fp, &tileIndexFromFile); // a diagnostic
	if (status) {
		GvrsError = GVRSERR_FILE_ERROR;
		return GVRSERR_FILE_ERROR;
	}

	// TO DO:  handle other elements, etc.  Lot's of stuff needed here.

	// The tile "objects" from the cache are reused.  If this one was already used,
	// then the data pointer will be populated with a reference to the previously
	// allocated memory.  In that case, we just reuse the existing memory.
	// Otherwise, we need to allocate memory.
	if (!tile->data) {
		tile->data = calloc(1, gvrs->nBytesForTileData);
		if (!tile->data) {
			GvrsError = GVRSERR_NOMEM;
			return GVRSERR_NOMEM;
		}
	}
	
	for (i = 0; i < gvrs->nElementsInTupple; i++) {
		GvrsElement* element = gvrs->elements[i];
		GvrsInt n;
		GvrsReadInt(fp, &n);  // this will tell us if it's compressed or not
		if (n < element->dataSize) {
			// a compressed segment
			status = readAndDecomp(gvrs, n, element, tile->data + element->dataOffset);
		}
		else {
			status = GvrsReadByteArray(fp, element->dataSize, tile->data + element->dataOffset);
		}
		if (status) {
			return status;
		}
	}

	return 0;
}


static void moveLastTileToFreeList(GvrsTileCache* tc) {
	// the assumption is that this method will never be called
	// if there are fewer than 2 nodes on the tile list.
	GvrsTile *node = tc->lastNodeInTileList;
	GvrsTile* prior = node->prior;
	node->tileIndex = -1;
	prior->next = 0; // removes node from tile list
	tc->lastNodeInTileList = prior;
	node->prior = 0;
	node->next = tc->freeList;
	tc->freeList = node;
}

static void moveTileToHeadOfMainList(GvrsTileCache* tc, GvrsTile* node) {
	if (node == tc->tileList) {
		return; // should never happen if properly implemented
	}
	if (node->next) {
		node->next->prior = node->prior;
	}
	else {
		// The node was the last node in the tile list.
		// We need to update the last-node reference.
		// however, if there is only one node in the list,
		// the node will node have a prior and it will remain
		// the last node in the tile list with no further action required..
		if (node->prior) {
			tc->lastNodeInTileList = node->prior;
		}
	}
	if (node->prior) {
		node->prior->next = node->next;
	}

	node->prior = 0;
	node->next = tc->tileList;
	node->next->prior = node;
	tc->tileList = node;
}

static GvrsTile* moveFreeTileToMainList(GvrsTileCache* tc, int tileIndex) {
	GvrsTile* node = tc->freeList;
	tc->freeList = node->next;
	node->tileIndex = tileIndex;
	node->next = tc->tileList;
	// node->prior should already be null because it was at the head of the free list.
	if (node->next) {
		node->next->prior = node;
	} else {
		// the tile list was empty and the node will be the only
		// node on the tile list
		tc->lastNodeInTileList = node;
	}
	tc->tileList = node;
	return node;
}

GvrsTileCache* GvrsTileCacheAlloc(void* gvrspointer, int maxTileCacheSize) {
	Gvrs* gvrs = gvrspointer;
	if (maxTileCacheSize <= 0) {
		maxTileCacheSize = 16;
	}
	int i;
	GvrsTile* prior;
	GvrsTile* node;
	GvrsTileCache* tc = calloc(1, sizeof(GvrsTileCache));
	if (!tc) {
		GvrsError = GVRSERR_NOMEM;
		return 0;
	}
	tc->gvrs = gvrs;
	tc->firstTileIndex = -1;
	tc->maxTileCacheSize = maxTileCacheSize;
	tc->tileAllocation = calloc(tc->maxTileCacheSize, sizeof(GvrsTile));
	tc->freeList = tc->tileAllocation;
	// initialize the links for the free list
	// the firstNode->prior and lastNode->next will be null.
	if (!tc->freeList) {
		free(tc);
		GvrsError = GVRSERR_NOMEM;
		return 0;
	}
	prior = tc->freeList;
	prior->tileIndex = -1;
	for (i = 1; i < tc->maxTileCacheSize; i++) {
	    node = tc->freeList+i;
		node->tileIndex = -1;
		node->prior = prior;
		prior->next = node;
		prior = node;
	}

	tc->tileDirectory = gvrs->tileDirectory;
 
	 tc->nRowsInRaster = gvrs->nRowsInRaster;
	 tc->nColsInRaster = gvrs->nColsInRaster;
	 tc->nRowsInTile = gvrs->nRowsInTile;
	 tc->nColsInTile = gvrs->nColsInTile;
	 tc->nRowsOfTiles = gvrs->nRowsOfTiles;
	 tc->nColsOfTiles = gvrs->nColsOfTiles;
	 GvrsInt nTilesTotal = gvrs->nRowsOfTiles * gvrs->nColsOfTiles;

	 for (i = 0; i < tc->maxTileCacheSize; i++) {
		 tc->tileAllocation[i].allocationIndex = i;
	 }

	 tc->hashTable = hashTableAlloc();
	 if (!tc->hashTable) {
		 free(tc);
		 return 0;
	 }
	return tc;
}
 

GvrsTile* GvrsTileCacheFetchTile(GvrsTileCache *tc, int gridRow, int gridColumn, int *indexInTile, int* errCode) {
	 
	int nRowsInRaster = tc->nRowsInRaster;
	int nColsInRaster = tc->nColsInRaster;


	if (gridRow < 0 || gridRow >= nRowsInRaster || gridColumn < 0 || gridColumn >= nColsInRaster) {
		GvrsError = GVRSERR_COORDINATE_OUT_OF_BOUNDS;
		*errCode = GVRSERR_COORDINATE_OUT_OF_BOUNDS;
		return      0;
	}
	GvrsError = 0; // start off optimistic
	*errCode = 0;

	int nRowsInTile = tc->nRowsInTile;
	int nColsInTile = tc->nColsInTile;
	int nColsOfTiles = tc->nColsOfTiles;
	int tileRow = gridRow / nRowsInTile;
	int tileCol = gridColumn / nColsInTile;
	int tileIndex = tileRow * nColsOfTiles + tileCol;
	int rowInTile = gridRow - tileRow * nRowsInTile;
	int colInTile = gridColumn - tileCol * nColsInTile;
	*indexInTile = rowInTile * nColsInTile + colInTile;
	// *indexInTile = (gridRow - tileRow * nRowsInTile) * nColsInTile + (gridColumn - tileCol * nColsInTile);


	tc->nFetches++;
	if (tileIndex == tc->firstTileIndex) {
		return tc->tileList; // return the first node in the list
	}


	// Since the first tile in the list is not the target tile,
	// search the tile list for a matching index. There is a special case
	// if a tile has never been accessed, the tile list will be empty
	// and the tileList reference will be null.
	tc->nCacheSearches++;
	GvrsTile *node= hashTableLookup(tc, tileIndex);
	if (node) {
		// the node is already in the cache
		moveTileToHeadOfMainList(tc, node);
		tc->firstTileIndex = tileIndex;
		return node;
	}
 
 

	// The tile does not exist in the cache.  It will need to be read
	// from the source file.  Check to see if it is populated at all.
	GvrsLong tileOffset = getFilePositionByRowColumn(tc->tileDirectory, tileRow, tileCol);
	if (!tileOffset) {
		return 0; // tile not found
	}

	// The target tile is not in the cache
	if (!tc->freeList) {
		// all tiles are already committed.  we need to remove the
		// least-recently-used tile from the cache. 
		hashTableRemove(tc, tc->lastNodeInTileList);
		moveLastTileToFreeList(tc);
	}
	if (!tc->freeList) {
		GvrsError = GVRSERR_NOMEM;
		return 0;
	}

	tc->nTileReads++;
	node = tc->freeList;
	int status = readTile(tc->gvrs, tileOffset, node);
	if (status) {
		// The node will remain on the free list for future use
		node->tileIndex = -1;
		*errCode = status;
		return 0;
	}

	// The content was sucessfully read into the first node on the
	// free list.  We can now move it to the main cache
	moveFreeTileToMainList(tc, tileIndex);
	hashTablePut(tc, node);
	tc->firstTileIndex = tileIndex;


	return node;
}


GvrsTileCache* GvrsTileCacheFree(GvrsTileCache* cache) {
	if (cache) {
		int i;
		cache->hashTable = hashTableFree(cache->hashTable);
		for (i = 0; i < cache->maxTileCacheSize; i++) {
			if (cache->tileAllocation[i].data) {
				free(cache->tileAllocation[i].data);
				cache->tileAllocation[i].data = 0;
			}
		}
		free(cache->tileAllocation);
		cache->tileAllocation = 0;
		cache->freeList = 0;
		cache->tileList = 0;
		// the following references are managed elsewhere, but are nullified as a diagnostic
		cache->gvrs = 0;
		cache->tileDirectory = 0;

		free(cache);
	}
	return 0;
}

GvrsTileDirectory* GvrsTileDirectoryFree(GvrsTileDirectory* tileDirectory) {
	if (tileDirectory) {
		if(tileDirectory->iOffsets){
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
