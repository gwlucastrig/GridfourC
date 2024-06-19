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
#include "GvrsError.h"

static int cmpMetaRec(const void* p1, const void* p2) {
	GvrsMetadataReference* m1 = (GvrsMetadataReference*)p1;
	GvrsMetadataReference* m2 = (GvrsMetadataReference*)p2;
	int test = strcmp(m1->name, m2->name);
	if (test == 0) {
		test = m1->recordID - m2->recordID;
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
		if (dir->records) {
			free(dir->records);
			dir->records = 0;
		}
		free(dir);
	}
	return 0;
}

static GvrsMetadataDirectory* readFailed(GvrsMetadataDirectory* dir, int status, int* errCode) {
	*errCode = status;
	GvrsError = status;
	return GvrsMetadataDirectoryFree(dir);
}

GvrsMetadataDirectory* GvrsMetadataDirectoryRead(FILE *fp, GvrsLong filePosMetadataDirectory, int *errCode) {
	*errCode = 0;
	GvrsMetadataDirectory* dir = calloc(1, sizeof(GvrsMetadataDirectory));
	if (!dir) {
		return readFailed(dir, GVRSERR_NOMEM, errCode);
	}
	int i;
	if (filePosMetadataDirectory == 0) {
		// there is no metadata stored in the file, no further action required
		return dir;
	}
	int status;

	status = GvrsSetFilePosition(fp, filePosMetadataDirectory);
	if (status) {
		return readFailed(dir, status, errCode);
	}


	GvrsInt nRecords;
	status = GvrsReadInt(fp, &nRecords);
	if (status) {
		return readFailed(dir, status, errCode);
	}


	dir->records = calloc(nRecords, sizeof(GvrsMetadataReference));
	if (!dir->records) {
		return readFailed(dir, GVRSERR_NOMEM, errCode);
	}
	dir->nMetadataRecords = nRecords;

	for (i = 0; i < nRecords; i++) {
		GvrsMetadataReference* r = dir->records + i;
		status = GvrsReadLong(fp, &r->offset);
		if (status) {
			return readFailed(dir, status, errCode);
		}
		status = GvrsReadIdentifier(fp, sizeof(r->name), r->name);
		if (status) {
			return readFailed(dir, status, errCode);
		}

		status = GvrsReadInt(fp, &r->recordID);
		if (status) {
			return readFailed(dir, status, errCode);
		}
		GvrsByte typeCode;
		status = GvrsReadByte(fp, &typeCode);
		if (status) {
			return readFailed(dir, status, errCode);
		}
		if (typeCode < 0 || typeCode>9) {
			return readFailed(dir, GVRSERR_FILE_ERROR, errCode);
		}
		GvrsMetadataType metadataType = (GvrsMetadataType)typeCode;
		r->metadataType = metadataType;
	}

	if (nRecords > 1) {
		qsort(dir->records, nRecords, sizeof(GvrsMetadataReference), cmpMetaRec);
	}
	return dir;
}

 
GvrsMetadata *GvrsMetadataFree(GvrsMetadata* m) {
	if (m) {
		if (m->data) {
			free(m->data);
		}
		free(m);
	}
	return 0;
}
 
