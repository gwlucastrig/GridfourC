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

#pragma once

#include "Gvrs.h"
#include "GvrsCodec.h"

GvrsCodec* GvrsCodecDeflateAlloc();
GvrsCodec* GvrsCodecFloatAlloc();
GvrsCodec* GvrsCodecLsopAlloc();

/**
* Allocates and registers instances of GVRS compressions codecs based on the Zlib (DEFLATE) library.
* Codecs are identified by name. If the specified GVRS instance already includes codecs with matching
* names, then the previously existing codecs will be replaced by new versions.
* <p>
* This function was adopted as a way of addressing build environments where it was inconvenient
* to include an external reference to the Zlib (DEFLATE) API.  By default, the GvrsOpen function
* will not register the Zlib-related codecs and does not have a build dependency of those codecs.
* Thus GVRS source files that do not include data compressed using DEFLATE can be accessed without
* introducing a dependency on Zlib.  GVRS source files that do use DEFLATE can be supported using
* this function to register the appropriate codecs.
* @param gvrs a valid, open instance of the GVRS data store.
* @return if successful, a zero; otherwise an error code.
*/
int GvrsRegisterDeflateExtensions(Gvrs* gvrs);
