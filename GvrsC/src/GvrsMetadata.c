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

static int cmpMetaRec(const void* p1, const void* p2) {
	GvrsMetadataReference* m1 = (GvrsMetadataReference*)p1;
	GvrsMetadataReference* m2 = (GvrsMetadataReference*)p2;
	int test = strcmp(m1->name, m2->name);
	if (test == 0) {
		if (m1->recordID > m2->recordID) {
			return 1;
		}
		else if (m1->recordID < m2->recordID) {
			return -1;
		}
		else {
			return 0;
		}
	}
	return test;
}

static const char* metadataTypeName[] = {
	"Unspecified",
	"Byte",
	"Short",
	"Short Unsigned",
	"Int",
	"Int Unsigned",
	"Float",
	"Double",
	"String",
	"ASCII",
};

static int metadataTypeBytesPerValue[] = {
	1,
	1,
	2,
	2,
	4,
	4,
	4,
	8,
	1,
	1
};


const char* GvrsMetadataGetTypeName(GvrsMetadataType mType) {
	if (0 <= mType && mType <= 9) {
		return metadataTypeName[mType];
	}
	return metadataTypeName[0]; // unspecified
}
 

GvrsMetadataDirectory* GvrsMetadataDirectoryFree(GvrsMetadataDirectory* dir) {
	if (dir) {
		if (dir->references) {
			free(dir->references);
			dir->references = 0;
		}
		free(dir);
	}
	return 0;
}

static int readFailed(GvrsMetadataDirectory* dir, int status) {
     GvrsMetadataDirectoryFree(dir);
	 return status;
}

int
GvrsMetadataDirectoryRead(FILE *fp, GvrsLong filePosMetadataDirectory, GvrsMetadataDirectory** directory) {
	int status;
	if (!fp || filePosMetadataDirectory == 0 || !directory) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*directory = 0;
	GvrsMetadataDirectory* dir = calloc(1, sizeof(GvrsMetadataDirectory));
	if (!dir) {
		return readFailed(dir, GVRSERR_NOMEM);
	}
	int i;
	if (filePosMetadataDirectory == 0) {
		// there is no metadata stored in the file, no further action required
		*directory = dir;
		return 0;
	}
 

	dir->filePosMetadataDirectory = filePosMetadataDirectory;
	status = GvrsSetFilePosition(fp, filePosMetadataDirectory);
	if (status) {
		return readFailed(dir, status);
	}


	GvrsInt nRecords;
	status = GvrsReadInt(fp, &nRecords);
	if (status) {
		return readFailed(dir, status);
	}


	dir->references = calloc(nRecords, sizeof(GvrsMetadataReference));
	if (!dir->references) {
		return readFailed(dir, GVRSERR_NOMEM);
	}
	dir->nMetadataReferences = nRecords;

	for (i = 0; i < nRecords; i++) {
		GvrsMetadataReference* r = dir->references + i;
		status = GvrsReadLong(fp, &r->filePos);
		if (status) {
			return readFailed(dir, status);
		}
		status = GvrsReadIdentifier(fp, sizeof(r->name), r->name);
		if (status) {
			return readFailed(dir, status);
		}

		status = GvrsReadInt(fp, &r->recordID);
		if (status) {
			return readFailed(dir, status);
		}
		GvrsByte typeCode;
		status = GvrsReadByte(fp, &typeCode);
		if (status) {
			return readFailed(dir, status);
		}
		if (typeCode < 0 || typeCode>9) {
			return readFailed(dir, GVRSERR_FILE_ERROR);
		}
		GvrsMetadataType metadataType = (GvrsMetadataType)typeCode;
		r->metadataType = metadataType;
	}

	if (nRecords > 1) {
		qsort(dir->references, nRecords, sizeof(GvrsMetadataReference), cmpMetaRec);
	}
	*directory = dir;
	return 0;
}

 

 
int GvrsMetadataRead(FILE* fp, GvrsMetadata **metadata) {

	GvrsMetadata *m = calloc(1, sizeof(GvrsMetadata));
	if (m) {
		*metadata = m;
	}else{
		return GVRSERR_NOMEM;
	}
	int status;
	status = GvrsReadIdentifier(fp, sizeof(m->name), m->name);
	if (status) {
		GvrsMetadataFree(m);
		return status;
	}
	GvrsReadInt(fp, &m->recordID);
	GvrsByte typeCode;
	GvrsReadByte(fp, &typeCode);
	m->metadataType = (GvrsMetadataType)typeCode;
	GvrsSkipBytes(fp, 3); // reserved for future use
	status = GvrsReadInt(fp, &m->dataSize);
	if (status) {
		GvrsMetadataFree(m);
		return status;
	}

	int n = m->dataSize;
	if (typeCode == GvrsMetadataTypeString || typeCode == GvrsMetadataTypeAscii) {
		// special-handling for the C-language.
		// The GVRS format gives a string value (String or ASCII) using a 4-byte integer
		// followed for the byte sequence for the content.  This configuration is slightly
		// redundant since the length of the string could also be determined by the dataSize element.
		// For the C-language, special handling is required because the the GVRS format does 
		// not guarantee that there is a null-terminator on an input string.
		// So when the metadata entity is read from the file, add one more by to store a null terminator.
		n++;
	}


	// it is possible to have an empty metadata element, though it's not encouraged.
	if (n > 0) {
		m->data = (GvrsByte*)malloc(n);
		if (!m->data) {
			GvrsMetadataFree(m);
			return GVRSERR_NOMEM;
		}
	}
	if (m->dataSize > 0) {
		status = GvrsReadByteArray(fp, m->dataSize, m->data);
		if (status) {
			GvrsMetadataFree(m);
			return status;
		}
	}
	if (n>0 && (typeCode == GvrsMetadataTypeString || typeCode == GvrsMetadataTypeAscii)) {
		m->data[n-1] = 0;
	}

	m->bytesPerValue = metadataTypeBytesPerValue[m->metadataType];
	m->nValues = m->dataSize / m->bytesPerValue;

	m->description = GvrsReadString(fp, &status);
 
	return 0;
}