GvrsMetadata* GvrsMetadataRead(FILE* fp, int *errorCode) {
	*errorCode = 0; // start off optimistic
	GvrsMetadata *m = calloc(1, sizeof(GvrsMetadata));
	if (!m) {
		*errorCode = GVRSERR_NOMEM;
		return 0;
	}
	int status;
	status = GvrsReadIdentifier(fp, sizeof(m->name), m->name);
	if (status) {
		*errorCode = status;
		return GvrsMetadataFree(m);
	}
	GvrsReadInt(fp, &m->recordID);
	GvrsByte typeCode;
	GvrsReadByte(fp, &typeCode);
	m->metadataType = (GvrsMetadataType)typeCode;
	GvrsSkipBytes(fp, 3); // reserved for future use
	status = GvrsReadInt(fp, &m->dataSize);
	if (status) {
		*errorCode = status;
		return GvrsMetadataFree(m);
	}

	// it is possible to have an empty metadata element, though it's not encouraged.
	if (m->dataSize > 0) {
		m->data = (GvrsByte*)malloc(m->dataSize);
		if (!m->data) {
			*errorCode = GVRSERR_NOMEM;
			return GvrsMetadataFree(m);
		}
	}

	if (typeCode == GvrsMetadataTypeString || typeCode == GvrsMetadataTypeAscii) {
		// special-handling for the C-language.
		// The GVRS format gives a string value (String or ASCII) using a 4-byte integer
		// followed for the byte sequence for the content.  This configuration is slightly
		// redundant since the length of the string could also be determined by the dataSize element.
		// For the C-language, special handling is required because the the GVRS format does 
		// not guarantee that there is a null-terminator on an input string.
		// So when the metadata entity is read from the file, this code adjusts the structure of the
		// data elements slightly to a form more compatible with C.
		GvrsSkipBytes(fp, 4);
		m->dataSize -= 4;
		status = GvrsReadByteArray(fp, m->dataSize, m->data);
		if (status) {
			*errorCode = status;
			return GvrsMetadataFree(m);
		}
		m->data[m->dataSize] = 0;
	}
	else {
		status = GvrsReadByteArray(fp, m->dataSize, m->data);
		if (status) {
			*errorCode = status;
			return GvrsMetadataFree(m);
		}
	}

	m->bytesPerValue = metadataTypeBytesPerValue[m->metadataType];
	m->nValues = m->dataSize / m->bytesPerValue;
	return m;
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

GvrsMetadataResultSet* GvrsMetadataReadByNameAndID(Gvrs* gvrs, const char* name, int recordID, int* errorCode) {
	int i;
	*errorCode = 0;
	if (!gvrs || !gvrs->fp) {
		*errorCode = GVRSERR_FILE_ERROR;
		return 0;
	}
	GvrsMetadataResultSet *rs = calloc(1, sizeof(GvrsMetadataResultSet));
	if (!rs) {
		*errorCode = GVRSERR_NOMEM;
		return 0;
	}

	GvrsMetadataDirectory* dir = gvrs->metadataDirectory;
	if (!dir || dir->nMetadataRecords == 0) {
		return rs; // no further action required.
	}

	rs->records = calloc(dir->nMetadataRecords, sizeof(GvrsMetadata *));
	if (!rs->records) {
		*errorCode = GVRSERR_NOMEM;
		return GvrsMetadataResultSetFree(rs);
	}

	FILE* fp = gvrs->fp;
	for (i = 0; i < dir->nMetadataRecords; i++) {
		GvrsMetadataReference r = dir->records[i];
		if ((*name == '*' || strcmp(name, r.name) == 0) && (recordID == INT32_MIN || recordID == r.recordID)) {
			int status = GvrsSetFilePosition(fp, r.offset);
			if (status) {
				// non-zero status indicates an error
				*errorCode = status;
				return GvrsMetadataResultSetFree(rs);
			}
			rs->records[rs->nRecords++] = GvrsMetadataRead(fp, errorCode);
			if (*errorCode) {
				return GvrsMetadataResultSetFree(rs);
			}

		}
	
	}
	return rs;
}


char* GvrsMetadataGetString(GvrsMetadata* metadata, int *errCode) {
	if (!metadata) {
		*errCode = GVRSERR_FILE_ACCESS;
		return 0;
	}

	// Within the body of the metadata, strings are stored in a different form
	// than elsewhere in the GVRS specification.  The length is given as a 4-byte
	// integer rather than the standard 2-byte integer.  This approach allows
	// the metadata to hold strings of length greater than 64 Kbytes, though it does
	// add complexity to the GVRS format.    For the C-lanaguage API, we don't need
	// this length specification, but we will have added a null-terminator to the
	// string when we read it from the file (see GvrsMetadataRead functions).

	if (metadata->metadataType == GvrsMetadataTypeAscii || metadata->metadataType == GvrsMetadataTypeString) {
		return metadata->data;
	}
	
	*errCode = GVRSERR_FILE_ACCESS;
	return 0;
}


GvrsDouble* GvrsMetadataGetDouble(GvrsMetadata *m, int* nValues) {
	*nValues = 0;
	if (!m || m->metadataType != GvrsMetadataTypeDouble) {
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
	*nValues = m->nValues;
	return (GvrsDouble*)(m->data);
}

GvrsFloat* GvrsMetadataGetFloat(GvrsMetadata* m, int* nValues) {
	*nValues = 0;
	if (!m || m->metadataType != GvrsMetadataTypeFloat) {
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
	*nValues = m->nValues;
	return (GvrsFloat*)(m->data);
}

GvrsShort* GvrsMetadataGetShort(GvrsMetadata* m, int* nValues) {
	if (m && (m->metadataType == GvrsMetadataTypeShort || m->metadataType == GvrsMetadataTypeUnsignedShort)) {
		*nValues = m->nValues;
		return (GvrsShort*)(m->data);
	}
	else {
		*nValues = 0;
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
}


GvrsUnsignedShort* GvrsMetadataGetUnsignedShort(GvrsMetadata* m, int* nValues) {
	if (m && (m->metadataType == GvrsMetadataTypeShort || m->metadataType == GvrsMetadataTypeUnsignedShort)) {
		*nValues = m->nValues;
		return (GvrsUnsignedShort*)(m->data);
	}
	else{
		*nValues = 0;
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
}



GvrsInt* GvrsMetadataGetInt(GvrsMetadata* m, int* nValues) {
	if (m && (m->metadataType == GvrsMetadataTypeInt || m->metadataType == GvrsMetadataTypeUnsignedInt)) {
		*nValues = m->nValues;
		return (GvrsInt*)(m->data);
	}else{
		*nValues = 0;
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
}


GvrsUnsignedInt* GvrsMetadataGetUnsignedInt(GvrsMetadata* m, int* nValues) {
	if (m && (m->metadataType == GvrsMetadataTypeInt || m->metadataType == GvrsMetadataTypeUnsignedInt)) {
		*nValues = m->nValues;
		return (GvrsUnsignedInt*)(m->data);
	}
	else {
		*nValues = 0;
		GvrsError = GVRSERR_FILE_ACCESS;
		return 0;
	}
}


GvrsByte* GvrsMetadataGetByte(GvrsMetadata* m, int* nValues) {
	*nValues = m->dataSize;
	return m->data;
}