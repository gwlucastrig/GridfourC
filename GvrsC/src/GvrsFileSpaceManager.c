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


/*
* DEVELOPMENT NOTE:
* 
* This group of functions is unusually challenging to debug.  Be especially careful when
* making changes or enhancements.
* 
* Apparently, the C I/O libraries (fread, fwrite, etc.) require that the calling application
* performs either an fflush or fseek between calls when "changing direction" (changing from
* reading to writing or writing to reading).  This is a pretty lame requirement because it pushes
* the onus onto the calling application rather than handling it internally.  But that's the way
* it's done.
* 
* Here is some text from the C11 standard ISO/IEC 9899:2011, paragraph 7.21.5.3:
* 
*    output shall not be directly followed by input without an intervening call to the 
*    fflush function or to a file positioning function (fseek, fsetpos, or rewind),
*    and input shall not be directly followed by output without an intervening
*    call to a file positioning function...
* 
* TO DO: At this time, there are a lot of unnecessary calls to fflush in the code because I was
* trying to work out debugging issues.  We need to reduce this to just the necessary. 
*/

#include "GvrsFramework.h"
#include "GvrsPrimaryTypes.h"
#include "GvrsPrimaryIo.h"
#include "Gvrs.h"
#include "GvrsInternal.h"
#include "GvrsError.h"

#define RECORD_HEADER_SIZE     8
#define RECORD_OVERHEAD_SIZE  12
#define RECORD_CHECKSUM_SIZE   4

#define MIN_FREE_BLOCK_SIZE  32
 

// round value up to nearest multiple of 8.  if already a multiple of 8, return value.
static int multipleOf8(int value) {
	return (value + 7) & 0x7ffffff8;
}


