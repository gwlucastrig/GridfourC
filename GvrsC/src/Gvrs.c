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
#include <math.h>


GvrsCodec* GvrsCodecHuffmanAlloc();
#ifdef GVRS_ZLIB
GvrsCodec* GvrsCodecDeflateAlloc();
GvrsCodec* GvrsCodecFloatAlloc();
GvrsCodec* GvrsCodecLsopAlloc();
#endif


static double to180(double angle) {
	if (-180 <= angle && angle < 180) {
		return angle;
	}
	double a = fmod(angle, 360.0);
	if (a < -180.0) {
		return 360.0 + a;
	}
	else if (a >= 180.0) {
		return a - 360.0;
	}
	else if (a == -0.0) {
		return 0.0; // avoid -0.0 condition
	}
	return a;
}


static GvrsCodec* destroyCodecPlaceholder(GvrsCodec* codec) {
	if (codec) {
		if (codec->description) {
			free(codec->description);
		}
		free(codec);
	}
	return 0;
}

static GvrsCodec* createCodecPlaceholder(const char *identification) {
	GvrsCodec* codec = calloc(1, sizeof(GvrsCodec));
	if (!codec) {
		return 0;
	}
	GvrsStrncpy(codec->identification, sizeof(codec->identification), identification);
	codec->description = GVRS_STRDUP("Unimplemented compressor");
	codec->destroyCodec = destroyCodecPlaceholder;
	return codec;
}


static Gvrs *fail(Gvrs *gvrs, FILE *fp ,int code) {
	if (gvrs) {
		GvrsClose(gvrs);
	}
	else if (fp) {
		// the file pointer wasn't yet copied into the Gvrs structure
		fclose(fp);
	}
	GvrsError = code;
	return 0;
}

static void skipToMultipleOf4(FILE* fp) {
	long pos = ftell(fp);
	int k = (int)(pos & 0x3);
	if (k > 0) {
		GvrsSkipBytes(fp, 4 - k);
	}
}

static void readAffineTransform(FILE *fp, GvrsAffineTransform *transform) {
	GvrsReadDouble(fp, &transform->a00);
	GvrsReadDouble(fp, &transform->a01);
	GvrsReadDouble(fp, &transform->a02);
	GvrsReadDouble(fp, &transform->a10);
	GvrsReadDouble(fp, &transform->a11);
	GvrsReadDouble(fp, &transform->a12);
}

static char* freeString(char* s) {
	if (s) {
		free(s);
	}
	return 0;
}
static GvrsElement* freeElement(GvrsElement* element) {
	if (element) {
		element->name[0] = 0;
		element->label = freeString(element->label);
		element->description = freeString(element->description);
		element->unitOfMeasure = freeString(element->unitOfMeasure);
		// the following references are managed elsewhere, but are nullified as a diagnostic.
		element->tileCache = 0;
		element->gvrs = 0;
		// We are going to free the element.  even so, we zero it out as a diagnostic.
		// Some external code may hold references to the elements.  Zero it out to prevent
		// an external function from accidentally using trying to access the element
		memset(element, 0, sizeof(GvrsElement));
		free(element);
	}
	return 0;
}