GvrsMetadataResultSet* GvrsMetadataResultSetFree(GvrsMetadataResultSet* rs) {
	if (rs) {
		int i;
		for (i = 0; i < rs->nRecords; i++) {
			GvrsMetadataFree(rs->records[i]);
		}
		free(rs->records);
		free(rs);
	}
	return 0;
}

int  GvrsReadMetadataByNameAndID(Gvrs* gvrs, const char* name, int recordID, GvrsMetadataResultSet** resultSet) {
	*resultSet = 0;
	int i;
	if (!gvrs || !gvrs->fp) {
		return GVRSERR_NULL_ARGUMENT;
	}
	GvrsMetadataResultSet *rs = calloc(1, sizeof(GvrsMetadataResultSet));
	if (rs) {
		*resultSet = rs;
	}else{
		return GVRSERR_NOMEM;
	}

	GvrsMetadataDirectory* dir = gvrs->metadataDirectory;
	if (!dir || dir->nMetadataReferences == 0) {
		return 0;  // no further action required
	}

	rs->records = calloc(dir->nMetadataReferences, sizeof(GvrsMetadata *));
	if (!rs->records) {
		GvrsMetadataResultSetFree(rs);
		return GVRSERR_NOMEM;
	}

	FILE* fp = gvrs->fp;
	for (i = 0; i < dir->nMetadataReferences; i++) {
		GvrsMetadataReference r = dir->references[i];
		if ((*name == '*' || strcmp(name, r.name) == 0) && (recordID == INT32_MIN || recordID == r.recordID)) {
			int status = GvrsSetFilePosition(fp, r.filePos);
			if (status) {
				// non-zero status indicates an error
				GvrsMetadataResultSetFree(rs);
				return status;
			}
			//rs->references[rs->nRecords++] = GvrsMetadataRead(fp, &status);
			GvrsMetadata* m;
			status  = GvrsMetadataRead(fp, &m);
			if (status) {
				GvrsMetadataResultSetFree(rs);
				return status;
			}
			else {
				rs->records[rs->nRecords++] = m;
			}
		}
	
	}
	return 0;
}