int
GvrsFileSpaceAlloc(GvrsFileSpaceManager* manager, GvrsRecordType recordType, int sizeOfContent, GvrsLong* filePos) {
	if (!manager || !filePos) {
		return GVRSERR_NULL_ARGUMENT;
	}
	int status;
	*filePos = 0;
	manager->nAllocations++;
	FILE* fp = manager->fp;
	fflush(fp);
	fseek(fp, 0, SEEK_END);

	// compute the required block size.  The block size includes the overhead for the
	// record header and checksum (12 bytes) plus what ever padding
	// that is needed to make the size a multiple of 8.
	int blockSize = multipleOf8(sizeOfContent + RECORD_OVERHEAD_SIZE);
	int minSizeForSplit = blockSize + MIN_FREE_BLOCK_SIZE;
 

	// Search for a free-space block we can use to store data.  If we find one, then
	// at the end of the search, the variable "node" will be populated with a reference
	// to the appropriate free-space block and "prior" will point to the node in the list
	// that immediately preceeds it.  If we don't find one, then the variable node will
	// be assigned a null reference.  The variable "prior" will point to the last node
	// in the list (unless the list is empty, in which case it will be null)
	// and "priorPrior" will point to the predecessor of prior (if any).
	GvrsFileSpaceNode* priorPrior = 0;
	GvrsFileSpaceNode* prior = 0;
	GvrsFileSpaceNode* node = manager->freeList;
	while (node) {
		if (node->blockSize == blockSize || node->blockSize >= minSizeForSplit) {
			break;
		}
		priorPrior = prior;
		prior = node;
		node = node->next;
	}

	if (node) {
		// we found a free-space node that is large enough to meet the required allocation size
		manager->recentRecordPosition = node->filePos;
		manager->recentRecordSize = blockSize;
		manager->recentRecordType = recordType;
		long startOfContent = (long)manager->recentRecordPosition + (long)RECORD_HEADER_SIZE;
		manager->recentStartOfContent = startOfContent;
	
		if (node->blockSize == blockSize) {
			if (prior) {
				prior->next = node->next;
			}
			else {
				manager->freeList = node->next;
			}
			memset(node, 0, sizeof(GvrsFileSpaceNode));  //  diagnostic
			free(node);
		}else{
		    // node->blockSize > minSizeForSplit, split the storage space by reassigning
			// the node position+size and writing a new header record for the free space record
			// Node will stay in the free list, but its size and position will be adjusted.
			node->blockSize -= blockSize;
			node->filePos += blockSize;
			GvrsSetFilePosition(fp, node->filePos);
			GvrsWriteInt(fp, node->blockSize);
			GvrsWriteByte(fp, (GvrsByte)GvrsRecordTypeFreespace);
			GvrsWriteZeroes(fp, 3);
		}
		// Set the position for the newly allocated record and write the record header.  This action
		// will leave the file position set to the start of the content for the newly allocated record.
		GvrsSetFilePosition(fp, manager->recentRecordPosition);
		GvrsWriteInt(fp, blockSize);
		GvrsWriteByte(fp, (GvrsByte)recordType);
		int status = GvrsWriteZeroes(fp, 3);
		if (status) {
			return status;
		}
		*filePos = startOfContent;
		return 0;
	}


	// node is null, no available file-space block met the criteria for holding the intended element  
	// Extend the file size.  But first, check the last free node in the list.  If it is
	// at the end of the file, we can reuse the space that it occupies and just extend
	// the file size as necessary   The variable "prior" will point to the last free space node
	// in the file (if any).  And "priorPrior" will point to the node that preceeds prior (if any).
	fseek(fp, 0, SEEK_END);
	GvrsLong fileSize = ftell(fp);
	if (fileSize < manager->expectedFileSize) {
		int n = (int)(manager->expectedFileSize - fileSize);
		int status = GvrsWriteZeroes(fp, n);
		if (status) {
			return status;
		}
		fileSize = manager->expectedFileSize;
	}


	// If there is a free block located at the end of the file, we may be able to overwrite it and use it.
	// However, we can do so ONLY if the free block is smaller than the requested blockSize.
	// There is a possibility that the free block is larger than the requested size, but not
	// large enough for a split.  If it could have been split, it would have been picked up
	// in the logic above.  Since it can't be split, writing the newly allocated block over the free space
	// would result in some orphaned bytes and, much worse, would break the calculation for the next
	// record position based on block size and end-of-file conditions
	//   Note that one consequence of this logic is that the last record in the file must always
	// extend to the end of the file.
	//   Also, this approach is based on the assumption that we don't have an API to allow us to truncate
	// a data file.  We can research that for future applications.
	if (prior && prior->filePos + prior->blockSize == fileSize && prior->blockSize<blockSize) {
		// The end of the file includes a free block.  We can use its space and extend the
		// size of the file appropriately.  
		manager->recentRecordPosition = prior->filePos;
		manager->recentRecordSize = blockSize;
		manager->recentRecordType = recordType;
		long startOfContent = (long)manager->recentRecordPosition + (long)RECORD_HEADER_SIZE;
		manager->recentStartOfContent = startOfContent;
		manager->expectedFileSize = manager->recentRecordPosition + blockSize;
		
		if (priorPrior) {
			priorPrior->next = 0;
		}
		else {
			manager->freeList = 0;
		}
		free(prior);
	}
	else {
		// There are no free blocks that can be used for the output.
		// Just extend the file.
		manager->recentRecordPosition = fileSize;
		manager->recentRecordSize = blockSize;
		manager->recentRecordType = recordType;
		long startOfContent = (long)manager->recentRecordPosition + (long)RECORD_HEADER_SIZE;
		manager->recentStartOfContent = startOfContent;
		manager->expectedFileSize = manager->recentRecordPosition + blockSize;
	}
	// write out the record header
	GvrsSetFilePosition(fp, manager->recentRecordPosition);
	GvrsWriteInt(fp, manager->recentRecordSize);
	GvrsWriteByte(fp, (GvrsByte)recordType);
	int n = (int)(blockSize - RECORD_HEADER_SIZE) + 3;
	status = GvrsWriteZeroes(fp, n);
	if (status) {
		return status;
	}
	status = GvrsSetFilePosition(fp, manager->recentStartOfContent); 
	if (status) {
		return status;
	}
	*filePos = manager->recentStartOfContent;
	return 0;
}
 

int
GvrsFileSpaceFinish(GvrsFileSpaceManager* manager, GvrsLong contentPos) {
	manager->nFinish++;
	FILE* fp = manager->fp;
	fflush(fp);
	GvrsLong currentFilePos = ftell(fp);
	GvrsInt allocatedSize;
	int status = 0;
	GvrsLong recordPos = contentPos - RECORD_HEADER_SIZE;
	if (recordPos == manager->recentRecordPosition) {
		allocatedSize = manager->recentRecordSize;
	}
	else {
		return 0;
	}
	manager->recentRecordPosition = 0;
	manager->recentStartOfContent = 0;
	manager->recentRecordSize = 0;

	GvrsLong endOfRecord = recordPos + (GvrsLong)allocatedSize;

	// To finish the record, we need to ensure that the last 4 bytes are the checksum
	// (or zero if checksums are not used).  Also, due to the requirement that a record size
	// be a multiple of 8, the allocation may specify more bytes than the calling code
	// requested. In that case, the code needs to pad some zeroes.
	//     a.  Header: Allocated Size    4 bytes
	//     b.  Header: Record type+pad   4 bytes
	//     c.  Content           content size
	//     d.  Padding           (0 to 4, as needed)
	//     e.  Checksum          4 bytes (may be zeroes if checksums are not used.
	//  The record-overhead size includes both the record header and the checksum.
	//  Set up a target position that it reaches the end of the record.
	//  So we pad any remaining space in the record with zeroes, as needed.
	//  In doing so, this code will write a zero value into the 4-byte checksum at the end of the record. 
	//  If checksums are enabled, we will overwrite that value afterwards when the GVRS file is closed.

	if (contentPos <= currentFilePos && currentFilePos < endOfRecord) {
		GvrsLong shortfall = endOfRecord - currentFilePos;
		if (shortfall > 0) {
			GvrsWriteZeroes(fp, (int)shortfall);
		}
	}
	else {
		// The calling application overwrote the space that was allocated to it
		// for writing the record.   This is a pretty serious error because it could
		// lead to a corrupt file.
		//   TO DO:  implement some kind of debugging aid to tran this issue.
		return GVRSERR_INTERNAL_ERROR;
	}
	 
	return status;
}