static GvrsElement* readElement(Gvrs* gvrs, int iElement, int nCellsInTile, int offsetWithinTileData) {
	FILE* fp = gvrs->fp;
	GvrsElement* element = calloc(1, sizeof(GvrsElement));
	if (!element) {
		return (GvrsElement*)0;
	}
	element->elementIndex = iElement;
	element->dataOffset = offsetWithinTileData;

	int status;
	GvrsByte eType;
	status = GvrsReadByte(fp, &eType);
	if (status || eType < 0 || eType>3) {
		free(element);
		return (GvrsElement*)0;
	}
	GvrsReadBoolean(fp, &element->continuous);
	GvrsSkipBytes(fp, 6); // reserved for future use
	status = GvrsReadIdentifier(fp, sizeof(element->name), element->name);
	skipToMultipleOf4(fp); // needed because the name often breaks byte-alignment
	// TO DO:  in the following, most of the types aren't completely implemented.
	//         the code skips the appropriate bytes.
	switch ((GvrsElementType)eType) {
	case GvrsElementTypeInt:
		element->elementType = GvrsElementTypeInt;
		element->typeSize = 4;
		GvrsElementSpecInt* intSpec = &(element->elementSpec.intSpec);
		GvrsReadInt(fp, &intSpec->minValue);
		GvrsReadInt(fp, &intSpec->maxValue);
		GvrsReadInt(fp, &intSpec->fillValue);
		element->fillValueInt = intSpec->fillValue;
		element->fillValueFloat = (float)intSpec->fillValue;
		break;
	case GvrsElementTypeIntCodedFloat:
		element->elementType = GvrsElementTypeIntCodedFloat;
		element->typeSize = 4;
		GvrsElementSpecIntCodedFloat* icfSpec = &(element->elementSpec.intFloatSpec);
		GvrsReadFloat(fp, &icfSpec->minValue);
		GvrsReadFloat(fp, &icfSpec->maxValue);
		GvrsReadFloat(fp, &icfSpec->fillValue);
		GvrsReadFloat(fp, &icfSpec->scale);
		GvrsReadFloat(fp, &icfSpec->offset);
		GvrsReadInt(fp, &icfSpec->iMinValue);
		GvrsReadInt(fp, &icfSpec->iMaxValue);
		GvrsReadInt(fp, &icfSpec->iFillValue);
		element->fillValueInt = icfSpec->iFillValue;
		element->fillValueFloat = icfSpec->fillValue;
		break;
	case GvrsElementTypeFloat:
		element->elementType = GvrsElementTypeFloat;
		element->typeSize = 4;
		GvrsElementSpecFloat* floatSpec = &(element->elementSpec.floatSpec);
		GvrsReadFloat(fp, &floatSpec->minValue);
		GvrsReadFloat(fp, &floatSpec->maxValue);
		GvrsReadFloat(fp, &floatSpec->fillValue);
		element->fillValueInt = (int)floatSpec->fillValue;
		element->fillValueFloat = floatSpec->fillValue;
		break;
	case GvrsElementTypeShort:
		element->elementType = GvrsElementTypeShort;
		element->typeSize = 2;
		GvrsElementSpecShort* shortSpec = &(element->elementSpec.shortSpec);
		GvrsReadShort(fp, &shortSpec->minValue);
		GvrsReadShort(fp, &shortSpec->maxValue);
		GvrsReadShort(fp, &shortSpec->fillValue);
		element->fillValueInt = shortSpec->fillValue;
		element->fillValueFloat = shortSpec->fillValue;
		break;
	default:
		break; // no action required
	}

	element->label = GvrsReadString(fp, &status);
	element->description = GvrsReadString(fp, &status);
	element->unitOfMeasure = GvrsReadString(fp, &status);
	skipToMultipleOf4(fp);

	// compute number of bytes needed for tile.  Note that this size
	// is adjusted to be a multiple of 4, if necessary.  This action
	// ensures byte alignment within the data.  For example, an adjustment
	// would be necessary for a 16-bit integer type (2 bytes) with an 
	// odd number of cells in the tile.

	int n = nCellsInTile * element->typeSize;
	element->dataSize = GVRS_MULTI_4(n);

	element->gvrs = gvrs;

	// The unit-to-meters conversion factor is optional.  It is intended to support
	// cases where an interpolator calculates first derivatives for a surface
	// given in geographic coordinates.  Calculations of that type often require that
	// the dependent variable (the element value) is isotropic to the underlying
	// (model) coordinates.  In the "does not apply" cases, just set this value to 1.
	//    At present, the GVRS specification does not identify a formal standard for
	// unit-of-measure abbreviations or conversion factors.  Therefore, this setting
	// is of limited usefulness.
	element->unitsToMeters = 1.0;
	if (element->unitOfMeasure) {
		// since Windows does not support strcasecmp, we convert
		// the input string to lower case.
		char s[32];
		int i;
		for (i = 0; i < 32; i++) {
			char c = element->unitOfMeasure[i];
			if (c == 0) {
				s[i] = 0;
				break;
			}
			else if (isupper(c)) {
				c = tolower(c);
			}
			s[i] = c;
		}
		s[31] = 0;
		if (strcmp("f", s) == 0 || strcmp("ft", s) == 0 || strcmp("feet", s) == 0) {
			element->unitsToMeters = 0.3048;  // multiplicative feet to meters
		}
		else if (strcmp("y", s)==0 || strcmp("yards", s) == 0 || strcmp("yd", s) == 0 || strcmp("yrd", s) == 0) {
			element->unitsToMeters = 0.9144; // multiplicative yards to meters
		}
		else if (strcmp("fathoms", s) == 0 || strcmp("fm", s) == 0 || strcmp("fms", s) == 0) {
			element->unitsToMeters = 1.8388;  // multiplicative fathoms to meters
		}
	}

	return element;
}


