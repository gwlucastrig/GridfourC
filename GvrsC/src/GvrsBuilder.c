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


#include "GvrsBuilder.h"
#include "GvrsError.h"

#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
 
 


// This method references an error condition as part of the builder.
// For compactness, it may also be used in a return statement
static int recordStatus(GvrsBuilder* builder, int errorCode) {
	if (errorCode) {
		if (builder) {
			builder->errorCode = errorCode;
		}
	}
	return errorCode;
}


static int gvrsFail(GvrsBuilder* builder, Gvrs* gvrs, int errorCode) {
	recordStatus(builder, errorCode);
	GvrsDisposeOfResources(gvrs);
	return errorCode;
}

static char* optstrdup(const char* s, int* status) {
	if (s && *s) {
		char* t = GVRS_STRDUP(s);
		if (!t) {
			*status = GVRSERR_NOMEM;
			return 0;
		}
		return t;
	}
	return 0;
}




static int padMultipleOf4(FILE* fp) {
	long pos = ftell(fp);
	int k = (int)(pos & 0x3L);
	if (k > 0) {
		int i, status;
		for (i = k; i < 4; i++) {
			status = GvrsWriteByte(fp, 0);
		}
		if (status) {
			return status;
		}
	}
	return 0;
}

static int checkNumberOfTiles(GvrsBuilder* builder) {
	GvrsLong nRowsOfTiles = ((GvrsLong)builder->nRowsInRaster + (GvrsLong)builder->nRowsInTile - 1) / (GvrsLong)builder->nRowsInTile;
	GvrsLong nColsOfTiles = ((GvrsLong)builder->nColsInRaster + (GvrsLong)builder->nColsInTile - 1) / (GvrsLong)builder->nColsInTile;
	GvrsLong n = nRowsOfTiles * nColsOfTiles;
	if (n > INT32_MAX) {
		return recordStatus(builder, GVRSERR_BAD_RASTER_SPECIFICATION);
	}
	builder->nRowsOfTiles = (GvrsInt)nRowsOfTiles;
	builder->nColsOfTiles = (GvrsInt)nColsOfTiles;
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


static int checkArguments(GvrsBuilder* builder, const char* name, GvrsElementSpec** specReference) {
	if (!builder || !name || !*name || !specReference) {
		if (builder) {
			recordStatus(builder, GVRSERR_NULL_ARGUMENT);
		}
		return GVRSERR_NULL_ARGUMENT;
	}
	return 0;
}

static int addElementSpec(GvrsBuilder* builder, GvrsElementType eType, const char* name, GvrsElementSpec** specReference) {
	*specReference = 0;
	int status = checkIdentifier(name, GVRS_ELEMENT_NAME_SZ);
	if (status) {
		recordStatus(builder, status);
		return status;
	}

	// If other elements have already been added, check to verify that
	// the new element has a unique name.
	if (builder->elementSpecs) {
		int i;
		for (i = 0; i < builder->nElementSpecs; i++) {
			if (strcmp(builder->elementSpecs[i]->name, name) == 0) {
				recordStatus(builder, GVRSERR_NAME_NOT_UNIQUE);
				return GVRSERR_NAME_NOT_UNIQUE;
			}
		}
	}



	GvrsElementSpec** specs;
	if (builder->nElementSpecs == 0) {
		specs = calloc(1, sizeof(GvrsElementSpec*));
	}
	else {
		int n = builder->nElementSpecs + 1;
		specs = (GvrsElementSpec**)realloc(builder->elementSpecs, n * sizeof(GvrsElementSpec*));
		if (specs) {
			specs[builder->nElementSpecs] = 0;
		}
	}
	if (!specs) {
		if (builder->elementSpecs) {
			free(builder->elementSpecs);
		}
		recordStatus(builder, GVRSERR_NOMEM);
		return GVRSERR_NOMEM;
	}
	
	builder->elementSpecs = specs;
	GvrsElementSpec* eSpec = calloc(1, sizeof(GvrsElementSpec));
	if (!eSpec) {
		recordStatus(builder, GVRSERR_NOMEM);
		return GVRSERR_NOMEM;
	}
	eSpec->builder = builder;
	eSpec->description = 0;
	eSpec->unitOfMeasure = 0;
	eSpec->label = 0;
	eSpec->unitsToMeters = 1;
	GvrsStrncpy(eSpec->name, sizeof(eSpec->name), name);
	eSpec->elementType = eType;
	specs[builder->nElementSpecs++] = eSpec;
	*specReference = eSpec;
	return 0;
}

static void  computeAndStoreInternalTransforms(GvrsBuilder* b) {

	// It would be easy enough to compute the r2m matrix directly, but
	// testing showed that when we used the Java createInverse() method
	// and multiplied the two matrices togther, the values from createInverse
	// actually produced a result closer to the identify matrix.

	GvrsAffineTransform m2r;  // model to raster
	m2r.a00 = 1 / b->cellSizeX;
	m2r.a01 = 0;
	m2r.a02 = -b->x0 * m2r.a00;
	m2r.a10 = 0;
	m2r.a11 = 1 / b->cellSizeY;
	m2r.a12 = -b->y0 * m2r.a11;
	 
	GvrsAffineTransform r2m; // raster to model
	r2m.a00 = b->cellSizeX;
	r2m.a01 = 0;
	r2m.a02 = b->x0;
	r2m.a10 = 0;
	r2m.a11 = b->cellSizeY;
	r2m.a12 = b->y0;
 
	b->m2r = m2r;
	b->r2m = r2m;
}

int
GvrsBuilderInit(GvrsBuilder** builderReference, int nRows, int nColumns) {
	if (!builderReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*builderReference = 0;

	if (nRows < 1 || nColumns < 1) {
		return GVRSERR_BAD_RASTER_SPECIFICATION;
	}

	GvrsBuilder* builder = calloc(1, sizeof(GvrsBuilder));
	if(!builder){
		return GVRSERR_NOMEM;
	}
  
	int nRowsInRaster = nRows;
	int nColsInRaster = nColumns;
	int nRowsInTile;
	int nColsInTile;
	// Compute initial tile dimensions based
	  // on its own rules.   At this time, we use simple logic.
	  // In the future, we may try to develop something that ensures
	  // a better fit across grids of various sizes.
	  //   Note also that we use 120, rather than 128, because it has
	  // more factors and, thus, a higher probability of being an integral
	  // divisor of the overall grid size.
	if (nRowsInRaster < 120) {
		nRowsInTile = nRowsInRaster;
	}
	else {
		nRowsInTile = 120;
	}
	if (nColsInRaster < 120) {
		nColsInTile = nColsInRaster;
	}
	else {
		nColsInTile = 120;
	}
 
	builder->nRowsInRaster = nRows;
	builder->nColsInRaster = nColumns;
	builder->nRowsInTile = nRowsInTile;
	builder->nColsInTile = nColsInTile;
	builder->nCellsInTile = nRowsInTile * nColsInTile;
	int status1 = checkNumberOfTiles(builder);
	if (status1) {
		GvrsBuilderFree(builder);
		return status1;
	}

	builder->x0 = 0;
	builder->y0 = 0;
	builder->x1 = nColsInRaster - 1;
	builder->y1 = nRowsInRaster - 1;
	builder->cellSizeX = 1.0;
	builder->cellSizeY = 1.0;

	computeAndStoreInternalTransforms(builder);

	*builderReference = builder;
	return 0;
}

int
GvrsBuilderSetTileSize(GvrsBuilder* builder, int nRowsInTile, int nColumnsInTile) {
	if (!builder || nRowsInTile < 1 || nColumnsInTile < 1) {
		return recordStatus(builder, GVRSERR_BAD_RASTER_SPECIFICATION);
	}

	builder->nRowsInTile = nRowsInTile;
	builder->nColsInTile = nColumnsInTile;
	builder->nCellsInTile = nRowsInTile * nColumnsInTile;
	int status = checkNumberOfTiles(builder);
	if (status) {
		return recordStatus(builder, status);
	}
	return 0;
}


int GvrsBuilderSetCartesianCoordinates(GvrsBuilder* builder, double x0, double y0, double x1, double y1) {
	double dx = x1 - x0;
	double dy = y1 - y0;
	if (!isfinite(dx) || !isfinite(dy)) {
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}
	builder->geographicCoordinates = 0;
	builder->geoBracketsLongitude = 0;
	builder->geoWrapsLongitude = 0;
	builder->x0 = x0;
	builder->y0 = y0;
	builder->x1 = x1;
	builder->y1 = y1;
	builder->cellSizeX = dx / (builder->nColsInRaster - 1);
	builder->cellSizeY = dy / (builder->nRowsInRaster - 1);
	computeAndStoreInternalTransforms(builder);
	return 0;
}


static double to360(double angle) {
	if (fabs(angle) < 1.0e-9) {
		return 0;
	}

	double a;
	if (angle < 0) {
		a = 360 - fmod(-angle, 360.0);
	}
	else {
		a = fmod(fabs(angle), 360.0);
	}
 
	return a;
}


int GvrsBuilderSetGeographicCoordinates(GvrsBuilder* builder, double lat0, double lon0, double lat1, double lon1) {
	double x0 = lon0;
	double y0 = lat0;
	double x1 = lon1;
	double y1 = lat1;

	double dx = x1 - x0;
	double dy = y1 - y0;
	if (!isfinite(dx) || !isfinite(dy)) {
		return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
	}

	dx = to360(dx);
	if (fabs(dx) <1.0e-9 ) {
		dx = 360;
	}

	
	builder->geographicCoordinates = 2;
	builder->geoBracketsLongitude = 0;
	builder->geoWrapsLongitude = 0;
	builder->x0 = x0;
	builder->y0 = y0;
	builder->x1 = x1;
	builder->y1 = y1;
	builder->cellSizeX = dx / (builder->nColsInRaster - 1);
	builder->cellSizeY = dy / (builder->nRowsInRaster - 1);
	computeAndStoreInternalTransforms(builder);

	// Because longitude is cyclic in nature, we need special logic to resolve coordinates.
	// If the grid includes a redundant column such that the longitude of the left-most
	// column is the same as, or 360 degrees more than, the longitude of the right-most column,
	// we say that the geographic coordinate system "brackets" the range of longitude.
	// If the left-most column is also one cell to the right of the right-most column
	// we say that the geographic coordinate system "wraps" the range of longitude.
	double gxDelta = builder->cellSizeX * (builder->nColsInRaster - 1);
	if (fabs(gxDelta - 360) < 1.0e-9) {
		builder->geoWrapsLongitude = 0;
		builder->geoBracketsLongitude = 1;
	}
	else {
		// see if one grid cell beyond x1 matches x0  (within numerical precision).
		gxDelta = builder->cellSizeX * builder->nColsInRaster;
		if (fabs(gxDelta - 360) < 1.0e-9) {
			builder->geoWrapsLongitude = 1;
			builder->geoBracketsLongitude = 0;
		}
	}

	return 0;
}

void GvrsBuilderSetChecksumEnabled(GvrsBuilder* builder, int checksumEnabled)
{
	if (checksumEnabled) {
		builder->checksumEnabled = 1;
	}
	else {
		builder->checksumEnabled = 0;
	}
}

static void freeCodecs(GvrsBuilder* builder) {
	if (builder->nDataCompressionCodecs) {
		for (int i = 0; i < builder->nDataCompressionCodecs; i++) {
			builder->dataCompressionCodecs[i]->destroyCodec(builder->dataCompressionCodecs[i]);
			builder->dataCompressionCodecs[i] = 0;
		}
		free(builder->dataCompressionCodecs);
	}
	builder->nDataCompressionCodecs = 0;
	builder->dataCompressionCodecs = 0;
}

GvrsBuilder* GvrsBuilderFree(GvrsBuilder* builder) {
	if (builder) {
		int i;
		if (builder->elementSpecs) {
			for (i = 0; i < builder->nElementSpecs; i++) {
				GvrsElementSpec* spec = builder->elementSpecs[i];
				if (spec) {
					// The name is a fixed length string and not malloc'd.
					// Other strings are malloc'd.
					if (spec->description) {
						free(spec->description);
						spec->description = 0;
					}
					if (spec->unitOfMeasure) {
						free(spec->unitOfMeasure);
						spec->unitOfMeasure = 0;
					}
					if (spec->label) {
						free(spec->label);
						spec->label = 0;
					}
					spec->builder = 0;
					free(spec);
					spec = 0;
					builder->elementSpecs[i] = 0;
				}
			}
			free(builder->elementSpecs);
			builder->elementSpecs = 0;
		}

		if (builder->nDataCompressionCodecs) {
			for (i = 0; i < builder->nDataCompressionCodecs; i++) {
				GvrsCodec* c = builder->dataCompressionCodecs[i];
				if (c) {
					c->destroyCodec(c);
				}
			}
			free(builder->dataCompressionCodecs);
		}
		memset(builder, 0, sizeof(GvrsBuilder));  // a debugging aid
		free(builder);
	}
	return 0;
}

int 
GvrsBuilderAddElementShort(GvrsBuilder* builder, const char* name, GvrsElementSpec** specReference) {
	int status;
	status = checkArguments(builder, name, specReference);
	if (status) {
		return status;
	}

	GvrsElementSpec* spec;
	status = addElementSpec(builder, GvrsElementTypeShort, name, &spec);
	if (status) {
		return status;
	}

 
	spec->elementSpec.shortSpec.minValue = INT16_MIN+1;
	spec->elementSpec.shortSpec.maxValue = INT16_MAX;
	spec->elementSpec.shortSpec.fillValue = INT16_MIN;
	spec->typeSize = 2;
	*specReference = spec;
	return 0;
}
 
int 
GvrsBuilderAddElementInt(GvrsBuilder* builder, const char* name, GvrsElementSpec **specReference) { 
	int status;
	status = checkArguments(builder, name, specReference);
	if (status) {
		return status;
	}

	GvrsElementSpec* spec;
	status = addElementSpec(builder, GvrsElementTypeInt, name, &spec);
	if (status) {
		return status;
	}


	spec->elementSpec.intSpec.minValue = INT32_MIN + 1;
	spec->elementSpec.intSpec.maxValue = INT32_MAX;
	spec->elementSpec.intSpec.fillValue = INT32_MIN;
	spec->typeSize = 4;
	*specReference = spec;
	return 0;
}

int GvrsBuilderAddElementFloat(GvrsBuilder* builder, const char* name, GvrsElementSpec** specReference)
{
	int status;
	status = checkArguments(builder, name, specReference);
	if (status) {
		return status;
	}
	GvrsElementSpec* spec;
	status = addElementSpec(builder, GvrsElementTypeFloat, name, &spec);
	if (status) {
		return status;
	}

	spec->elementSpec.floatSpec.minValue = -1.0e+32F;
	spec->elementSpec.floatSpec.maxValue = 1.0e+32F;
	spec->elementSpec.floatSpec.fillValue = NAN;
	spec->continuous = 1;
	spec->typeSize = 4;
	*specReference = spec;
	return 0;
}


int
GvrsBuilderAddElementIntCodedFloat(GvrsBuilder* builder, 
	const char* name, 
	GvrsFloat scale,
	GvrsFloat offset,
	GvrsElementSpec** specReference)
{
	int status;
	status = checkArguments(builder, name, specReference);
	if (status) {
		return status;
	}

	if (scale == 0 || isnan(scale) || isnan(offset)){
		recordStatus(builder, GVRSERR_BAD_ICF_PARAMETERS);
		return GVRSERR_BAD_ICF_PARAMETERS;
	}

	GvrsElementSpec* spec;
	status = addElementSpec(builder, GvrsElementTypeIntCodedFloat, name, &spec);
	if (status) {
		return status;
	}

	spec->elementSpec.intFloatSpec.scale = scale;
	spec->elementSpec.intFloatSpec.offset = offset;
	spec->elementSpec.intFloatSpec.iMinValue = INT32_MIN + 1;
	spec->elementSpec.intFloatSpec.iMaxValue = INT32_MAX;
	spec->elementSpec.intFloatSpec.iFillValue = INT32_MIN;
	spec->elementSpec.intFloatSpec.minValue = (INT32_MIN + 1) / scale + offset;;
	spec->elementSpec.intFloatSpec.maxValue = INT32_MAX / scale + offset;
	spec->elementSpec.intFloatSpec.fillValue = NAN;
	spec->continuous = 1;
	spec->typeSize = 4;
	*specReference = spec;

	return 0;
}

int
GvrsElementSpecSetRangeInt(GvrsElementSpec* eSpec, GvrsInt iMin, GvrsInt iMax) {
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if (iMin > iMax) {
		return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
	}

	switch (eSpec->elementType) {
	case GvrsElementTypeInt:
		eSpec->elementSpec.intSpec.minValue = iMin;
		eSpec->elementSpec.intSpec.maxValue = iMax;
		break;
	case GvrsElementTypeIntCodedFloat: {
		GvrsElementSpecIntCodedFloat icf = eSpec->elementSpec.intFloatSpec;
		eSpec->elementSpec.intFloatSpec.iMinValue = iMin;
		eSpec->elementSpec.intFloatSpec.iMaxValue = iMax;
		eSpec->elementSpec.intFloatSpec.minValue = iMin / icf.scale + icf.offset;
		eSpec->elementSpec.intFloatSpec.maxValue = iMax / icf.scale + icf.offset;
	}
		break;
	case GvrsElementTypeFloat:
		eSpec->elementSpec.floatSpec.minValue = (GvrsFloat)iMin;
		eSpec->elementSpec.floatSpec.maxValue = (GvrsFloat)iMax;
		break;
	case GvrsElementTypeShort:
		if (iMin<INT16_MIN || iMax>INT16_MAX) {
			return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
		}
		eSpec->elementSpec.shortSpec.minValue = (GvrsShort)iMin;
		eSpec->elementSpec.shortSpec.maxValue = (GvrsShort)iMax;
		break;
	default:
		return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
	}

	return 0;
}

int GvrsElementSpecSetContinuous(GvrsElementSpec* eSpec, int continuous)
{
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}
	eSpec->continuous = continuous;
	return 0;
}

int
GvrsElementSpecSetDescription(GvrsElementSpec* eSpec, char *description) {
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}

	if (!description || !*description) {
		eSpec->description = 0;
		return 0;
	}

	int status;
	eSpec->description = optstrdup(description, &status);
	return status;
}


