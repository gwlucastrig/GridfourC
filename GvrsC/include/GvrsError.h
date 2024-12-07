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

#ifndef GVRS_ERROR_H
#define GVRS_ERROR_H

#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define GVRSERR_FILENOTFOUND          -1
#define GVRSERR_FILE_ACCESS           -2
#define GVRSERR_INVALID_FILE           -3
#define GVRSERR_VERSION_NOT_SUPPORTED -4
#define GVRSERR_EXCLUSIVE_OPEN        -5   // opened for exclusive access by other process
#define GVRSERR_NOMEM                 -6   // malloc error
#define GVRSERR_EOF                   -7   // premature EOF
#define GVRSERR_FILE_ERROR            -8   // general file errors
// #define GVRSERR_NULL_POINTER          -9   // application passed a null argument to GVRS
#define GVRSERR_ELEMENT_NOT_FOUND    -10
#define GVRSERR_COORDINATE_OUT_OF_BOUNDS    -11
#define GVRSERR_COMPRESSION_NOT_IMPLEMENTED -12
#define GVRSERR_BAD_COMPRESSION_FORMAT      -13
#define GVRSERR_BAD_RASTER_SPECIFICATION    -14
#define GVRSERR_BAD_NAME_SPECIFICATION      -15
#define GVRSERR_BAD_ICF_PARAMETERS          -16
#define GVRSERR_BAD_ELEMENT_SPEC            -17
#define GVRSERR_NULL_ARGUMENT               -18
#define GVRSERR_NOT_OPENED_FOR_WRITING      -19
#define GVRSERR_COMPRESSION_FAILURE         -20    // includes both encode and decode errors
#define GVRSERR_INTERNAL_ERROR              -21
#define GVRSERR_NAME_NOT_UNIQUE             -22
#define GVRSERR_INVALID_PARAMETER           -23
#define GVRSERR_COUNTER_OVERFLOW            -24


#ifdef __cplusplus
}
#endif


#endif