int GvrsSetTileCacheSize(Gvrs* gvrs, GvrsTileCacheSizeType cacheSize) {
	int i, n;
	if (cacheSize < 0 || cacheSize>3) {
		// improper specification from application code.
		cacheSize = GvrsTileCacheSizeMedium;
	}
	gvrs->tileCacheSize = cacheSize;
	n = GvrsTileCacheComputeStandardSize(gvrs->nRowsOfTiles, gvrs->nColsOfTiles, cacheSize);

	GvrsTileCache* tileCache = (GvrsTileCache *)gvrs->tileCache;
	gvrs->tileCache = 0;
	if (tileCache) {
		if (tileCache->maxTileCacheSize == n) {
			return 0;
		}
		int status = GvrsTileCacheWritePendingTiles(tileCache);
		if (status) {
			return status;
		}
		GvrsTileCacheFree(tileCache);
	}
	tileCache = GvrsTileCacheAlloc(gvrs, n);
	if (tileCache) {
		gvrs->tileCache = tileCache;
		for (i = 0; i < gvrs->nElementsInTupple; i++) {
			gvrs->elements[i]->tileCache = tileCache;
		}
		return 0;
	}
	return GvrsError;
}

 

Gvrs *GvrsOpen(const char* path, const char* accessMode) {
	errno = 0;
	int status;
	int iElement;
 
	Gvrs* gvrs = 0;
	FILE* fp = fopen(path, "rb+");

	if (!fp) {
		if (errno == EACCES) {
			return fail(gvrs, fp, GVRSERR_FILE_ACCESS);
		}
		else if(errno == ENOENT){
			return fail(gvrs, fp, GVRSERR_FILENOTFOUND);
		}
		return fail(gvrs, fp, GVRSERR_FILE_ERROR);
	}

	int openedForWriting = 0;
	const char* p = accessMode;
	while (p && *p) {
		if (*p == 'w' || *p == 'W') {
			openedForWriting = 1;
			break;
		}
		p++;
	}
 

	// As this function is reading the file, it checks the return status
	// from read functions only at critical points in the code.  It does not check
	// the status for every read operation because that would clutter the code
	// and reduce its readability without significantly improving the functionality.

	char buffer[64];
	int istat = GvrsReadASCII(fp, 12, sizeof(buffer), buffer);
	if (istat!=0 || strcmp(buffer, "gvrs raster")) {
		return fail(gvrs, fp, GVRSERR_INVALID_FILE);
	}

	unsigned  char v1, v2;
	GvrsReadByte(fp, &v1);
	GvrsReadByte(fp, &v2);
	if (v1 != 1 && v2 < 4) {
		return fail(gvrs, fp, GVRSERR_VERSION_NOT_SUPPORTED);
	}

	gvrs = calloc(1, sizeof(Gvrs));
	if (!gvrs) {
		return fail(gvrs, fp, GVRSERR_NOMEM);
	}
	gvrs->fp = fp;
	gvrs->path = GVRS_STRDUP(path);
	if (!path) {
		return fail(gvrs, fp, GVRSERR_NOMEM);
	}

	GvrsSkipBytes(fp, 2);
	GvrsInt sizeOfHeaderInBytes;
	GvrsReadInt(fp, &sizeOfHeaderInBytes);
	gvrs->offsetToContent = sizeOfHeaderInBytes;
	GvrsSkipBytes(fp, 4);
	
	GvrsReadLong(fp, &gvrs->uuidLow);
	GvrsReadLong(fp, &gvrs->uuidHigh);
 
	GvrsReadLong(fp, &gvrs->modTimeMS);
	gvrs->modTimeSec = gvrs->modTimeMS / 1000LL;

	GvrsReadLong(fp, &gvrs->timeOpenedForWritingMS);
	if (gvrs->timeOpenedForWritingMS) {
		return fail(gvrs, fp, GVRSERR_EXCLUSIVE_OPEN);
	}

 
	GvrsReadLong(fp, &gvrs->filePosFreeSpaceDirectory);
	GvrsReadLong(fp, &gvrs->filePosMetadataDirectory);

	GvrsShort nLevels = 0;
	GvrsReadShort(fp, &nLevels);

	GvrsSkipBytes(fp, 6);

	GvrsReadLong(fp, &gvrs->filePosTileDirectory);

	GvrsSkipBytes(fp, 16);

	GvrsReadInt(fp, &gvrs->nRowsInRaster);
	GvrsReadInt(fp, &gvrs->nColsInRaster);
	GvrsReadInt(fp, &gvrs->nRowsInTile);
	GvrsReadInt(fp, &gvrs->nColsInTile);
	gvrs->nRowsOfTiles = (gvrs->nRowsInRaster + gvrs->nRowsInTile - 1) / gvrs->nRowsInTile;
	gvrs->nColsOfTiles = (gvrs->nColsInRaster + gvrs->nColsInTile - 1) / gvrs->nColsInTile;
	gvrs->nCellsInTile = gvrs->nRowsInTile * gvrs->nColsInTile;

	GvrsSkipBytes(fp, 8);
	GvrsByte scratch;
	GvrsReadBoolean(fp, &gvrs->checksumEnabled);
	GvrsReadByte(fp, &scratch);
	gvrs->rasterSpaceCode = scratch;
	GvrsReadByte(fp, &scratch);
	gvrs->geographicCoordinates = (scratch == 2);
	GvrsSkipBytes(fp, 5);

	GvrsReadDouble(fp, &gvrs->x0);
	GvrsReadDouble(fp, &gvrs->y0);
	GvrsReadDouble(fp, &gvrs->x1);
	GvrsReadDouble(fp, &gvrs->y1);
	GvrsReadDouble(fp, &gvrs->cellSizeX);
	GvrsReadDouble(fp, &gvrs->cellSizeY);
	// The "x-center" parameters are intended to support geographic coordinate
	// transformations, but may be applied to other purposes as needed.
	gvrs->xCenterGrid = (gvrs->nColsInRaster - 1) / 2.0;
	gvrs->xCenter = gvrs->x0 + gvrs->xCenterGrid*gvrs->cellSizeX;  

	if (gvrs->geographicCoordinates) {
		// Because longitude is cyclic in nature, we need special logic to resolve coordinates.
		// If the grid includes a redundant column such that the longitude of the left-most
		// column is the same as, or 360 degrees more than, the longitude of the right-most column,
		// we say that the geographic coordinate system "brackets" the range of longitude.
		// If the left-most column is also one cell to the right of the right-most column
		// we say that the geographic coordinate system "wraps" the range of longitude.
		double gxDelta = gvrs->cellSizeX * (gvrs->nColsInRaster-1);
		if (fabs(gxDelta-360)<1.0e-9) {
			gvrs->geoWrapsLongitude = 0;
			gvrs->geoBracketsLongitude = 1;
		}
		else {
			// see if one grid cell beyond x1 matches x0  (within numerical precision).
			gxDelta = gvrs->cellSizeX * gvrs->nColsInRaster;
			if (fabs(gxDelta - 360) < 1.0e-9) {
				gvrs->geoWrapsLongitude = 1;
				gvrs->geoBracketsLongitude = 0;
			}
		}
	}


	readAffineTransform(fp, &gvrs->m2r);
	readAffineTransform(fp, &gvrs->r2m);

	istat = GvrsReadInt(fp, &gvrs->nElementsInTupple);
	if (istat != 0) {
		return fail(gvrs, fp, GVRSERR_EOF);
	}

	gvrs->elements = calloc((size_t)(gvrs->nElementsInTupple+1), sizeof(GvrsElement*));
	if (!gvrs->elements) {
		return fail(gvrs, fp, GVRSERR_NOMEM);
	}

	// The format for data within the file is a set of the following
	// for each element:
	//     0.   GvrsInt indicating the number of bytes used to store data
	//     1.   The bytes for the data
	// If the data is uncompressed, then the bytes for the data will be
	// just the type-size times the number of cells (padded to a multiple of 4)
	// If the data is compressed, the bytes will be smaller, but will eventually
	// be decompressed to their required size.  
	gvrs->nBytesForTileData = 0;
	int nCellsInTile = gvrs->nRowsInTile * gvrs->nColsInTile;
	for (iElement = 0; iElement < gvrs->nElementsInTupple; iElement++) {
		GvrsElement *element =   readElement(gvrs, iElement, nCellsInTile, gvrs->nBytesForTileData);
		if (!element) {
			return fail(gvrs, fp, GVRSERR_FILE_ACCESS);
		}
		gvrs->elements[iElement] = element;
		gvrs->nBytesForTileData += element->dataSize;
	}

	GvrsReadInt(fp, &gvrs->nDataCompressionCodecs);
	if (gvrs->nDataCompressionCodecs > 0) {
		int iCompress;
		gvrs->dataCompressionCodecs = calloc(gvrs->nDataCompressionCodecs, sizeof(GvrsCodec*));
		if (!gvrs->dataCompressionCodecs) {
			return fail(gvrs, fp, GVRSERR_NOMEM);
		}

#ifdef GVRS_ZLIB
		for (iCompress = 0; iCompress < gvrs->nDataCompressionCodecs; iCompress++) {
			char* sp = "";
			sp = GvrsReadString(fp, &status);
			if (status) {
				return fail(gvrs, fp, status);
			}
			if (strcmp("GvrsHuffman", sp) == 0) {
				gvrs->dataCompressionCodecs[iCompress] = GvrsCodecHuffmanAlloc();
			}
			else if (strcmp("GvrsDeflate", sp) == 0) {
				gvrs->dataCompressionCodecs[iCompress] = GvrsCodecDeflateAlloc();
			}
			else if (strcmp("GvrsFloat", sp) == 0) {
				gvrs->dataCompressionCodecs[iCompress] = GvrsCodecFloatAlloc();
	}
			else if (strcmp("LSOP12", sp)==0) {
				gvrs->dataCompressionCodecs[iCompress] = GvrsCodecLsopAlloc();
			}
			else {
				gvrs->dataCompressionCodecs[iCompress] = createCodecPlaceholder(sp);
			}
			if (!gvrs->dataCompressionCodecs[iCompress]) {
				return fail(gvrs, fp, GVRSERR_BAD_COMPRESSION_FORMAT);
			}
			free(sp);
		}
#else
		for (iCompress = 0; iCompress < gvrs->nDataCompressionCodecs; iCompress++) {
			unsigned char* sp = "";
			sp = GvrsReadString(fp, &status);
			if (status) {
				return fail(gvrs, fp, status);
			}
	        if (strcmp("GvrsHuffman", sp) == 0) {
				gvrs->dataCompressionCodecs[iCompress] = GvrsCodecHuffmanAlloc();
			}
			else {
				gvrs->dataCompressionCodecs[iCompress] = createCodecPlaceholder(sp);
			}
			if (!gvrs->dataCompressionCodecs[iCompress]) {
				return fail(gvrs, fp, GVRSERR_BAD_COMPRESSION_FORMAT);
			}
			free(sp);
		}
#endif

	}

	gvrs->productLabel = GvrsReadString(fp, &status);

	gvrs->tileDirectory = GvrsTileDirectoryRead(gvrs, gvrs->filePosTileDirectory, &status);
	if (status) {
		return fail(gvrs, fp, status);
	}

	gvrs->metadataDirectory =  GvrsMetadataDirectoryRead(fp, gvrs->filePosMetadataDirectory, &status);
	if (status) {
		return fail(gvrs, fp, status);
	}

	status = GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeMedium);
	if (status) {
		fail(gvrs, fp, GvrsError);
	}

	for (iElement = 0; iElement < gvrs->nElementsInTupple; iElement++) {
		gvrs->elements[iElement]->tileCache = gvrs->tileCache;
	}
	
	if (openedForWriting) {
		gvrs->timeOpenedForWritingMS = GvrsTimeMS();
		GvrsSetFilePosition(fp, FILEPOS_MODIFICATION_TIME+8);
		GvrsWriteLong(fp, gvrs->timeOpenedForWritingMS);  // the modification time
		gvrs->fileSpaceManager = GvrsFileSpaceManagerAlloc();
	}
	return gvrs;


}