int
GvrsElementSpecSetLabel(GvrsElementSpec* eSpec, char* label) {
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}

	if (!label || !*label) {
		eSpec->label = 0;
		return 0;
	}

	int status;
	eSpec->label = optstrdup(label, &status);
	return status;
}


int
GvrsElementSpecSetUnitOfMeasure(GvrsElementSpec* eSpec, char* unitOfMeasure) {
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}

	if (!unitOfMeasure || !*unitOfMeasure) {
		eSpec->unitOfMeasure = 0;
		return 0;
	}

	int status;
	eSpec->unitOfMeasure = optstrdup(unitOfMeasure, &status);
	return status;
}


static int writeHeader(Gvrs *gvrs);

int
GvrsBuilderOpenNewGvrs(GvrsBuilder* builder, const char* path, Gvrs** gvrsReference) {
	int i;
	if (!builder || !path || !path[0] || !gvrsReference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*gvrsReference = 0;

	if (builder->errorCode) {
		// there was an error recorded while building the specification.
		// we cannot build a file
		return builder->errorCode;
	}

	int status = 0;

	// step 1:  test for internal completeness of the specification -------------
	if (builder->nElementSpecs == 0) {
		return recordStatus(builder, GVRSERR_BAD_ELEMENT_SPEC);
	}

	// step 2: establish access to a file ---------------------------------------
	struct stat statbuffer;
	int status1;

	status1 = stat(path, &statbuffer);
	if (!status1) {
		// the file exists
		status1 = remove(path);
		if (status1) {
			return recordStatus(builder, GVRSERR_FILE_ACCESS);
		}
	}

	errno = 0;
	FILE* fp = fopen(path, "wb+");
	if (!fp) {
		if (errno == EACCES) {
			return recordStatus(builder, GVRSERR_FILE_ACCESS);
		}
		else if (errno == ENOENT) {
			return recordStatus(builder, GVRSERR_FILENOTFOUND);
		}
		return recordStatus(builder, GVRSERR_FILE_ACCESS);
	}
 

	// step 3: populate a GVRS object --- ---------------------------------------
	Gvrs* gvrs = calloc(1, sizeof(Gvrs));
	if (!gvrs) {
		fclose(fp);
		return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
	}

	
	gvrs->fp = fp;
	gvrs->path = GVRS_STRDUP(path);
	if (!path) {
		return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
	}

	gvrs->checksumEnabled = builder->checksumEnabled;

	gvrs->rasterSpaceCode = builder->rasterSpaceCode;
	gvrs->geographicCoordinates = builder->geographicCoordinates;
	gvrs->geoWrapsLongitude = builder->geoWrapsLongitude;
	gvrs->geoBracketsLongitude = builder->geoBracketsLongitude;
	gvrs->nRowsInRaster = builder->nRowsInRaster;
	gvrs->nColsInRaster = builder->nColsInRaster;
	gvrs->nRowsInTile = builder->nRowsInTile;
	gvrs->nColsInTile = builder->nColsInTile;
	gvrs->nRowsOfTiles = builder->nRowsOfTiles;
	gvrs->nColsOfTiles = builder->nColsOfTiles;
	gvrs->nCellsInTile = builder->nCellsInTile;
	
	gvrs->x0 = builder->x0;
	gvrs->y0 = builder->y0;
	gvrs->x1 = builder->x1;
	gvrs->y1 = builder->y1;
	gvrs->cellSizeX = builder->cellSizeX;
	gvrs->cellSizeY = builder->cellSizeY;
	// The "x-center" parameters are intended to support geographic coordinate
		// transformations, but may be applied to other purposes as needed.
	gvrs->xCenterGrid = (gvrs->nColsInRaster - 1) / 2.0;
	gvrs->xCenter = gvrs->x0 + gvrs->xCenterGrid * gvrs->cellSizeX;

	if (gvrs->geographicCoordinates) {
		// NOTE: This same code is in the GvrsOpen module....
		// Because longitude is cyclic in nature, we need special logic to resolve coordinates.
		// If the grid includes a redundant column such that the longitude of the left-most
		// column is the same as, or 360 degrees more than, the longitude of the right-most column,
		// we say that the geographic coordinate system "brackets" the range of longitude.
		// If the left-most column is also one cell to the right of the right-most column
		// we say that the geographic coordinate system "wraps" the range of longitude.
		double gxDelta = gvrs->cellSizeX * (gvrs->nColsInRaster - 1);
		if (fabs(gxDelta - 360) < 1.0e-9) {
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

 
	gvrs->m2r = builder->m2r;
	gvrs->r2m = builder->r2m;
 

	gvrs->nElementsInTupple = builder->nElementSpecs;
 
	gvrs->elements = calloc((size_t)(gvrs->nElementsInTupple + 1), sizeof(GvrsElement*));
	if (!gvrs->elements) {
		return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
	}
	int offsetWithinTileData = 0;
	for (i = 0; i < builder->nElementSpecs; i++) {
		GvrsElementSpec* eSpec = builder->elementSpecs[i];
		GvrsElement* e = calloc(1, sizeof(GvrsElement));
		if (!e) {
			return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
		}
		gvrs->elements[i] = e;
		e->gvrs = gvrs;
		GvrsStrncpy(e->name, sizeof(e->name), eSpec->name);
		e->elementType = eSpec->elementType;
		e->elementIndex = i;
		e->dataOffset = offsetWithinTileData;
		int n = eSpec->typeSize * builder->nCellsInTile;
		e->dataSize = (n + 2) & 0xfffffffc; // round up to nearest multiple of 4 (sometimes needed for short) 
		offsetWithinTileData += e->dataSize;

		memcpy(&e->elementSpec, &eSpec->elementSpec, sizeof(eSpec->elementSpec));
		e->label = optstrdup(eSpec->label, &builder->errorCode);
		e->description = optstrdup(eSpec->description, &builder->errorCode);
		e->unitOfMeasure = optstrdup(eSpec->unitOfMeasure, &builder->errorCode);
		e->unitsToMeters = eSpec->unitsToMeters;
		switch (eSpec->elementType) {
		case GvrsElementTypeInt:
			e->fillValueInt = eSpec->elementSpec.intSpec.fillValue;
			e->fillValueFloat = (float)e->fillValueInt;
			break;
		case GvrsElementTypeIntCodedFloat:
			e->fillValueInt = eSpec->elementSpec.intFloatSpec.iFillValue;
			e->fillValueFloat = eSpec->elementSpec.intFloatSpec.fillValue;
			break;
		case GvrsElementTypeFloat:
			e->fillValueFloat = eSpec->elementSpec.floatSpec.fillValue;
			e->fillValueInt = (int)e->fillValueFloat;
			break;
		case GvrsElementTypeShort:
			e->fillValueInt = eSpec->elementSpec.shortSpec.fillValue;
			e->fillValueFloat = (float)e->fillValueInt;
			break;
		default:
			break;
		}
	}
	gvrs->nBytesForTileData = offsetWithinTileData;

	gvrs->nDataCompressionCodecs = builder->nDataCompressionCodecs;
	if (gvrs->nDataCompressionCodecs >0) {
		gvrs->dataCompressionCodecs = calloc(gvrs->nDataCompressionCodecs, sizeof(GvrsCodec*));
		if (!gvrs->dataCompressionCodecs) {
			return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
		}
		for (i = 0; i < gvrs->nDataCompressionCodecs; i++) {
			GvrsCodec* codec = builder->dataCompressionCodecs[i];
			if (codec) {
				gvrs->dataCompressionCodecs[i] = codec->allocateNewCodec(codec);
				if (!gvrs->dataCompressionCodecs[i]) {
					return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
				}
			}
		}
	}

	status = writeHeader(gvrs);
	if (status == 0) {
		GvrsTileDirectory* tileDirectoryReference;
		status = GvrsTileDirectoryAllocEmpty(gvrs->nRowsOfTiles, gvrs->nColsOfTiles, &tileDirectoryReference);
		if (status) {
			return  gvrsFail(builder, gvrs, status);
		}
		gvrs->tileDirectory = tileDirectoryReference;
	}
	else {
		return gvrsFail(builder, gvrs, status);
	}

	GvrsMetadataDirectory* metadataDirectory = (GvrsMetadataDirectory*)gvrs->metadataDirectory;
	status = GvrsMetadataDirectoryAllocEmpty(gvrs, &metadataDirectory);
	if (status) {
		return gvrsFail(builder, gvrs, status);
	}
	gvrs->metadataDirectory = metadataDirectory;

	status = GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeMedium);
	if (status == 0) {
		gvrs->fileSpaceManager = GvrsFileSpaceManagerAlloc(fp);
		if (!gvrs->fileSpaceManager) {
			return gvrsFail(builder, gvrs, GVRSERR_NOMEM);
		}
	}
	else {
		return gvrsFail(builder, gvrs, status);
	}

	fflush(fp);
	*gvrsReference = gvrs;
	return 0;
}


static int writeSpec(Gvrs* gvrs) {
	int iElement;
	int status;
	FILE* fp = gvrs->fp;

	status = GvrsWriteInt(fp, gvrs->nRowsInRaster);
	status = GvrsWriteInt(fp, gvrs->nColsInRaster);
	status = GvrsWriteInt(fp, gvrs->nRowsInTile);
	status = GvrsWriteInt(fp, gvrs->nColsInTile);
	status = GvrsWriteInt(fp, 0);  // reserved
	status = GvrsWriteInt(fp, 0);
	if (status) {
		return status;
	}

	status = GvrsWriteBoolean(fp, gvrs->checksumEnabled);
	status = GvrsWriteByte(fp, gvrs->rasterSpaceCode);
	if (gvrs->geographicCoordinates) {
		status = GvrsWriteByte(fp, 2);
	}
	else {
		status = GvrsWriteByte(fp, 1);
	}
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	if (status) {
		return status;
	}

	// file spec line 1179
	status = GvrsWriteDouble(fp, gvrs->x0);
	status = GvrsWriteDouble(fp, gvrs->y0);
	status = GvrsWriteDouble(fp, gvrs->x1);
	status = GvrsWriteDouble(fp, gvrs->y1);
	status = GvrsWriteDouble(fp, gvrs->cellSizeX);
	status = GvrsWriteDouble(fp, gvrs->cellSizeY);

	status = GvrsWriteDouble(fp, gvrs->m2r.a00);
	status = GvrsWriteDouble(fp, gvrs->m2r.a01);
	status = GvrsWriteDouble(fp, gvrs->m2r.a02);
	status = GvrsWriteDouble(fp, gvrs->m2r.a10);
	status = GvrsWriteDouble(fp, gvrs->m2r.a11);
	status = GvrsWriteDouble(fp, gvrs->m2r.a12);

	status = GvrsWriteDouble(fp, gvrs->r2m.a00);
	status = GvrsWriteDouble(fp, gvrs->r2m.a01);
	status = GvrsWriteDouble(fp, gvrs->r2m.a02);
	status = GvrsWriteDouble(fp, gvrs->r2m.a10);
	status = GvrsWriteDouble(fp, gvrs->r2m.a11);
	status = GvrsWriteDouble(fp, gvrs->r2m.a12);

	if (status) {
		return status;
	}

	// spec line 1208
	status = GvrsWriteInt(fp, gvrs->nElementsInTupple);
	for (iElement = 0; iElement < gvrs->nElementsInTupple; iElement++) {
		GvrsElement* e = gvrs->elements[iElement];
		status = GvrsWriteByte(fp, e->elementType);
		status = GvrsWriteByte(fp, e->continuous);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteByte(fp, 0);
		status = GvrsWriteString(fp, e->name);
		padMultipleOf4(fp);
		switch (e->elementType) {
		case GvrsElementTypeShort: {
			GvrsElementSpecShort sSpec = e->elementSpec.shortSpec;
			status = GvrsWriteShort(fp, sSpec.minValue);
			status = GvrsWriteShort(fp, sSpec.maxValue);
			status = GvrsWriteShort(fp, sSpec.fillValue);
			break;
		}
		case GvrsElementTypeFloat: {
			GvrsElementSpecFloat fSpec = e->elementSpec.floatSpec;
			status = GvrsWriteFloat(fp, fSpec.minValue);
			status = GvrsWriteFloat(fp, fSpec.maxValue);
			status = GvrsWriteFloat(fp, fSpec.fillValue);
			break;
		}
		case GvrsElementTypeIntCodedFloat: {
			GvrsElementSpecIntCodedFloat icfSpec = e->elementSpec.intFloatSpec;
			status = GvrsWriteFloat(fp, icfSpec.minValue);
			status = GvrsWriteFloat(fp, icfSpec.maxValue);
			status = GvrsWriteFloat(fp, icfSpec.fillValue);
			status = GvrsWriteFloat(fp, icfSpec.scale);
			status = GvrsWriteFloat(fp, icfSpec.offset);
			status = GvrsWriteInt(fp, icfSpec.iMinValue);
			status = GvrsWriteInt(fp, icfSpec.iMaxValue);
			status = GvrsWriteInt(fp, icfSpec.iFillValue);
			break;
		}
		case GvrsElementTypeInt: {
			GvrsElementSpecInt iSpec = e->elementSpec.intSpec;
			status = GvrsWriteInt(fp, iSpec.minValue);
			status = GvrsWriteInt(fp, iSpec.maxValue);
			status = GvrsWriteInt(fp, iSpec.fillValue);
			break;
		}
		default:
			break;
		}

		status = GvrsWriteString(fp, e->label);
		status = GvrsWriteString(fp, e->description);
		status = GvrsWriteString(fp, e->unitOfMeasure);
		if (status) {
			return status;
		}
		status = padMultipleOf4(fp);
	}

	// write compression codecs
	int i;
	status = GvrsWriteInt(fp, gvrs->nDataCompressionCodecs); 
	for (i = 0; i < gvrs->nDataCompressionCodecs; i++) {
		GvrsWriteString(fp, gvrs->dataCompressionCodecs[i]->identification);
	}

	status = GvrsWriteString(fp, gvrs->productLabel);
	return 0;

}

static int writeHeader(Gvrs* gvrs) {
	
	int status;
	FILE* fp = gvrs->fp;

	status = GvrsWriteASCII(fp, 12, "gvrs raster");
	if (status) {
		return status;
	}

	status = GvrsWriteByte(fp, GVRS_VERSION);
	status = GvrsWriteByte(fp, GVRS_SUB_VERSION);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);

	status = GvrsWriteInt(fp, 0); // the size of the header, to be filled in later
	status = GvrsWriteByte(fp, (GvrsByte)GvrsRecordTypeHeader);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);

	status = GvrsWriteLong(fp, gvrs->uuidLow);
	status = GvrsWriteLong(fp, gvrs->uuidHigh);

	gvrs->timeOpenedForWritingMS = GvrsTimeMS();
	status = GvrsWriteLong(fp, gvrs->timeOpenedForWritingMS);
	status = GvrsWriteLong(fp, gvrs->timeOpenedForWritingMS);
	status = GvrsWriteLong(fp, 0);
	status = GvrsWriteLong(fp, 0);

	status = GvrsWriteShort(fp, 1);  // n levels
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);
	status = GvrsWriteByte(fp, 0);

	GvrsLong filePos = ftell(fp);
	status = GvrsWriteLong(fp, 0);  // pos 80, filePos to first (only) tile directory

	// write a block of reservd longs for future use
	status = GvrsWriteLong(fp, 0);
	status = GvrsWriteLong(fp, 0);

	status = writeSpec(gvrs);
	if (status) {
		return status;
	}

	GvrsWriteZeroes(fp, 8);  // reserved for future use

	// The filePos to the end of the header needs to be a multiple
	// of 8 in order to support file position compression. The size is
	// not known a priori because it will depend on the structure
	// of the elements in the specification.  At this point,
	// we will also need to reserve 4 extra bytes for the checksum
	// and then pad out the record.
	filePos = ftell(fp);
	GvrsLong filePosContent = (filePos + 4LL + 7LL) & 0xfffffff8LL;
	GvrsInt sizeOfHeaderInBytes = (int)(filePosContent - FILEPOS_OFFSET_TO_HEADER_RECORD);
	GvrsInt padding = (int)(filePosContent - filePos);
	GvrsWriteZeroes(fp, padding);
	fflush(fp);

	  status = GvrsSetFilePosition(fp, FILEPOS_OFFSET_TO_HEADER_RECORD);
	  status = GvrsWriteInt(fp, sizeOfHeaderInBytes);
	  if (status) {
		  return status;
	  }
	  status = GvrsSetFilePosition(fp, filePosContent);

	if (status) {
		return status;
	}

	return 0;
}