int
GvrsFileSpaceDealloc(GvrsFileSpaceManager* manager, GvrsLong contentPosition) {
	manager->nDeallocations++;
	FILE* fp = manager->fp;
	int status;
	manager->recentRecordPosition = 0;
	manager->recentStartOfContent = 0;
	manager->recentRecordSize = 0;
	GvrsLong releasePos = contentPosition - (GvrsLong)RECORD_HEADER_SIZE;
	
	// There is a block of bytes in the file that is to be released.  It is defined by:
	//    releasePos     (position of record header)
	//    releaseSize    (includes size of record header and checksum)

	GvrsFileSpaceNode* prior = 0;
	GvrsFileSpaceNode* next = manager->freeList;
	while (next) {
		if (next->filePos >= releasePos) {
			break;
		}
		prior = next;
		next = next->next;
	}

	if (prior && prior->filePos + prior->blockSize > releasePos) {
		// DOUBLE DE-ALLOC error
		// This is a little protection that may or may not help.
		// the calling application tried to free a block more than once.
		// it might be a good idea to treat this as fatal, but we let it go for now.
		// "Double dealloc with overlapping block space\n");
		return 0;
	}

	if (next && next->filePos == releasePos) {
		// DOUBLE DE-ALLOC error
		return 0;
	}
 

	GvrsInt releaseSize;
	GvrsByte releaseType; // a diagnostic
	GvrsRecordType freeSpaceRecordType = GvrsRecordTypeFreespace;
	GvrsSetFilePosition(fp, releasePos);
	status = GvrsReadInt(fp, &releaseSize);
	status = GvrsReadByte(fp, &releaseType);
	if (status) {
		return status;
	}

	GvrsSetFilePosition(fp, releasePos + 4);
	status = GvrsWriteByte(fp, freeSpaceRecordType);
	if (status) {
		return status;
	}


	if (!prior && !next) {
		// The free-space list does not currently include any entries
		GvrsFileSpaceNode* node = calloc(1, sizeof(GvrsFileSpaceNode));
		if (node) {
			node->blockSize = releaseSize;
			node->filePos = releasePos;
			manager->freeList = node;
			return 0;
		}
		else {
			return GVRSERR_NOMEM;
		}
	}


	// see if we can merge the prior block with the newly deallocated block
	if (prior && prior->filePos + prior->blockSize == releasePos) {
		// we can merge prior with the release block, no new node is required
		prior->blockSize += releaseSize;

		// There is a chance that the release block was located between two
		// free blocks and that by deallocating it, we can also merge the prior
		// block with the next block.
		if (next && next->filePos == prior->filePos + prior->blockSize) {
			prior->blockSize += next->blockSize;
			prior->next = next->next;
			memset(next, 0, sizeof(GvrsFileSpaceNode)); // a diagnostic
			free(next);
		}

		GvrsSetFilePosition(fp, prior->filePos);
		status = GvrsWriteInt(fp, prior->blockSize);
		fflush(fp);
		return status;
	}

	// the release block was not adjacent to the prior block.  check to see
	// if it is adjacent to the next block.  If so, we can merge the blocks and
	// no new node will be added to the free-space list
	if (next && next->filePos == releasePos + releaseSize) {
		next->filePos = releasePos;
		next->blockSize += releaseSize;
		GvrsSetFilePosition(fp, next->filePos);
		status = GvrsWriteInt(fp, next->blockSize);
		status = fflush(fp);
		return status;
	}

	// If we reached this point, the released block is not adjacent to any existing
	// free blocks.  Create a new node and insert it into the list. 

	GvrsFileSpaceNode* node = calloc(1, sizeof(GvrsFileSpaceNode));
	if (!node) {
		return GVRSERR_NOMEM;
	}

	node->filePos = releasePos;
	node->blockSize = releaseSize;
	node->next = next;
	if (prior) {
		prior->next = node;
	}
	else {
		manager->freeList = node;
	}
	return 0;
}
 
