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

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#include "GvrsInterpolation.h"
#include "GvrsError.h"


int GvrsGeneralBspline(
    double row,
    double column,
    int nRowsInGrid,
    int nColumnsInGrid,
    float* grid,
    int computeDerivatives,
    double rowSpacing,
    double columnSpacing,
    GvrsInterpolationResult *result) {
   
    memset(result, 0, sizeof(GvrsInterpolationResult));
    result->row = row;
    result->column = column;
 
    if (nColumnsInGrid < 4) {
        return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
    }
    if (nRowsInGrid < 4) {
        return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
    }

    double uCol = floor(column);
    double vRow = floor(row);
    double u = column - uCol;
    double v = row - vRow;

    int iCol = (int)uCol;
    int iRow = (int)vRow;
    int col0 = iCol - 1;
    int row0 = iRow - 1;

    if (iCol < 0 || iCol > nColumnsInGrid - 1) {
        return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
    }
 

    if (iRow < 0 || iRow > nRowsInGrid - 1) {
        GVRSERR_COORDINATE_OUT_OF_BOUNDS;
    }
 

    // make special adjustments for query points on the outer band
    // of the raster.
    if (col0 < 0) {
        col0 = 0;
        u = column - 1.0; // will be a negative number
    }
    else if (col0 > nColumnsInGrid - 4) {
        col0 = nColumnsInGrid - 4;
        u = column - 1.0 - col0;
    }

    if (row0 < 0) {
        row0 = 0;
        v = row - 1.0;
    }
    else if (row0 > nRowsInGrid - 4) {
        row0 = nRowsInGrid - 4;
        v = row - 1.0 - row0;
    }

    int offset = row0 * nColumnsInGrid + col0;
    double z00 = grid[offset];
    double z01 = grid[offset + 1];
    double z02 = grid[offset + 2];
    double z03 = grid[offset + 3];
    offset += nColumnsInGrid;
    double z10 = grid[offset];
    double z11 = grid[offset + 1];
    double z12 = grid[offset + 2];
    double z13 = grid[offset + 3];
    offset += nColumnsInGrid;
    double z20 = grid[offset];
    double z21 = grid[offset + 1];
    double z22 = grid[offset + 2];
    double z23 = grid[offset + 3];
    offset += nColumnsInGrid;
    double z30 = grid[offset];
    double z31 = grid[offset + 1];
    double z32 = grid[offset + 2];
    double z33 = grid[offset + 3];

    //    In the code below, we use the variables b and p to represent
    // the "basis functions" for the B-Spline. Traditionally, the variable
    // b is used for ordinary B-Splines, but since we are performing a 
    // 2-D computation, we need a second variable.  
    //    For derivatives, we use the variable bu for db/du and
    // pv for dp/dv.   Second derivatives would be buu, pvv, etc.
    // compute basis weighting factors b(u) in direction of column axis
    //    In the case of the derivatives, note that the computations
    // all involve one or two divisions by the rowScale and columnScale.
    // These are a consequence of the chain rule from calculus
    // and the fact that we need to ensure coordinate axes are isotropic
    // (see Javadoc above).  So the grid coordinates can be viewed as 
    // functions of x, and y, with grid column u(x) and row v(y).
    // For example, recall that the column scale factor is the distance
    // between columns.  So we have an x coordinate x = u*columnScale.
    // Thus u(x) = x/columnScale and du/dx = 1/columnScale.
    // So when we take a derivative of b(u), we have
    //    db/dx  = (db/du)*(du/dx)  =   (db/du)/columnScale.
    //
    double um1 = 1.0 - u;
    double b0 = um1 * um1 * um1 / 6.0;
    double b1 = (3 * u * u * (u - 2) + 4) / 6.0;
    double b2 = (3 * u * (1 + u - u * u) + 1) / 6.0;
    double b3 = u * u * u / 6.0;

    // comnpute basis weighting factors p(v) in direction of row axis
    double vm1 = 1.0 - v;
    double p0 = vm1 * vm1 * vm1 / 6.0;
    double p1 = (3 * v * v * (v - 2) + 4) / 6.0;
    double p2 = (3 * v * (1 + v - v * v) + 1) / 6.0;
    double p3 = v * v * v / 6.0;

    // combine sample points using the basis weighting functions
    // and create four partial results, one for each row of data.
    double s0 = b0 * z00 + b1 * z01 + b2 * z02 + b3 * z03;
    double s1 = b0 * z10 + b1 * z11 + b2 * z12 + b3 * z13;
    double s2 = b0 * z20 + b1 * z21 + b2 * z22 + b3 * z23;
    double s3 = b0 * z30 + b1 * z31 + b2 * z32 + b3 * z33;

    // combine the 4 partial results, computing in the y direction
    result->z = p0 * s0 + p1 * s1 + p2 * s2 + p3 * s3;
    result->computedZ = 1;
 
    if (computeDerivatives==0) {
        return 0;
    }

    if (columnSpacing == 0 || rowSpacing == 0) {
        return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
    }

    // compute derivatives of basis functions b, bu(i)=(db/du)(i)
    double bu0 = -um1 * um1 / 2.0 / columnSpacing;
    double bu1 = (3.0 * u / 2.0 - 2.0) * u / columnSpacing;
    double bu2 = (0.5 - (3.0 * u / 2.0 - 1.0) * u) / columnSpacing;
    double bu3 = u * u / 2.0 / columnSpacing;

    // compute derivatives of basis functions pv(i) = (dp/dv)(i)
    double pv0 = -vm1 * vm1 / 2.0 / rowSpacing;
    double pv1 = (3.0 * v / 2.0 - 2.0) * v / rowSpacing;
    double pv2 = (0.5 - (3.0 * v / 2.0 - 1.0) * v) / rowSpacing;
    double pv3 = v * v / 2.0 / rowSpacing;

    // using the partial derivatives of the basis functions db/bu,
    // interpolate dz/bu at u for each row, then combine the interplations
    // the compute partial derivative dz/bu at (u, v).
    s0 = bu0 * z00 + bu1 * z01 + bu2 * z02 + bu3 * z03;
    s1 = bu0 * z10 + bu1 * z11 + bu2 * z12 + bu3 * z13;
    s2 = bu0 * z20 + bu1 * z21 + bu2 * z22 + bu3 * z23;
    s3 = bu0 * z30 + bu1 * z31 + bu2 * z32 + bu3 * z33;
    result->zx = p0 * s0 + p1 * s1 + p2 * s2 + p3 * s3;

    // using the partial derivatives of the basis functions db/pv,
    // interpolate dz/bu at u for each row, then combine the interplations
    // the compute partial derivative dz/bu at (u, v).
    double t0 = pv0 * z00 + pv1 * z10 + pv2 * z20 + pv3 * z30;
    double t1 = pv0 * z01 + pv1 * z11 + pv2 * z21 + pv3 * z31;
    double t2 = pv0 * z02 + pv1 * z12 + pv2 * z22 + pv3 * z32;
    double t3 = pv0 * z03 + pv1 * z13 + pv2 * z23 + pv3 * z33;

    result->zy = b0 * t0 + b1 * t1 + b2 * t2 + b3 * t3;

    result->computedFirstDerivative = 1;
    if (computeDerivatives == 1) {
        return 0;
    }

    result->zxy = pv0 * s0 + pv1 * s1 + pv2 * s2 + pv3 * s3;
    result->zyx = result->zxy;

    double buu0 = (1 - u) / (columnSpacing * columnSpacing);
    double buu1 = (3 * u - 2) / (columnSpacing * columnSpacing);
    double buu2 = (1 - 3 * u) / (columnSpacing * columnSpacing);
    double buu3 = u / (columnSpacing * columnSpacing);

    s0 = buu0 * z00 + buu1 * z01 + buu2 * z02 + buu3 * z03;
    s1 = buu0 * z10 + buu1 * z11 + buu2 * z12 + buu3 * z13;
    s2 = buu0 * z20 + buu1 * z21 + buu2 * z22 + buu3 * z23;
    s3 = buu0 * z30 + buu1 * z31 + buu2 * z32 + buu3 * z33;
    result->zxx = p0 * s0 + p1 * s1 + p2 * s2 + p3 * s3;

    double pvv0 = (1 - v) / (rowSpacing * rowSpacing);
    double pvv1 = (3 * v - 2) / (rowSpacing * rowSpacing);
    double pvv2 = (1 - 3 * v) / (rowSpacing * rowSpacing);
    double pvv3 = v / (rowSpacing * rowSpacing);

    t0 = pvv0 * z00 + pvv1 * z10 + pvv2 * z20 + pvv3 * z30;
    t1 = pvv0 * z01 + pvv1 * z11 + pvv2 * z21 + pvv3 * z31;
    t2 = pvv0 * z02 + pvv1 * z12 + pvv2 * z22 + pvv3 * z32;
    t3 = pvv0 * z03 + pvv1 * z13 + pvv2 * z23 + pvv3 * z33;

    result->zyy = b0 * t0 + b1 * t1 + b2 * t2 + b3 * t3;
    result->computedSecondDerivative = 1;

    return 0;
}