int
GvrsBuilderRegisterStandardDataCompressionCodecs(GvrsBuilder* builder) {
	freeCodecs(builder);

#ifdef GVRS_ZLIB
	builder->nDataCompressionCodecs = 4;
	builder->dataCompressionCodecs = calloc(4, sizeof(GvrsCodec*));
	if (builder->dataCompressionCodecs) {
		builder->dataCompressionCodecs[0] = GvrsCodecHuffmanAlloc();
		builder->dataCompressionCodecs[1] = GvrsCodecDeflateAlloc();
		builder->dataCompressionCodecs[2] = GvrsCodecFloatAlloc();
		builder->dataCompressionCodecs[3] = GvrsCodecLsopAlloc();
	}
	else {
		return GVRSERR_NOMEM;
	}
#else
	if (builder->dataCompressionCodecs) {
		builder->nDataCompressionCodecs = 1;
		builder->dataCompressionCodecs = calloc(4, sizeof(GvrsCodec*));
		builder->dataCompressionCodecs[0] = GvrsCodecHuffmanAlloc();
	}
	else {
		return GVRSERR_NOMEM;
	}
#endif

	int i;
	for (i = 0; i < builder->nDataCompressionCodecs; i++) {
		if (!builder->dataCompressionCodecs[i]) {
			return GVRSERR_NOMEM;
		}
	}
	return 0;
}