static int argumentCheck(GvrsMetadata* metadata, int* nValues, void* reference) {
	if (!metadata || !nValues || !reference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	return 0;
}

int  GvrsMetadataGetString(GvrsMetadata* metadata, char **string) {
	int dummy; // TO DO: replace with string length?
	if (argumentCheck(metadata, &dummy, string)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*string = 0;

	// Within the body of the metadata, strings are stored in a different form
	// than elsewhere in the GVRS specification.  The length is given as a 4-byte
	// integer rather than the standard 2-byte integer.  This approach allows
	// the metadata to hold strings of length greater than 64 Kbytes, though it does
	// add complexity to the GVRS format.    For the C-lanaguage API, we don't need
	// this length specification, but we will have added a null-terminator to the
	// string when we read it from the file (see GvrsMetadataRead functions).

	if (metadata->metadataType == GvrsMetadataTypeAscii || metadata->metadataType == GvrsMetadataTypeString) {
		*string = (char *)(metadata->data+4);
		return 0;
	}
	else {
		return  GVRSERR_FILE_ACCESS;
	}
}


int
GvrsMetadataGetDoubleArray(GvrsMetadata* m, int* nValues, GvrsDouble** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeDouble) {
		*nValues = m->nValues;
		*data = (GvrsDouble*)(m->data);
		return 0;
	}
	else {
		*nValues = 0;
		return GVRSERR_FILE_ACCESS;
	}
}



int
GvrsMetadataGetFloatArray(GvrsMetadata* m, int* nValues, GvrsFloat** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeFloat) {
		*nValues = m->nValues;
		*data = (GvrsFloat*)(m->data);
		return 0;
	}
	else {
		*nValues = 0;
		return GVRSERR_FILE_ACCESS;
	}
}


int
GvrsMetadataGetShortArray(GvrsMetadata* m, int* nValues, GvrsShort** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeShort || m->metadataType == GvrsMetadataTypeUnsignedShort) {
		*nValues = m->nValues;
		*data = (GvrsShort*)(m->data);
		return 0;
	}
	else {
		*nValues = 0;
		return GVRSERR_FILE_ACCESS;
	}
}


int GvrsMetadataGetUnsignedShortArray(GvrsMetadata* m, int* nValues, GvrsUnsignedShort** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeShort || m->metadataType == GvrsMetadataTypeUnsignedShort) {
		*nValues = m->nValues;
		*data = (GvrsUnsignedShort*)(m->data);
		return 0;
	}
	else {
		*nValues = 0;
		return GVRSERR_FILE_ACCESS;
	}
}

 


int
GvrsMetadataGetIntArray(GvrsMetadata* m, int* nValues, GvrsInt** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeInt || m->metadataType == GvrsMetadataTypeUnsignedInt) {
		*nValues = m->nValues;
		*data = (GvrsInt*)(m->data);
		return 0;
	}else{
		*nValues = 0;
		return  GVRSERR_FILE_ACCESS;
	}
}

