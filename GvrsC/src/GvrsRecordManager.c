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

#define RECORD_HEADER_SIZE     8
#define RECORD_OVERHEAD_SIZE  12
#define RECORD_CHECKSUM_SIZE   4


static char* recordTypeNames[] = {
	"Freespace",
	"Metadata",
	"Tile",
	"FilespaceDir",
	"MetadataDir",
	"TileDir",
	"Header"
};

// the following is an array of VALID record types
static GvrsRecordType recordTypeIndex[] = {
	GvrsRecordTypeFreespace,
	GvrsRecordTypeMetadata,
	GvrsRecordTypeTile ,
	GvrsRecordTypeFilespaceDir,
	GvrsRecordTypeMetadataDir,
	GvrsRecordTypeTileDir,
	GvrsRecordTypeHeader
};


const char *GvrsGetRecordTypeName(int index) {
	if (index < 0 || index >= sizeof(recordTypeNames)/sizeof(char *)) {
		return "Undefined";
	}
	return recordTypeNames[index];
}

GvrsRecordType GvrsGetRecordType(int index)
{
	if (index < 0 || index >= GVRS_RECORD_TYPE_COUNT) {
		return GvrsRecordTypeUndefined;
	}
	return recordTypeIndex[index];
}

static int multipleOf8(int value) {
	return (value + 7) & 0x7ffffff8;
}



GvrsLong
GvrsFileSpaceAlloc(Gvrs* gvrs, GvrsRecordType recordType, int sizeOfContent) {
	FILE* fp = gvrs->fp;
	GvrsFileSpaceManager* manager = gvrs->fileSpaceManager;

	int status;

	fseek(fp, 0, SEEK_END);
	GvrsLong recordPos = ftell(fp);
	int sizeToStore = multipleOf8(sizeOfContent + RECORD_OVERHEAD_SIZE);
	manager->recentRecordPosition = recordPos;
	manager->recentRecordSize = sizeToStore;
	manager->recentRecordType = recordType;

	status = GvrsWriteInt(fp, sizeToStore);
	status = GvrsWriteByte(fp, (GvrsByte)recordType);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	if (status) {
		GvrsError = status;
		return 0;
	}

	long startOfContent = (long)(recordPos + RECORD_HEADER_SIZE);
	manager->recentStartOfContent = startOfContent;
	return startOfContent;
}


int
GvrsFileSpaceFinish(Gvrs* gvrs, GvrsLong contentPos ) {
	FILE* fp = gvrs->fp;
	GvrsFileSpaceManager* manager = gvrs->fileSpaceManager;
	GvrsLong currentFilePos = ftell(fp);
	GvrsByte recordType;
	GvrsInt allocatedSize;
	int status;
	GvrsLong recordPos = contentPos - RECORD_HEADER_SIZE;
	if (recordPos == manager->recentRecordPosition) {
		allocatedSize = manager->recentRecordSize;
		recordType = manager->recentRecordType;

	}
	else {
		// we need to read the record header to find the size of the record
		fflush(fp);
		GvrsSetFilePosition(fp, recordPos);
		status = GvrsReadInt(fp, &allocatedSize);
		status = GvrsReadByte(fp, &recordType);  // diagnostic, should match expectation
		GvrsSetFilePosition(fp, currentFilePos);
	}
	manager->recentRecordPosition = 0;
	manager->recentStartOfContent = 0;
	manager->recentRecordSize = 0;

 
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
	//  If checksums are enabled, we will overwrite that value afterwards.

	GvrsLong targetPosition = recordPos + allocatedSize;
	GvrsLong shortfall = targetPosition - currentFilePos;
	if (shortfall > 0) {
		GvrsWriteZeroes(fp, (int)shortfall);
		fflush(fp);
	}

	GvrsUnsignedInt checksum = 0;
	if (gvrs->checksumEnabled) {
		GvrsSetFilePosition(fp, recordPos);
		GvrsByte* b = (GvrsByte*)malloc(allocatedSize);
		if (!b) {
			GvrsError = GVRSERR_NOMEM;
			return GVRSERR_NOMEM;
		}
		status = GvrsReadByteArray(fp, allocatedSize - 4, b);
		if (status == 0) {
			unsigned long crc = 0;
			crc = GvrsChecksumUpdateArray(b, 0, allocatedSize - 4, crc);
			checksum = (GvrsUnsignedInt)(crc & 0xFFFFFFFFL);
		}
		free(b);
	}
	GvrsSetFilePosition(fp, recordPos+allocatedSize - 4);
	status =  GvrsWriteInt(fp, checksum);   // this should be the checksum
	return status;

}
 
GvrsFileSpaceManager* GvrsFileSpaceManagerAlloc() {
	return calloc(1, sizeof(GvrsFileSpaceManager));
}

GvrsFileSpaceManager* GvrsFileSpaceManagerFree(GvrsFileSpaceManager* manager) {
	if (manager) {
		free(manager);
	}
	return 0;
}