int GvrsBuilderRegisterDataCompressionCodec(GvrsBuilder* builder, GvrsCodec* codec)
{
	int i;
	for (i = 0; i < builder->nDataCompressionCodecs; i++) {
		GvrsCodec* c = builder->dataCompressionCodecs[i];
		if (strcmp(c->identification, codec->identification) == 0) {
			c->destroyCodec(c);
			builder->dataCompressionCodecs[i] = codec;
			return 0;
		}
	}

	if (builder->nDataCompressionCodecs == 0) {
		builder->dataCompressionCodecs = malloc(sizeof(GvrsCodec*));
		if (!builder->dataCompressionCodecs) {
			return GVRSERR_NOMEM;
		}
	}
	else {
		int n = builder->nDataCompressionCodecs + 1;
		GvrsCodec** p = realloc(builder->dataCompressionCodecs, n * sizeof(GvrsCodec*));
		if (!p) {
			return GVRSERR_NOMEM;
		}
		builder->dataCompressionCodecs = p;
	}
	builder->dataCompressionCodecs[builder->nDataCompressionCodecs++] = codec;
	return 0;
}


int 
GvrsElementSpecSetFillValueInt(GvrsElementSpec* spec, GvrsInt fillValue) {
	if (!spec || !spec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}
	GvrsBuilder* builder = spec->builder;

	switch (spec->elementType) {
	case GvrsElementTypeInt:
		spec->elementSpec.intSpec.fillValue = fillValue;
		return 0;
	case GvrsElementTypeIntCodedFloat: {
		float scale = spec->elementSpec.intFloatSpec.scale;
		float offset = spec->elementSpec.intFloatSpec.offset;
		spec->elementSpec.intFloatSpec.iFillValue = fillValue;
		if (fillValue == INT32_MIN) {
			spec->elementSpec.intFloatSpec.fillValue = NAN;
		}
		else {
			spec->elementSpec.intFloatSpec.fillValue = fillValue / scale + offset;
		}
	}
									 return 0;
	case GvrsElementTypeFloat:
		spec->elementSpec.floatSpec.fillValue = (GvrsFloat)fillValue;
		return 0;
	case GvrsElementTypeShort:
		if (fillValue<SHRT_MIN || fillValue>SHRT_MAX) {
			builder->errorCode = GVRSERR_BAD_ELEMENT_SPEC;
			return builder->errorCode;
		}
		spec->elementSpec.shortSpec.fillValue = (GvrsShort)fillValue;
		return 0;
	default:
		builder->errorCode = GVRSERR_BAD_ELEMENT_SPEC;
		return builder->errorCode;
	}
}