// To get a meaningful first and second derivative for the surface slope
// we would need to convert angular units for geographic coordinates
// to a unit of distance. This value needs to be in the same unit of measure
// as the element. To convert degrees to meters, this implemenetation uses
// the radius of a sphere with the same surface area of Earth (source datum WGS84).
static const double degreesToMetersScale = (6371007.2 * M_PI / 180.0);



int
GvrsInterpolateBspline(GvrsElement* e,
    double x, double y, 
    int computeDerivatives, 
    GvrsInterpolationResult* result) {
    
    Gvrs* gvrs = (Gvrs*)e->gvrs;

    double row, col;
    double yRow, xCol;
    int row0, col0;

    int nRowsInRaster = gvrs->nRowsInRaster;
    int nColsInRaster = gvrs->nColsInRaster;
    double rowSpacing;
    double colSpacing;

    int geoCoordinates = gvrs->geographicCoordinates;
    if (geoCoordinates) {
        GvrsMapGeoToGrid(gvrs, y, x, &row, &col);
        double phi = y * M_PI / 180.0;  // latitude in radians
        rowSpacing = gvrs->cellSizeY * degreesToMetersScale * e->unitsToMeters;
        colSpacing = gvrs->cellSizeX * degreesToMetersScale * cos(phi) * e->unitsToMeters;
    }
    else {
        GvrsMapModelToGrid(gvrs, x, y, &row, &col);
        rowSpacing = gvrs->cellSizeY;
        colSpacing = gvrs->cellSizeX;
    }
    float grid[16];

    // The y/row logic does not have the potential of wrapping. So it's straightforward
    // If the row is out of range, but still in the fringes, truncate its value.
    // If the row is beyond the fringe, reject the input.
    if (row <= 0) {
        if (row >= -0.5) {
            yRow = 0;
            row0 = 0;
        }
        else {
            return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
        }
    }
    else if (row > nRowsInRaster - 1) {
        if (row <= nRowsInRaster - 0.5) {
            row0 = nRowsInRaster - 4;
            yRow = 4;
        }
        else {
            return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
        }
    }
    else {
        row0 = (int)floor(row) - 1;
        if (row0 < 0) {
            row0 = 0;
        }
        else if (row0 > nRowsInRaster - 4) {
            row0 = nRowsInRaster - 4;
        }
        yRow = row - row0;
    }


    if (gvrs->geoWrapsLongitude || gvrs->geoBracketsLongitude) {
        // The range of longitude values covers the full 360 degrees.
        // In some cases, it could span across the dividing longitude.
        // For example, a coordinate system with longitude interval [-180, 180) 
        // with an input interpolation longitude of -179.999 could include
        // columns nColsInRaster-2, nColsInRaster-1, 0, and 1.  So we need logic
        // to ensure that the proper columns are obtained from the GVRS source raster
        int n = nColsInRaster;
        if (gvrs->geoBracketsLongitude) {
            n--;
        }
        int col0 = (int)floor(col) - 1;
        xCol = col - col0;
        if (col0 < 0) {
            col0 = (col0 + n) % n;
        }
        int k = 0;
        for (int iRow = 0; iRow < 4; iRow++) {
            for (int iCol = 0; iCol < 4; iCol++) {
                GvrsElementReadFloat(e, iRow + row0, (iCol + col0)%n, grid + k);
                k++;
            }
        }
    }
    else {
        // Since we are not dealing with coordinate wrapping, 
        // need to perform fringe truncation for the x coordinates in the same manner
        // as for the fringe logic for the y coordinates from the code above.
        if (col < 0) {
            if (col >= -0.5) {
                xCol = 0;
                col0 = 0;
            }
            else {
                return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
            }
        }
        else if (col > nColsInRaster - 1) {
            if (col <= nColsInRaster - 0.5) {
                col0 = nColsInRaster - 4;
                xCol = 4;
            }
            else {
                return GVRSERR_COORDINATE_OUT_OF_BOUNDS;
            }
        }
        else {
            col0 = (int)floor(col) - 1;
            if (col0 < 0) {
                col0 = 0;
            }
            else if (col0 > nColsInRaster - 4) {
                col0 = nColsInRaster - 4;
            }
            xCol = col - col0;
        }
        int k = 0;
        for (int iRow = 0; iRow < 4; iRow++) {
            for (int iCol = 0; iCol < 4; iCol++) {
                GvrsElementReadFloat(e, iRow + row0, iCol + col0, grid + k);
                k++;
            }
        }
    }
         
    int status = GvrsGeneralBspline(yRow, xCol, 4, 4, grid, computeDerivatives, rowSpacing, colSpacing, result);
 

    return 0;

}