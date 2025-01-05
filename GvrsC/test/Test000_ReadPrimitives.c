// Test000_ReadPrimitives.c 
//    This file tests the GvrsPrimaryIo read functions.
//    It access the file resource/samples/SampleDataPrimitived.dat
//    which was created using the original Java API.
//
 
#include <stdio.h>
#include <errno.h>
#include "GvrsPrimaryIo.h"


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

static void testShort(FILE* fp, const char* testID, int16_t valueExpected) {
	int status;
	int16_t valueRead;

	status = GvrsReadShort(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (valueRead != valueExpected) {
		printf("Test failed for %s, expected 0x%04x, read %04x\n", testID, valueExpected, valueRead);
		exit(-1);
	}
}

static void testInteger(FILE* fp, const char* testID, int32_t valueExpected) {
	int status;
	int32_t valueRead;

	status = GvrsReadInt(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (valueRead != valueExpected) {
		printf("Test failed for %s, expected 0x%08x, read %08x\n", testID, valueExpected, valueRead);
		exit(-1);
	}
}


static void testFloat(FILE* fp, const char* testID, float valueExpected) {
	int status;
	float valueRead;

	status = GvrsReadFloat(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (valueRead != valueExpected) {
		printf("Test failed for %s, expected %f, read %f\n", testID, valueExpected, valueRead);
		exit(-1);
	}
}


static void testDouble(FILE* fp, const char* testID, double valueExpected) {
	int status;
	double valueRead;

	status = GvrsReadDouble(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (valueRead != valueExpected) {
		printf("Test failed for %s, expected %f, read %f\n", testID, valueExpected, valueRead);
		exit(-1);
	}
}

static void testString(FILE* fp, const char* testID, const char* valueExpected) {
	int status;
	char *valueRead;

	status = GvrsReadString(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (strcmp(valueExpected, valueRead)) {
		printf("Test failed for %s, expected \"%s\", read \"%s\"\n", testID, valueExpected, valueRead);
		exit(-1);
	}
	free(valueRead);
}


static void testLong(FILE* fp, const char* testID, int64_t valueExpected) {
	int status;
	int64_t valueRead;

	status = GvrsReadLong(fp, &valueRead);
	checkReadStatus(status, fp, testID);

	if (valueRead != valueExpected) {
		printf("Test failed for %s, expected 0x%016llx, read 0x%016llx\n", testID, (long long)valueExpected, (long long)valueRead);
		exit(-1);
	}
}


int main(int argc, char* argv[]) {

	const char* target = argv[1];

	printf("Test 000 Read Data Primitives\n");
	printf("Input file %s\n", target);

	FILE* fp = fopen(target, "rb+");
	if (!fp) {
		printf("Error %d opening file %s\n", errno, target);
		exit(-1);
	}

	testShort(fp, "read short 0", 0x01ff);
	testShort(fp, "read short 1", 0xff01);

	testInteger(fp, "read integer 0", 0x010203ff);
	testInteger(fp, "read integer 1", 0x0203ff01);
	testInteger(fp, "read integer 2", 0x03ff0102);
	testInteger(fp, "read integer 3", 0xff010203);

	float f = 1.0f + 1.0f / 256.0f;
	testFloat(fp, "read float 0", -f);
	testFloat(fp, "read float 1", f);

	double d = 1.0 + 1.0 / 256.0;
	testDouble(fp, "read double 0", -d);
	testDouble(fp, "read double 1", d);

	testString(fp, "read string 0", "Test data for GVRS");

	testLong(fp, "read long 0", 0x0102030405060708LL);
	testLong(fp, "read long 1", 0xff02030405060708LL);
	
	fclose(fp);

	printf("All tests passed\n");
	exit(0);
	
}