GvrsElement* GvrsGetElementByName(Gvrs* gvrs, const char *name) {
	int i;
	if (!gvrs || !name || !gvrs->elements) {
		GvrsError = GVRSERR_NULL_POINTER;
		return 0;
	}

	for (i = 0; i < gvrs->nElementsInTupple; i++) {
		GvrsElement* element = gvrs->elements[i];
		if (strcmp(name, element->name) == 0) {
			return element;
		}
	}
	GvrsError = GVRSERR_ELEMENT_NOT_FOUND;

	return 0;
}

GvrsElement* GvrsGetElementByIndex(Gvrs* gvrs, int index) {
	if (!gvrs || !gvrs->elements) {
		GvrsError = GVRSERR_NULL_POINTER;
		return 0;
	}
	if (index<0 || index>=gvrs->nElementsInTupple) {
		return 0;
	}
	return gvrs->elements[index];
}

GvrsElement** GvrsGetElements(Gvrs* gvrs, int* nElements) {
	*nElements = gvrs->nElementsInTupple;
	return gvrs->elements;
}


void GvrsMapGridToModel(Gvrs* gvrs, double row, double column, double* x, double* y) {
	GvrsAffineTransform r2m = gvrs->r2m;
	*x = r2m.a00 * column + r2m.a01 * row + r2m.a02;
	*y = r2m.a10 * column + r2m.a11 * row + r2m.a12;
}


