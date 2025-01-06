// Test001_ReadRecords.c 
//    This file exercises the ability to read the GVRS record structure using the GvrsPrimaryIo read functions.
//    It accesses a GVRS file supplied at the command line.  While it does not attempt to interpret the
//    content of a record, it does access the structural metadata (record size, type, etc.).
//

#include <stdio.h>
#include <errno.h>
#include "GvrsPrimaryIo.h"
#include "Gvrs.h"

#define MAX_RECORD_TYPE_INDEX 6

// The following maps the integer record type code [0-to-7] to a descriptive string
static const char* recordTypeName[] = {
	"Freespace",
	"Metadata",
	"Tile",
	"FilespaceDir",
	"MetadataDir",
	"TileDir",
	"Header",
};

static void checkReadStatus(int status, FILE* fp, const char* message) {
	if (status == 0) {
		return; // read was okay
	}
	printf("Test failed reading file with status %d for %s\n", status, message);
	if (fp) {
		fclose(fp);
	}
	exit(-1);
}
 
static const char* usage[] = {
	"Usage:  Test001_ReadRecords  <input GVRS file>",
	0  // null-reference terminates the print loop
};

static void checkForUsage(int argc, char* argv[]) {
	if (argc < 2) {
		const char** p = usage;
		while (*p) {
			printf("%s\n", *p);
			p++;
		}
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	checkForUsage(argc, argv);

	const char* target = argv[1];

	printf("Test 001 Read GVRS record structure\n");
	printf("Input file %s\n", target);

	FILE* fp = fopen(target, "rb+");
	if (!fp) {
		printf("Error %d opening file %s\n", errno, target);
		exit(-1);
	}


	char buffer[64];
	int istat = GvrsReadASCII(fp, 12, sizeof(buffer), buffer);
	if (istat != 0 || strcmp(buffer, "gvrs raster")) {
		printf("Input target is not a valid GVRS file\n");
		fclose(fp);
		exit(-1);
	}

	uint8_t v1, v2;
	GvrsReadByte(fp, &v1);
	GvrsReadByte(fp, &v2);
	printf("File version:  %s %d.%d\n", buffer, v1, v2);

	GvrsSkipBytes(fp, 2);  // skip two reserved bytes


	// Each record is given in the form:
	//   uint32_t        record length in bytes
	//   uint8_t         record type in range 0-to-6
	//   uint8_t         reservd[3]  
	//   variable size   content
	//   uint32_t        checksum

	uint32_t  recordLength;   // record length in bytes
	uint8_t   recordType;     // the record type code

	
	int nRecordsToPrint = 20;  // you may adjust this if you are debugging

	uint64_t priorFilePos = 0;
	uint8_t priorType = 0;
	uint32_t priorSize = 0;

	int iRecord = 0;
	int tileIndex = 0;

	int i;
	int failureFlag = 0;
	int counts[MAX_RECORD_TYPE_INDEX + 1];
	memset(counts, 0, sizeof(counts));

	for (;;) {
		int64_t  pos = GvrsGetFilePosition(fp);
		errno = 0;
		size_t istat = GvrsReadUnsignedInt(fp, &recordLength);
		if (istat) {
			if (feof(fp)) {
				printf("Read operation successfully reached end of file\n");
			}
			else {
				printf("Read operation encountered error %d\n", errno);
				failureFlag = 1;
			}
			break;
		}

		istat = GvrsReadByte(fp, &recordType);
		if (istat || recordType > MAX_RECORD_TYPE_INDEX){
			printf("Invalid record type %d at position %lld, prior: %lld, type %d, size %d\n",
				(int)recordType, (long long)pos, (long long)priorFilePos, priorType, priorSize);
			failureFlag = 1;
			break;
		}
		 
		priorFilePos = pos;
		priorType = recordType;
		priorSize = recordLength;

		// If it is a free-space record (recordType==0), the content is meaningless
		// and GVRS does not consider it when computing the checksum
		// (ideally, the content is zeroed out for Information Assuracance (IA) purposes
		// but that is not absolutely required by the GVRS specification).

		int nBytesForChecksum;
		if (recordType == 0){
			nBytesForChecksum = 8; // just the record header
		}
		else {
			nBytesForChecksum = recordLength - 4;
		}
		GvrsSetFilePosition(fp, pos);
		uint8_t* bytes = (uint8_t*)calloc(1, (size_t)nBytesForChecksum);
		if (!bytes) {
			exit(-1);
		}
		GvrsReadByteArray(fp, nBytesForChecksum, bytes);

		// compute the checksum usign both the array and single-value methods. 
		// the results should be the same if everything works correctly.
		unsigned long crc1 = GvrsChecksumUpdateArray(bytes, 0, nBytesForChecksum, 0);
		unsigned long crc2 = 0;
		for (i = 0; i < nBytesForChecksum; i++) {
			crc2 = GvrsChecksumUpdateValue(bytes[i], crc2);
		}
		GvrsSetFilePosition(fp, pos + recordLength - 4);
		uint32_t checksum;
		size_t readStat = GvrsReadUnsignedInt(fp, &checksum);

		iRecord++;
		if (iRecord <= nRecordsToPrint) {
			if (iRecord == 1) {
				printf("\n");
				printf("Record  File Offset   Type    Length      Checksum   Computed Checksum\n");
			}
			printf("%6d %12lld   %4d  %8d      %08x   %08x\n",
				iRecord, (long long)pos, recordType, recordLength, checksum, (unsigned int)(crc1 & 0xffffffffu));
		}

		// If the compute-checksum option was not enabled, then GVRS requires that
		// the value zero be stored in the checksum position.
		int testSum = (int)(crc1 & 0xfffffffful);
		if (checksum != 0 && testSum != crc1) {
			printf("CRC failure %4d  %8d   %12u   %12u\n", recordType, recordLength, checksum, crc1);
			failureFlag = 1;
			break;
		}
		int index = (int)recordType;
		if (index < MAX_RECORD_TYPE_INDEX) {
			counts[index]++;
		}

	}

	fclose(fp);

	if (failureFlag) {
		exit(-1);
	}

	printf("\nCounts collected from %d records\n", iRecord);
	printf("Type   Name             Count\n");
	for (i = 0; i <= MAX_RECORD_TYPE_INDEX; i++) {
		printf("%4d   %-15s %6d\n", i, recordTypeName[i], counts[i]);
	}

	printf("\nAll tests passed\n");
	exit(0);

}