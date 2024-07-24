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
 
#ifndef GVRS_INTERPOLATION_H
#define GVRS_INTERPOLATION_H

#ifdef __cplusplus
extern "C"
{
#endif


/**
* Defines a structure for receiving the results of a GVRS interpolation.
*/
typedef struct GvrsInterpolationResultTag {
    /**
    Indicates that the z-value was computed.
    */
    int computedZ;

    /**
    * Indicates that the first derivatives were computed by the interpolator.
    */
    int computedFirstDerivative;

    /**
    * Indicates that the second derivatives were computed by the interpolator.
    */
    int computedSecondDerivative;


    /**
    * A floating-point value indicating the row coordinate that was
    * specified for the interpolation. Row and column coordinates may be non-integral.
    */
    double row;

    /**
    * A floating-point value indicating the column coordinate that was
    * specified for the interpolation. Row and column coordinates may be non-integral.
    */
    double column;


    /**
    * The interpolated value for the coordinate
    */
    double z;

    /**
    * The value for the partial derivative of z with respect to x (in the
    * direction of the column axis).
    */
    double zx;

    /**
    * The value for the partial derivative of z with respect to y (in the
    * direction of the row axis).
    */
    double zy;

    /**
    * The value for the second derivative of z with respect to x (in the
    * direction of the column axis).
    */
    double zxx;

    /**
    * The value for the second partial derivative of z with respect to x and v.
    */
    double zxy;

    /**
    * The value for the second partial derivative of z with respect to y and u.
    */
    double zyx;

    /**
    * The value for the second partial derivative of z with respect to v.
    */
    double zyy;
}GvrsInterpolationResult;

/**
* Performs an interpolation based on the B-Spline technique.  This function uses a specified
* input grid and may be called without involving a GVRS data source.
* <p>
* The row and column specifications are real-valued specifications for the position
* within the grid at which the interpolation is to be conducted.  The input grid
* must be at least a 4-by-4 grid of values given in row-major order.
* <p>
* Derivatives may optionally be computed by specifying values of 0, 1, or 2 for
* the computeDerivatives parameter.  For a value of zero, no derivatives will be computed.
* For value of 1, only first derivatives will be computed.  For value of 2, both first
* and second derivatives will be computed.  Computing derivates requires extra processing.
* If derivatives are required, then the row and column spacing parameters are used to
* correctly scale the geometry of the grid for computations.  In cases where values for these
* scales are not available (or relevant), calling functions should supply values of 1.
* 
* @param row a real-valued row coordinate specification for the desired interpolation point.
* @param column a real-valued column coordinate specification for the desired interpolation point.
* @param nRowsInGrid the number of rows in the input grid.
* @param nColumnsInGrid the number of columns in the input grid.
* @param grid an array of floating-point values giving the input grid in row-major order.
* @param computeDerivatives an value of 0 (no derivatives), 1 (first derivative), or 2 (second derivative)
* @param rowSpacing the distance across grid rows, required for derivative computations
* @param columnSpacing the distance across grid columns, required for derivative computations
* @param result a pointer to a structure to receive the computation results.
* @return if successful, a zero; otherwise, an error code.
*/
int GvrsGeneralBspline(
    double row,
    double column,
    int nRowsInGrid,
    int nColumnsInGrid,
    float* grid,
    int computeDerivatives,
    double rowSpacing,
    double columnSpacing,
    GvrsInterpolationResult* result);


/**
* Performs an B-spline interpolation over GVRS raster data source.
* <p>
* The x and y specifications are real-valued coordinates for the position
* at which the interpolation is to be conducted.   These will correspond to either
* the <i>model</i> or <i>geographic</i> coordinates depending on the configuration
* of the GVRS data source.  Note that if geographic coordinates are used, they
* should be specified in the order longitude (x) followed by latitude (y), 
* not latitude and longitude.
* <p>
* Derivatives may optionally be computed by specifying values of 0, 1, or 2 for
* the computeDerivatives parameter.  For a value of zero, no derivatives will be computed.
* For value of 1, only first derivatives will be computed.  For value of 2, both first
* and second derivatives will be computed.  Computing derivates requires extra processing.
* If derivatives are required, then the row and column spacing parameters are used to
* correctly scale the geometry of the grid for computations.  In cases where values for these
* scales are not available (or relevant), calling functions should supply values of 1.
*
* @param element a valid element obtained from a GVRS data source.
* @param x a real-valued coordinate specification for the desired interpolation point.
* @param y a real-valued coordinate specification for the desired interpolation point.
* @param computeDerivatives an value of 0 (no derivatives), 1 (first derivative), or 2 (second derivative)
* @param result a pointer to a structure to receive the computation results.
* @return if successful, a zero; otherwise, an error code.
*/
int
GvrsInterpolateBspline(GvrsElement* element,
    double x, double y,
    int computeDerivatives,
    GvrsInterpolationResult* result);



#ifdef __cplusplus
}
#endif

#endif
