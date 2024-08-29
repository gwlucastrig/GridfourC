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
#include "GvrsError.h"
#include "GvrsCodec.h"

// Simple differencing
void GvrsPredictor1(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output) {
	int i, iRow, iCol;
	GvrsInt prior = seed;
	output[0] = seed;
	for (i = 1; i < nColumns; i++) {
		prior += GvrsM32GetNextSymbol(m32);
		output[i] = prior;
	}

	for (iRow = 1; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		prior = output[index - nColumns];
		for (iCol = 0; iCol < nColumns; iCol++) {
			prior += GvrsM32GetNextSymbol(m32);
			output[index++] = prior;
		}

	}
}


void GvrsPredictor2(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output) {
	int iRow, iCol;
	GvrsLong prior = seed;
	output[0] = seed;
	output[1] = (GvrsInt)(GvrsM32GetNextSymbol(m32) + prior);
	for (iRow = 1; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		GvrsLong test = GvrsM32GetNextSymbol(m32) + prior;
		output[index] = (GvrsInt)test;
		output[index + 1] = (int)(GvrsM32GetNextSymbol(m32) + test);
		prior = test;
	}
	for (iRow = 0; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		GvrsLong a = output[index];
		GvrsLong b = output[index + 1];
		//accumulate second differences starting at column 2 for row
		for (iCol = 2; iCol < nColumns; iCol++) {
			GvrsInt residual = GvrsM32GetNextSymbol(m32);
			GvrsInt prediction = (int)(2LL * b - a);
			int c = prediction + residual;
			output[index + iCol] = c;
			a = b;
			b = c;
		}
	}
}

void GvrsPredictor3(int nRows, int nColumns, int seed, GvrsM32* m32, GvrsInt* output) {
	int i, iRow, iCol;
	// The zeroeth row and column are populated using simple differences.
	// All other columns are populated using the triangle-predictor
	output[0] = seed;
	int prior = seed;
	for (i = 1; i < nColumns; i++) {
		prior += GvrsM32GetNextSymbol(m32);
		output[i] = prior;
	}
	prior = seed;
	for (i = 1; i < nRows; i++) {
		prior += GvrsM32GetNextSymbol(m32);
		output[i * nColumns] = prior;
	}

	for (iRow = 1; iRow < nRows; iRow++) {
		int k1 = iRow * nColumns;
		int k0 = k1 - nColumns;
		for (iCol = 1; iCol < nColumns; iCol++) {
			long za = output[k0++];
			long zb = output[k1++];
			long zc = output[k0];
			int prediction = (int)(zb + zc - za);
			output[k1] = prediction + GvrsM32GetNextSymbol(m32);
		}
	}

}



// Simple differencing
int
 GvrsPredictor1encode(int nRows, int nColumns, GvrsInt *values, GvrsInt *encodedSeed, GvrsM32** m32Reference) {
	if (!values || !encodedSeed || !m32Reference) {
		return GVRSERR_NULL_ARGUMENT;
    }
	*m32Reference = 0;
	GvrsM32* m32 = GvrsM32AllocForOutput();
	if (!m32) {
		return GVRSERR_NOMEM;
	}
 
	*encodedSeed = values[0];
	int prior = *encodedSeed;
	for (int i = 1; i < nColumns; i++) {
		int test = values[i];
		int delta = test - prior;
		GvrsM32AppendSymbol(m32, delta);
		prior = test;
	}

	for (int iRow = 1; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		prior = values[index - nColumns];
		for (int i = 0; i < nColumns; i++) {
			int test = values[index++];
			int delta = test - prior;
			GvrsM32AppendSymbol(m32, delta);
			prior = test;
		}
	}

	*m32Reference = m32;
	return 0;
}


 GvrsPredictor2encode(int nRows, int nColumns, GvrsInt* values, GvrsInt* encodedSeed, GvrsM32** m32Reference) {
	if (!values || !encodedSeed || !m32Reference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*m32Reference = 0;
	GvrsM32* m32 = GvrsM32AllocForOutput();
	if (!m32) {
		return GVRSERR_NOMEM;
	}

	*encodedSeed = values[0];
	long delta, test;
	long prior = values[0];
	delta = (long)values[1] - prior;
	GvrsM32AppendSymbol(m32, (int)delta);
	for (int iRow = 1; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		test = values[index];
		delta = test - prior;
		GvrsM32AppendSymbol(m32, (int)delta);
		prior = test;

		test = values[index + 1];
		delta = test - prior;
		GvrsM32AppendSymbol(m32, (int)delta);
	}

	for (int iRow = 0; iRow < nRows; iRow++) {
		int index = iRow * nColumns;
		long a = values[index];
		long b = values[index + 1];
		//accumulate second differences starting at column 2
		for (int iCol = 2; iCol < nColumns; iCol++) {
			int c = values[index + iCol];
			int prediction = (int)(2L * b - a);
			int residual = c - prediction;
			GvrsM32AppendSymbol(m32, residual);
			a = b;
			b = c;
		}
	}

	*m32Reference = m32;
	return 0;
}


GvrsPredictor3encode(int nRows, int nColumns, GvrsInt* values, GvrsInt* encodedSeed, GvrsM32**m32Reference ) {
	if (!values || !encodedSeed || !m32Reference) {
		return GVRSERR_NULL_ARGUMENT;
	}
	*m32Reference = 0;
	GvrsM32* m32 = GvrsM32AllocForOutput();
	if (!m32) {
		return GVRSERR_NOMEM;
	}

	*encodedSeed = values[0];
	long prior = *encodedSeed;
	for (int i = 1; i < nColumns; i++) {
		long test = values[i];
		long delta = test - prior;
		GvrsM32AppendSymbol(m32, (int)delta);
		prior = test;
	}

	prior = *encodedSeed;
	for (int i = 1; i < nRows; i++) {
		long test = values[i * nColumns];
		long delta = test - prior;
		GvrsM32AppendSymbol(m32, (int)delta);
		prior = test;
	}

	// populate the rest of the grid using the triangle-predictor model
	for (int iRow = 1; iRow < nRows; iRow++) {
		int k1 = iRow * nColumns;
		int k0 = k1 - nColumns;
		for (int i = 1; i < nColumns; i++) {
			long za = values[k0++];
			long zb = values[k1++];
			long zc = values[k0];
			int prediction = (int) (zc + zb - za);
			int residual = values[k1] - prediction;
			GvrsM32AppendSymbol(m32, residual);
		}
	}

	*m32Reference = m32;
	return 0;
}