int
GvrsMetadataGetUnsignedIntArray(GvrsMetadata* m, int* nValues, GvrsUnsignedInt** data) {
	if (argumentCheck(m, nValues, data)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (m->metadataType == GvrsMetadataTypeInt || m->metadataType == GvrsMetadataTypeUnsignedInt) {
		*nValues = m->nValues;
		*data = (GvrsUnsignedInt*)(m->data);
		return 0;
	}
	else {
		*nValues = 0;
		return  GVRSERR_FILE_ACCESS;
	}
}

 

int GvrsMetadataGetByteArray(GvrsMetadata* metadata, int* nValues, GvrsByte** bytes) {
	if (argumentCheck(metadata, nValues, bytes)) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*nValues = metadata->dataSize;
	*bytes = metadata->data;
	return 0;
}

int
GvrsMetadataDirectoryAllocEmpty(Gvrs* gvrs, GvrsMetadataDirectory** directory) {
	if (!gvrs || !directory) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*directory = 0;
	GvrsMetadataDirectory* d = calloc(1, sizeof(GvrsMetadataDirectory));
	if (!d) {
		return  GVRSERR_NOMEM;
	}
	*directory = d;
	return 0;
}

static int computeMetadataSize(GvrsMetadata* metadata) {
	int sumStorage =
		2 + (int)strlen(metadata->name)  // GVRS string format for name
		+ 4 // recordID
		+ 1 // data type
		+ 3; // reserved
		sumStorage += 4 + metadata->dataSize;
		sumStorage += 2; // length of description, may be zero
		if (metadata->description) {
			sumStorage += (int)strlen(metadata->description);
		}
	return sumStorage;
}


int
GvrsMetadataWrite(Gvrs *gvrs, GvrsMetadata* metadata) {
	if (!gvrs || !metadata || !metadata->name[0]) {
		return GVRSERR_NULL_ARGUMENT;
	}
	FILE* fp = gvrs->fp;
	if (!fp) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (!gvrs->timeOpenedForWritingMS) {
		return GVRSERR_NOT_OPENED_FOR_WRITING;
	}


	// TO DO:  fill in rest:
	//   1. Write new metadata record to file.  keep track of file position
	//   2. Search directory to see if a matching metadata element is already in place.
	//      The match is based on the name and recordID elements.  
	//        yes:    a. because a matching reference is already in place, re-use the existing
	//                   reference in the directory's reference-record array.
	//                b. de-allocate the file space currently occupied by old metadata record
	//                c. replace file-position information in the directory's reference array.
	//                   Note that even though the name and recordID match, the other information
	//                   (data type, storage size, etc.) may have changed (not the best practice,
	//                   of course, but we support it).
	//        no:     a. reallocate the directory's reference array to be one larger
	//                b. add the new information into the reference array, maintaining the proper
	//                   sorting order...  find the position for new reference record in array
	//                   if it needs to be inserted, shift reference references as necessary       
	//                    populate the reference record as necessary
 

	int i;
	int status;
	GvrsMetadataDirectory* d = gvrs->metadataDirectory;
	if (gvrs->filePosMetadataDirectory) {
		// The directory hasn't been modified since the GVRS was opened.  So the directory that
		// was stored in the backing file must be marked for replacement.
		GvrsFileSpaceDealloc(gvrs->fileSpaceManager, gvrs->filePosMetadataDirectory);
		gvrs->filePosMetadataDirectory = 0;
	}
	d->writePending = 1;


	GvrsMetadataReference* mRef = 0;
	GvrsMetadataReference* match = 0;
	if (d->references) {
		int iRef = -1;
		for (i = 0; i < d->nMetadataReferences; i++) {
		    mRef = d->references + i;
			if (strcmp(mRef->name, metadata->name) == 0 && mRef->recordID == metadata->recordID) {
				match = mRef;
				iRef = i;
				break;
			}
		}
		if (iRef >= 0) {
			status = GvrsFileSpaceDealloc(gvrs->fileSpaceManager, mRef->filePos);
			mRef->filePos = 0;
			if (status) {
				return status;
			}
		}
	}

	int n = computeMetadataSize(metadata);
	GvrsLong filePos;
	status = GvrsFileSpaceAlloc(gvrs->fileSpaceManager, GvrsRecordTypeMetadata, n, &filePos);
	if (status) {
		return status;
	}
	GvrsWriteString(fp, metadata->name);
	GvrsWriteInt(fp, metadata->recordID);
	GvrsWriteByte(fp, (GvrsByte)(metadata->metadataType));
	GvrsWriteZeroes(fp, 3); // reserved for future use
	GvrsWriteInt(fp, metadata->dataSize);
 

	// it is possible to have an empty metadata element, though it's not encouraged.
	if (metadata->dataSize > 0) {
		status = GvrsWriteByteArray(fp, metadata->dataSize, metadata->data);
		if (status) {
			return status;
		}
	}
	 
	
	GvrsWriteString(fp, metadata->description);
	status =  GvrsFileSpaceFinish(gvrs->fileSpaceManager, filePos);
	if (status) {
		return status;
	}

	// replace an existing reference or insert the new reference into the directory
	if (match) {
		// we can simply replace the file position for the old record
		match->dataSize = metadata->dataSize;
		match->metadataType = metadata->metadataType;
		match->filePos = filePos;
	}
	else {
		// we need to insert the a new reference into the reference list
		int nRecords = d->nMetadataReferences;
		if (!d->references) {
			// the array has not been allocated yet.  nRecords is expected to be zero
			d->references = calloc(1, sizeof(GvrsMetadataReference));
			if (!d->references) {
				return GVRSERR_NOMEM;
			}
			mRef = d->references;
		}
		else {
			GvrsMetadataReference* a = calloc((size_t)(nRecords + 1), sizeof(GvrsMetadataReference));
			if (!a) {
				return GVRSERR_NOMEM;
			}
			int index = 0; 
			int comparison = 0;
			for (i = 0; i < nRecords; i++) {
				index = i;
				comparison = strcmp(d->references[i].name, metadata->name);
				if (comparison == 0) {
					if (d->references[i].recordID > metadata->recordID) {
						comparison = 1;
					}
					else {
						comparison = -1;
					}
				}
				if (comparison > 0) {
					break;
				}
			}
			for (i = 0; i < index; i++) {
				a[i] = d->references[i];
			}
			mRef = a + index;
			for (int i = index; i < nRecords; i++) {
				a[i + 1] = d->references[i];
			}
			free(d->references);
			d->references = a;

		}
		d->nMetadataReferences++;
		GvrsStrncpy(mRef->name, sizeof(mRef->name), metadata->name); 
		mRef->recordID = metadata->recordID;
		mRef->dataSize = metadata->dataSize;
		mRef->metadataType = metadata->metadataType;
		mRef->filePos = filePos;
	}
	return 0;
}

int  GvrsMetadataDirectoryWrite(void * gvrsReference, GvrsLong *filePosMetadataDirectory) {
	if (!gvrsReference || !filePosMetadataDirectory) {
		return GVRSERR_NULL_ARGUMENT;
	}

	Gvrs* gvrs = gvrsReference;
	FILE* fp = gvrs->fp;
	GvrsMetadataDirectory* d = gvrs->metadataDirectory;
	if (!fp || !d) {
		return GVRSERR_NULL_ARGUMENT;
	}

	*filePosMetadataDirectory = 0;
	if (!d->writePending) {
		return 0;
	}
	
	// compute size for storage
	int i;
	int n = 4;
	for (i = 0; i < d->nMetadataReferences; i++) {
		n += 8; // file filePos
		// name is 2 bytes plus the length of the string
		n += 2;
		n += (int)strlen(d->references[i].name);
		n += 4; // record ID
		n += 1; // data type
	}

	int status;
	GvrsLong filePos;
	status = GvrsFileSpaceAlloc(gvrs->fileSpaceManager, GvrsRecordTypeMetadataDir, n, &filePos);
	if (status) {
		return status;
	}
	status = GvrsWriteInt(fp, d->nMetadataReferences);
	for (i = 0; i < d->nMetadataReferences; i++) {
		GvrsMetadataReference* r = d->references + i;
		GvrsWriteLong(fp, r->filePos);
		GvrsWriteString(fp, r->name);
		GvrsWriteInt(fp, r->recordID);
		GvrsWriteByte(fp, (GvrsByte)(r->metadataType));
	}
	status = GvrsFileSpaceFinish(gvrs->fileSpaceManager, filePos);
	if (status) {
		return status;
	}
	*filePosMetadataDirectory = filePos;
	return 0;
}


static int checkIdentifier(const char* name, int maxLength) {
	if (!name) {
		return GVRSERR_BAD_NAME_SPECIFICATION;
	}
	size_t len = strlen(name);
	if (len == 0 || len > maxLength) {
		return  GVRSERR_BAD_NAME_SPECIFICATION;
	}
	if (!isalpha(name[0])) {
		// GVRS identifiers always start with a letter
		return GVRSERR_BAD_NAME_SPECIFICATION;
	}
	int i;
	for (i = 1; i < len; i++) {
		if (!isalnum(name[i]) && name[i] != '_') {
			// GVRS identifiers are a mix of letters, numerals, and underscores
			return  GVRSERR_BAD_NAME_SPECIFICATION;
		}
	}
	return 0;
}


int GvrsMetadataInit(const char* name, GvrsInt recordID,  GvrsMetadata** metadata) {
	if (!name || !*name || !metadata) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*metadata = 0;
	int status;

	status = checkIdentifier(name, GVRS_METADATA_NAME_SZ);
	if (status) {
		return status;
	}

	GvrsMetadata* m = calloc(1, sizeof(GvrsMetadata));
	if (!m) {
		return GVRSERR_NOMEM;
	}
	GvrsStrncpy(m->name, sizeof(m->name), name);
	m->recordID = recordID;
	m->metadataType = GvrsMetadataTypeUnspecified;
	m->bytesPerValue = 1;
	*metadata = m;
	return 0;
}


int GvrsMetadataSetAscii(GvrsMetadata* metadata, const char* string) {
	if (!metadata || !string) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (metadata->data) {
		// we will replace any existing data
		free(metadata->data);
		metadata->data = 0;
	}
	// at this time, the ASCII type (8 bit extended ASCII) is supported
	// but the string type (UTF-8) is not.
	metadata->metadataType = GvrsMetadataTypeAscii;
	metadata->bytesPerValue = 1;

	// allocate data to hold:
	//     string length specification (4 bytes)
	//     string content (n bytes)
	//     null terminator (1 byte)
	if (!*string) {
		metadata->data = calloc(5, sizeof(char));
		if (!metadata->data) {
			return GVRSERR_NOMEM;
		}
		metadata->nValues = 0;
		metadata->dataSize = 0;
	}
	else {
		// allocate room for the null terminator
		int n = (int)strlen(string);
		metadata->data = (GvrsByte*)malloc((n + 5));
		if (!metadata->data) {
			return GVRSERR_NOMEM;
		}
		metadata->data[0] = (GvrsByte)(n & 0xff);
		metadata->data[1] = (GvrsByte)((n>>8) & 0xff);
		metadata->data[2] = (GvrsByte)((n>>16) & 0xff);
		metadata->data[3] = (GvrsByte)((n>>24) & 0xff);
		memcpy(metadata->data + 4, string, n);
		metadata->data[n + 4] = 0;
		metadata->nValues = 1;
		metadata->dataSize = n + 4;
	}

	return 0;
}


int GvrsMetadataSetDouble(GvrsMetadata* metadata, int nValues, GvrsDouble* doubleRef) {
	size_t dataSize = nValues * sizeof(GvrsDouble);
	return GvrsMetadataSetData(metadata, GvrsMetadataTypeDouble, dataSize, doubleRef);
}

int GvrsMetadataSetShort(GvrsMetadata* metadata, int nValues, GvrsShort* shortRef) {
	size_t dataSize = nValues * sizeof(GvrsShort);
	return GvrsMetadataSetData(metadata, GvrsMetadataTypeShort, dataSize, shortRef);
}


int GvrsMetadataSetUnsignedShort(GvrsMetadata* metadata, int nValues, GvrsUnsignedShort* unsRef) {
	size_t dataSize = nValues * sizeof(GvrsUnsignedShort);
	return GvrsMetadataSetData(metadata, GvrsMetadataTypeUnsignedShort, dataSize, unsRef);
}
 

int GvrsMetadataSetData(GvrsMetadata* metadata,  GvrsMetadataType metadataType, size_t dataSize, void *data) {
	if (!metadata || !data || dataSize < 0) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (metadataType == GvrsMetadataTypeString || metadataType == GvrsMetadataTypeAscii) {
		// strings get special handling
		return GvrsMetadataSetAscii(metadata, (const char*)data);
	}

	int typeIndex = (int)metadataType;
	if (typeIndex < 0 || typeIndex>9) {
		return GVRSERR_FILE_ERROR;
	}

	if (metadata->data) {
		// we will replace any existing data
		free(metadata->data);
		metadata->data = 0;
	}
	metadata->dataSize = 0;
	metadata->nValues = 0;

	int nBytesPerValue = metadataTypeBytesPerValue[typeIndex];
	metadata->metadataType = metadataType;
	metadata->bytesPerValue = nBytesPerValue;

	int nValues = (int)dataSize / nBytesPerValue;
	if (nValues == 0) {
		return 0;
	}
	
	metadata->nValues = nValues;
	metadata->dataSize = nValues * nBytesPerValue;
	metadata->data = (GvrsByte*)malloc(metadata->dataSize);
	if (!metadata->data) {
		metadata->dataSize = 0;
		metadata->nValues = 0;
		return GVRSERR_NOMEM;
	}
	memmove(metadata->data, data, metadata->dataSize);
	return 0;
}


GvrsMetadata*
GvrsMetadataFree(GvrsMetadata* metadata) {
	if (metadata) {
		if (metadata->data) {
			free(metadata->data);
			metadata->data = 0;  // to satisfy MSVC code checker
		}
		if (metadata->description) {
			free(metadata->description);
			metadata->description = 0;
		}
		memset(metadata, 0, sizeof(GvrsMetadata)); // diagnostic
		free(metadata);
	}
	return 0;
}


int
GvrsMetadataDelete(Gvrs* gvrs, const char *name, int recordID) {
	if (!gvrs || !name || !name[0]) {
		return GVRSERR_NULL_ARGUMENT;
	}
	FILE* fp = gvrs->fp;
	if (!fp) {
		return GVRSERR_FILE_ERROR;
	}
	if (!gvrs->timeOpenedForWritingMS) {
		return GVRSERR_NOT_OPENED_FOR_WRITING;
	}
	GvrsMetadataDirectory* dir = gvrs->metadataDirectory;
	if (dir->references) {
		int i;
		for (i = 0; i < dir->nMetadataReferences; i++) {
			GvrsMetadataReference* ref = dir->references + i;
			if (strcmp(name, ref->name) == 0 && recordID == ref->recordID) {
				dir->writePending = 1;
				int status = GvrsFileSpaceDealloc(gvrs->fileSpaceManager, dir->references[i].filePos);
				if (status) {
					return status;
				}
				int n = dir->nMetadataReferences - 1;
				for (int j = i; j < n; j++) {
					dir->references[j] = dir->references[j + 1];
				}
				memset(dir->references + n, 0, sizeof(GvrsMetadataReference));
				dir->nMetadataReferences--;
				break;
			}
		}
	}
	return 0;
}