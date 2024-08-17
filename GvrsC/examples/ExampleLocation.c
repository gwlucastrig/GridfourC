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


#include "Gvrs.h"
#include "GvrsError.h"
#include "GvrsInterpolation.h"
 

struct Place {
    char* name;
    double lat;
    double lon;
};

static struct Place places[] = {
    {"Auckland, NZ",                 -36.84,     174.74},
    {"Coachella, California, US",     33.6811,  -116.1744},
    {"Danbury, Connecticut, US",      41.386,    -73.482},
    {"Dayton, Ohio, US",              39.784,    -84.110},
    {"Deming, New Mexico, US",        32.268,   -107.757},
    {"Denver, Colorado, US",          39.7392,  -104.985},
    {"La Ciudad de Mexico, MX",       19.4450,   -99.1335},
    {"La Paz, Bolivia",              -16.4945,   -68.1389},
    {"Mauna Kea, US",                 19.82093, -155.46814},
    {"McMurdo Station., Antarctica", -77.85033,  166.69187},
    {"Nantes, France",                47.218,     -1.5528},
    {"Pontypridd, Wales",             51.59406,   -3.32126},
    {"Quebec, QC, Canada",            46.81224,  -71.20520},
    {"Sioux Falls, South Dakota, US", 43.56753,  -96.7245},
    {"Suzhou, CN",                    31.3347,   120.629},
    {"Zurich, CH",                    47.38,       8.543},
    {"Ocean Longitude 180 crossing",   0,       -180},
    {"Ocean cell center",              1/120.0, -180+1/120.0},
};


/**
* An example application demonstrating lookup and interpolation for
* a set of named places.
* @param argc the number of command-line arguments, including command vector, always one or greater.
* @param argv the command vector, argv[1] giving the path to the input file for processing.
* @return zero on successful completion; otherwise, an error code.
*/
int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "\nUsage:  ExampleLocation <input_file>\n");
		exit(0);
	}
	const char* target = argv[1];
	printf("Reading input file: %s\n", target);

    // Open the GVRS file.  If the file is not found or cannot be accessed,
    // GvrsOpen will set the global GvrsError flag and will return a null pointer.
    int status;
	Gvrs* gvrs = GvrsOpen(target, "r", &status);
	if (!gvrs) {
		printf("Unable to open GVRS file, error code %d\n", status);
		exit(1);
	}

    // The Deflate extensions introduce a dependency on zlib.  At this time, we have
    // not resolved the best way to handle this dependency in our build files.
    // So Deflate is not yet supported.  We've included it here for documentation purposes.
    // GvrsRegisterDeflateExtensions(gvrs);

    // For this demonstration program, it is not necessary to increase the tile cache.
    // Because the queries are at widely separated locations, each tile used by this program
    // is only used once. So there is no motivation to retain it in the cache,
    // and the default cache size (medium) is more than adequate for our needs.
    // GvrsSetTileCacheSize(gvrs, GvrsTileCacheSizeLarge);


    // This example assumes that the input file includes one or more elements
    // and the first one in the file is an elevation source
    GvrsElement* e = GvrsGetElementByIndex(gvrs, 0);


    size_t nPlaces = sizeof(places) / sizeof(struct Place);
    int iPlace;
    GvrsFloat fvalue;
    GvrsDouble row, col;
    GvrsInterpolationResult result;

    // for each place defined above, perform a lookup and print results
    // this example shows two results:
    //     single row, column lookup using a single raster data cell
    //     a lookup using a B-Spline interpolation
    for (iPlace = 0; iPlace < nPlaces; iPlace++) {
        struct Place p = places[iPlace];
        GvrsMapGeoToGrid(gvrs, p.lat, p.lon, &row, &col);
        int iRow, iCol;
        iRow = (int)(row + 0.5);
        iCol = (int)(col + 0.5);
        status = GvrsElementReadFloat(e, iRow, iCol, &fvalue);
        status = GvrsInterpolateBspline(e, p.lon, p.lat, 1, &result);
        if (status) {
            printf("Lookup failed for %s, grid coordinates (%d,%d), error %d\n", 
                p.name, iRow, iCol, status);
            break;
        }
        printf("%-30s %7.2f,  %7.2f,   %9.2f, %9.2f,   %12.1f  %8.1f\n",
            p.name, p.lat, p.lon, row, col, fvalue, result.z);

    }

    return 0;

}