int
GvrsElementSpecSetRangeFloat(GvrsElementSpec* eSpec, float min, float max) {
	if (!eSpec || !eSpec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}
	if(isnan(min) || isnan(max) || min > max) {
		return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
	}

	switch (eSpec->elementType) {
	case GvrsElementTypeInt:
		if (min<INT32_MIN || max>INT32_MAX) {
			return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
		}
		eSpec->elementSpec.intSpec.minValue = (int)min;
		eSpec->elementSpec.intSpec.maxValue = (int)max;
		break;
	case GvrsElementTypeIntCodedFloat: {
		GvrsElementSpecIntCodedFloat icf = eSpec->elementSpec.intFloatSpec;
		float tMin = (min - icf.offset) * icf.scale;
		float tMax = (max - icf.offset) * icf.scale;
		if (tMin<INT32_MIN || tMax>INT32_MAX) {
			return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
		}
		int iMin = (int)tMin;
		int iMax = (int)tMax;
		eSpec->elementSpec.intFloatSpec.iMinValue = iMin;
		eSpec->elementSpec.intFloatSpec.iMaxValue = iMax;
		eSpec->elementSpec.intFloatSpec.minValue = iMin / icf.scale + icf.offset;
		eSpec->elementSpec.intFloatSpec.maxValue = iMax / icf.scale + icf.offset;
	}
	break;
	case GvrsElementTypeFloat:
		eSpec->elementSpec.floatSpec.minValue = min;
		eSpec->elementSpec.floatSpec.maxValue = max;
		break;
	case GvrsElementTypeShort:
		if (min<INT16_MIN || max>INT16_MAX) {
			return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
		}
		eSpec->elementSpec.shortSpec.minValue = (GvrsShort)min;
		eSpec->elementSpec.shortSpec.maxValue = (GvrsShort)max;
		break;
	default:
		return recordStatus(eSpec->builder, GVRSERR_BAD_ELEMENT_SPEC);
	}

	return 0;
}