void GvrsMapModelToGrid(Gvrs* gvrs, double x, double y, double *row, double *column) {
	GvrsAffineTransform m2r = gvrs->m2r;
	*column = m2r.a00 * x + m2r.a01 * y + m2r.a02;
	*row    = m2r.a10 * x + m2r.a11 * y + m2r.a12;
}


void GvrsMapGeoToGrid(Gvrs* gvrs, double latitude, double longitude, double* row, double* column) {
	*row = (latitude - gvrs->y0) / gvrs->cellSizeY;
	*column = to180(longitude - gvrs->xCenter) / gvrs->cellSizeX + gvrs->xCenterGrid;
}

void GvrsMapGridToGeo(Gvrs* gvrs, double row, double column, double* latitude, double* longitude) {
	*latitude = row * gvrs->cellSizeY + gvrs->y0;
	*longitude = to180(column * gvrs->cellSizeX + gvrs->x0);
}


int
GvrsRegisterCodec(Gvrs* gvrs, GvrsCodec* codec) {
	if (!gvrs || !codec) {
		return GVRSERR_NOMEM;
	}
	int i;
	int n = gvrs->nDataCompressionCodecs;
	int n1 = n + 1;
	
	for (i = 0; i < n; i++) {
		GvrsCodec* c = gvrs->dataCompressionCodecs[i];
		if (strcmp(c->identification, codec->identification) == 0) {
			if (c->destroyCodec) {
				c->destroyCodec(c);
			}
			gvrs->dataCompressionCodecs[i] = codec;
			return 0;
		}
	}

	if (!gvrs->dataCompressionCodecs) {
		gvrs->dataCompressionCodecs = (GvrsCodec**)malloc(sizeof(GvrsCodec*));
		if (!gvrs->dataCompressionCodecs) {
			return GVRSERR_NOMEM;
		}
		gvrs->dataCompressionCodecs[0] = codec;
		gvrs->nDataCompressionCodecs = 1;
	}
 
	GvrsCodec** cpa = (GvrsCodec**)realloc(gvrs->dataCompressionCodecs, n1 * sizeof(GvrsCodec*));
	if (!cpa) {
		// the realloc failed.  our program is in a very bad place.  recovery is unlikely.
		// disable further compression-related actions and return an error code.
		free(gvrs->dataCompressionCodecs);
		gvrs->nDataCompressionCodecs = 0;
		return GVRSERR_NOMEM;
	}
	cpa[n] = codec;
	gvrs->dataCompressionCodecs = cpa;
	gvrs->nDataCompressionCodecs = n1;
 
	return 0;
}