GvrsFileSpaceManager* GvrsFileSpaceManagerAlloc(FILE *fp) {
	// Ensure that the initial file size is a multiple of 8.
	// This step is necessary to ensure that the manager provides
	// eight-byte alignment for all subsequent calls.
	fflush(fp);
	int status = fseek(fp, 0, SEEK_END);
	if (status) {
		// A file error.  Unable to initialize 
		return 0;
	}
	GvrsLong filePos = ftell(fp);
	int misAlignment = (int)(filePos & 0x07L);
	if (misAlignment) {
		int n = 8 - misAlignment;
		status = GvrsWriteZeroes(fp, n);
		filePos += n;
		if (status) {
			return 0;
		}
	}

	GvrsFileSpaceManager* m = calloc(1, sizeof(GvrsFileSpaceManager));
	if (m) {
		m->fp = fp;
		m->expectedFileSize = filePos;
	}
	return m;
}

GvrsFileSpaceManager* GvrsFileSpaceManagerFree(GvrsFileSpaceManager* manager) {
	if (manager) {
		if (manager->freeList) {
			GvrsFileSpaceNode* node = manager->freeList;
			while (node) {
				GvrsFileSpaceNode* x = node;
				node = node->next;
				free(x);
			}
		}
		memset(manager, 0, sizeof(GvrsFileSpaceManager));  // diagnostic
		free(manager);
	}
	return 0;
}


#define BYTES_PER_FS_NODE 12
int GvrsFileSpaceDirectoryWrite(Gvrs* gvrs, GvrsLong* filePosition) {

	*filePosition = 0;
	GvrsFileSpaceManager* manager = gvrs->fileSpaceManager;
	GvrsFileSpaceNode* node = manager->freeList;
	int kNode = 0;
	while (node) {
		kNode++;
		node = node->next;
	}
	if (kNode == 0) {
		return 0;
	}

	// for the bytes required, we compute:
	//     number of free nodes
	//     12 bytes per node (file pos and block size)
	//     12 bytes extra in case we need to add one more node to the file
	//        when we allocate additional file space (note that this action could
	//        actually reduce the number of free nodes if it merges blocks).
	int nBytesRequired = 4 + (kNode+1)*BYTES_PER_FS_NODE;
	int status;
	GvrsLong contentPos;
	status = GvrsFileSpaceAlloc(manager, GvrsRecordTypeFilespaceDir, nBytesRequired, &contentPos);
	if (!contentPos) {
		return status;
	}
	*filePosition = contentPos;
	// count the nodes again, in case the count changed
    node = manager->freeList;
	kNode = 0;
	while (node) {
		kNode++;
		node = node->next;
	}

	FILE* fp = manager->fp;
	GvrsWriteInt(fp, kNode);
	node = manager->freeList;
	while (node) {
		status = GvrsWriteLong(fp, node->filePos);
		status = GvrsWriteInt(fp, node->blockSize);
		if (status) {
			return status;
		}
		node = node->next;
	}
	return GvrsFileSpaceFinish(manager, contentPos);
}

GvrsFileSpaceManager *
GvrsFileSpaceDirectoryRead(Gvrs* gvrs, GvrsLong fileSpaceDirectoryPosition, int *errCode) {
	FILE* fp = gvrs->fp;
	*errCode = 0;
	GvrsFileSpaceManager* manager = GvrsFileSpaceManagerAlloc(fp);
	if (!manager) {
		*errCode = GVRSERR_NOMEM;
		return 0;
	}
	if (fileSpaceDirectoryPosition) {
		int status;
		GvrsInt nFreeSpaceRecords;
		status = GvrsSetFilePosition(fp, fileSpaceDirectoryPosition);
		if (status) {
			*errCode = status;
			return manager;
		}
		status = GvrsReadInt(fp, &nFreeSpaceRecords);
		if (status) {
			*errCode = status;
			return manager;
		}
		GvrsFileSpaceNode* prior = 0;
		int i;
		for (i = 0; i < nFreeSpaceRecords; i++) {
			GvrsLong filePos;
			GvrsInt blockSize;
			int status1, status2;
			status1 = GvrsReadLong(fp, &filePos);
			status2 = GvrsReadInt(fp, &blockSize);
			if (status1 || status2) {
				*errCode = status2;
				return manager;

			}
			GvrsFileSpaceNode* node = calloc(1, sizeof(GvrsFileSpaceNode));
			if (!node) {
				*errCode =  GVRSERR_NOMEM;
				return manager;
			}
			node->filePos = filePos;
			node->blockSize = blockSize;
			if (prior) {
				prior->next = node;
			}
			else {
				manager->freeList = node;
			}
			prior = node;
		}
	}
	return manager;
}
