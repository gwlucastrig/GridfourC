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

#include <time.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <sys/timeb.h>
#endif

#include "Gvrs.h"

int64_t GvrsTimeMS() {
#if defined(_WIN32) || defined(_WIN64)
	struct timeb timex;
	ftime(&timex);
	return  1000LL * (int64_t)timex.time + (int64_t)timex.millitm;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
#endif

}


int
GvrsStrncpy(char* destination, size_t destinationSize, const char* source) {
	if (!(destination && source && destinationSize > 0)){
		return -1;
	}
	// in this copy operation, overwrite any unused bytes with zeroes.
	// the motivation for this approach is to prevent any inadvertant exposure
	// of data from the calling routing
	size_t i;
	for (i = 0; i < destinationSize-1 && source[i] != '\0'; i++)
		destination[i] = source[i];
	for (; i < destinationSize; i++)
		destination[i] = '\0';
	return 0;
 }