int 
GvrsElementSpecSetFillValueFloat(GvrsElementSpec* spec, float fillValue) {
	if (!spec || !spec->builder) {
		return GVRSERR_NULL_ARGUMENT;
	}
	GvrsBuilder* builder = spec->builder;

	switch (spec->elementType) {
	case GvrsElementTypeInt:
		if (isnan(fillValue)) {
			spec->elementSpec.intSpec.fillValue = INT32_MIN;
		}
		else {
			if (fillValue<INT32_MIN || fillValue>INT32_MAX) {
				return recordStatus(spec->builder, GVRSERR_BAD_ELEMENT_SPEC);
			}
			spec->elementSpec.intSpec.fillValue = (int)fillValue;
		}
		return 0;
	case GvrsElementTypeIntCodedFloat: {
		float scale = spec->elementSpec.intFloatSpec.scale;
		float offset = spec->elementSpec.intFloatSpec.offset;
		spec->elementSpec.intFloatSpec.fillValue = fillValue;
		if (isnan(fillValue)) {
			spec->elementSpec.intFloatSpec.iFillValue = INT32_MIN;
		}
		else {
			spec->elementSpec.intFloatSpec.fillValue = (fillValue - offset) * scale; ;
		}
	}
	return 0;
	case GvrsElementTypeFloat:
		spec->elementSpec.floatSpec.fillValue = fillValue;
		return 0;
	case GvrsElementTypeShort:
		if (isnan(fillValue)) {
			spec->elementSpec.shortSpec.fillValue = SHRT_MIN;
		}
		else {
			if (fillValue<SHRT_MIN || fillValue>SHRT_MAX) {
				builder->errorCode = GVRSERR_BAD_ELEMENT_SPEC;
				return builder->errorCode;
			}
			spec->elementSpec.shortSpec.fillValue = (short)fillValue;
		}
		return 0;
	default:
		builder->errorCode = GVRSERR_BAD_ELEMENT_SPEC;
		return builder->errorCode;
	}
}