static int writeChecksumForHeader(Gvrs* gvrs) {
	if (!gvrs->checksumEnabled) {
		// nothing to do
		return 0;
	}
	int status;
	FILE* fp = gvrs->fp;
	fflush(fp);
	GvrsInt sizeOfHeaderInBytes;
	GvrsSetFilePosition(fp, FILEPOS_OFFSET_TO_HEADER_RECORD);
	status = GvrsReadInt(fp, &sizeOfHeaderInBytes);
	if (status) {
		GvrsError = status;
		return status;
	}
	GvrsByte* b = malloc(sizeOfHeaderInBytes);
	if (!b) {
		GvrsError = GVRSERR_NOMEM;
		return GVRSERR_NOMEM;
	}

	status = GvrsSetFilePosition(fp, FILEPOS_OFFSET_TO_HEADER_RECORD);
	status = GvrsReadByteArray(fp, sizeOfHeaderInBytes-4, b);
	if (status == 0) {
		unsigned long crc = 0;
		crc = GvrsChecksumUpdateArray(b, 0, sizeOfHeaderInBytes - 4, crc);
		// The windows API requires us to set file position between reading and writing.
		// even though the file position should already be a the correct location.
		// It was probably written by an unpaid intern.
		GvrsSetFilePosition(fp, (GvrsLong)(FILEPOS_OFFSET_TO_HEADER_RECORD + sizeOfHeaderInBytes - 4));
		status = GvrsWriteInt(fp, (GvrsInt)(crc & 0xFFFFFFFFL));
	}
	free(b);
	if (status) {
		GvrsError = status;
	}
	return status;


}
static int writeClosingElements(Gvrs* gvrs, FILE* fp) {
	int status = GvrsTileCacheWritePendingTiles(gvrs->tileCache);
	if (status) {
		GvrsError = status;
		return status;
	}
	if (gvrs->tileDirectory) {
		GvrsLong tileDirectoryPos = GvrsTileDirectoryWrite(gvrs, &status);
		if (status) {
			GvrsError = status;
			return status;
		}
		GvrsSetFilePosition(fp, FILEPOS_OFFSET_TO_TILE_DIR);
		status = GvrsWriteLong(fp, tileDirectoryPos);
		if (status) {
			GvrsError = status;
			return status;
		}
	}

	GvrsSetFilePosition(fp, FILEPOS_MODIFICATION_TIME);
	GvrsWriteLong(fp, GvrsTimeMS());
	status = GvrsWriteLong(fp, 0);
	if (status) {
		GvrsError = status;
		return status;
	}
 	status = writeChecksumForHeader(gvrs);
	if (status) {
		GvrsError = status;
		return status;
	}
	return 0;
}

