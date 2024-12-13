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

#ifndef GVRS_CROSS_PLATFORM_H
#define GVRS_CROSS_PLATFORM_H

#include "GvrsFramework.h"



#ifdef __cplusplus
extern "C"
{
#endif
 


/**
 * Gets the current clock time in milliseconds since epoch Jan 1, 1970.
 * <p>
 * This function was implemented to address compatibility issues across Windows and Linux operating systems
 * @return a positive integee.
 */ 
int64_t GvrsTimeMS();


/**
* Performs a robust string copy operation.  While this function is similar to the POSIX strncpy,
* it differs in the order of its arguments and it also ensures that the destination string is always null terminated.
* <p>
* This function was implemented to address compatibility issues across Windows and Linux operating systems.
* @param destination a valid location to receive the content of the copy operation.
* @param destinationSize a positive integral value giving the maximum size of the destination memory, including the null terminator.
* @param source the source string to be copied.
* @return if successful, zero; otherwise, an error code.
*/
int GvrsStrncpy(char* destination, size_t destinationSize, const char* source);



#ifdef __cplusplus
}
#endif


#endif