Gvrs* GvrsClose(Gvrs* gvrs) {
	if(gvrs){
		int status, status1;
		if (gvrs->fp) {
			if (gvrs->timeOpenedForWritingMS) {
				status = writeClosingElements(gvrs, gvrs->fp);
			}
			status = fflush(gvrs->fp);
			status1 = fclose(gvrs->fp);
			if (status || status1) {
				GvrsError = GVRSERR_FILE_ERROR;
			}
			gvrs->fp = 0;
		}

		// Free resources ------------------------
		int i;
		gvrs->path = freeString(gvrs->path);
	
		gvrs->productLabel = freeString(gvrs->productLabel);
		for (i = 0; i < gvrs->nElementsInTupple; i++) {
			gvrs->elements[i] = freeElement(gvrs->elements[i]);
		}
		free(gvrs->elements);
		gvrs->elements = 0;
		gvrs->tileCache = GvrsTileCacheFree(gvrs->tileCache);
		gvrs->tileDirectory = GvrsTileDirectoryFree(gvrs->tileDirectory);
		gvrs->metadataDirectory = GvrsMetadataDirectoryFree(gvrs->metadataDirectory);

		if (gvrs->dataCompressionCodecs) {
			for (i = 0; i < gvrs->nDataCompressionCodecs; i++) {
				GvrsCodec* codec = gvrs->dataCompressionCodecs[i];
				if (codec) {
					gvrs->dataCompressionCodecs[i] = codec->destroyCodec(codec);
				}
			}
			free(gvrs->dataCompressionCodecs);
			gvrs->dataCompressionCodecs = 0;
		}

		gvrs->fileSpaceManager = GvrsFileSpaceManagerFree(gvrs->fileSpaceManager);

		// we zero out the content of the GVRS structure as a diagnostic to help detect
		// cases where any external code attempts to access it after the close.  
		memset(gvrs, 0, sizeof(Gvrs));
		free(gvrs);
	}
	return 0;
}
