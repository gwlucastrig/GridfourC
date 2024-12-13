/* --------------------------------------------------------------------
 *
 * This file is in the public domain. 
 * 
 * This program demonstrates the ability to use the GVRS API to as a temporary
 * data store for collecting counts for a very large number of separate
 * elements.  In this case, the counts are used to compute probabilities
 * in support of the computation of first-order entropy (Shannon entropy).
 * 
 * Although this program uses a GVRS file as input, its logic could be adapted
 * to any data source that provided a four-byte integer or floating-point data type.
 * 
 * For more information on the computation performed by this program, see
 * https://gwlucastrig.github.io/GridfourDocs/notes/EntropyMetricForDataCompression.html
 * ---------------------------------------------------------------------
 */

#include "GvrsBuilder.h"
#include "GvrsCrossPlatform.h"
#include "GvrsError.h"
#include <math.h>


const char* usage[] = {

    "Example utility to tabulate the entropy of the contents of a GVRS file",
    "",
    "Usage:  TabulateEntropy <input file> [element identification]",
    "",
    "This program surveys an input file and tabulates the first-order information entropy",
    "for the specified element.  If no target element is provided, the program will",
    "use the first element in the file.",
    0
};
 


/**
* A test program that reads a GVRS file and tabulates its first-order entropy.
* @param argc the number of command-line arguments, including command vector, always one or greater.
* @param argv the command vector, argv[1] giving the path to the input file for processing.
* @return zero on successful completion; otherwise, an error code.
*/
int main(int argc, char* argv[]) {

    if (argc < 2) {
        const char** p = usage;
        while (*p) {
            printf("%s\n", *p);
            p++;
        }
        exit(0);
    }

    const char* inputFile = argv[1];
    char* inputElement = 0;
    if (argc > 2) {
        inputElement = argv[2];
    }
    int status;
    Gvrs* gInput;
    GvrsBuilder* builder;
    GvrsElementSpec* spec;
    GvrsElement** eInputArray;
    GvrsElement* eInput;
    GvrsElement* eCount;
    int iRow, iCol, xRow, xCol;
    int64_t nCellsInput;
    char eName[GVRS_ELEMENT_NAME_SZ + 4];
    status = GvrsOpen(&gInput, inputFile, "r");
    if (status) {
        printf("Error status %d opening input file\n", status);
        exit(1);
    }
    GvrsSetTileCacheSize(gInput, GvrsTileCacheSizeLarge);

    int nRowsInput = gInput->nRowsInRaster;
    int nColsInput = gInput->nColsInRaster;
    nCellsInput = (int64_t)nRowsInput * (int64_t)nColsInput;

    // For debugging, we can reduce the number of input rows and columns so that the
    // input file gets processed faster.
    // nRowsInput = 1000;   // for debugging
    // nColsInput = 180;

    eInput = 0;
    if (inputElement && inputElement[0]) {
        eInput = GvrsGetElementByName(gInput, inputElement);
        if (!eInput) {
            fprintf(stderr, "Source file does not provide element named \"%s\"\n", inputElement);
            exit(-1);
        }
    }
    else {
        int nElements;
        eInputArray = GvrsGetElements(gInput, &nElements);
        if (nElements < 1 || !eInputArray) {
            printf("Unable to access elements from input file\n");
            exit(1);
        }
        eInput = eInputArray[0];
        if (!eInput) {
            printf("Failed to obtain element from input file\n");
            exit(1);
        }
    }

    GvrsStrncpy(eName, sizeof(eName), eInput->name);

    // Use the GVRS builder to initialize a temporary integer-based raster that has enough
    // grid cells to collect the counts for the 2^32 separate values possible
    // for a 4-byte data value (integer or floating point).
    // The raster data store is initialized to 2^16-by-2^16 grid cells
    // with a "fill value" initialized to zero.
    //   To tabulate counts, loop through each value in the input file
    //      extract the integer value for the two high-order bytes and use them as the "row"
    //      extract the integer value for the two low-order bytes and use them as the "column"
    //      increment the value at the grid cell at position (row,column) in the "counting" raster
    
    GvrsBuilderInit(&builder, 65536, 65536);   // grid of 2^16 rows and columns
    GvrsBuilderSetTileSize(builder, 128, 128);
    GvrsBuilderAddElementInt(builder, "count", &spec);
    GvrsElementSpecSetFillValueInt(spec, 0);
    GvrsElementSpecSetRangeInt(spec, 0, INT_MAX);

    Gvrs* gCount;
    const char* countFile = "EntropyTabulationTemp.gvrs";
    remove(countFile);
    status = GvrsBuilderOpenNewGvrs(builder, countFile, &gCount);
    if (status) {
        printf("Error status %d to open temporary count file %s\n", status, countFile);
        exit(1);
    }

    GvrsSetTileCacheSize(gCount, GvrsTileCacheSizeLarge);
    GvrsSetDeleteOnClose(gCount, 1);

    eCount = GvrsGetElementByName(gCount, "count");
    if (!eCount) {
        printf("Failed to obtain count element from tabulation file\n");
        exit(1);
    }
    // collect counts
    printf("Tabulating symbol counts for input\n");
    printf("   File:    %s\n", inputFile);
    printf("   Element: %s\n", eInput->name);

    int64_t time0 = 0;
    int64_t time1 = 0;
    int64_t sumCounts = 0;
    int32_t  overflowEncountered = 0;
    int32_t iCount;
    int32_t maxCount = 0;
    int32_t maxCountValueInt = 0;
    float maxCountValueFloat = 0;
    int32_t fillCount = 0;

    // The program closes the GVRS input file as soon as it is done
    // counting.  At that point, the memory for the input GvrsElement structure, eInput,
    // will be freed. So we need to make copies of whatever of its fields will will want
    // when printing the results at the end of processing.
    //   Most GVRS elements are of either integral or floating-point data types.
    // But the Integer-Coded-Float (ICF) type is a hybrid of both.  For tabulation processes,
    // this program access the ICF type as integral, so the elementIsIntegral conditional block
    // comes first in the code below.  But that block of code also keeps track of floating-point
    // values for reporting the "maxCountValueFloat".

    int elementIsFloat = GvrsElementIsFloat(eInput);
    int elementIsIntegral = GvrsElementIsIntegral(eInput);
    int elementIsIcf = eInput->elementType == GvrsElementTypeIntCodedFloat;

    float fillValueFloat = eInput->fillValueFloat;
    int fillValueInt = eInput->fillValueInt;

    time0 = GvrsTimeMS();
    if (elementIsIntegral) {
        int iValue;
        for (iRow = 0; iRow < nRowsInput; iRow++) {
            if ((iRow % 1000) == 0) {
                GvrsSummarizeProgress(stdout, time0, "row", iRow, nRowsInput);
            }
            for (iCol = 0; iCol < nColsInput; iCol++) {
                status = GvrsElementReadInt(eInput, iRow, iCol, &iValue);
                if (status) {
                    printf("Input read failure at %d,%d\n", iRow, iCol);
                    exit(1);
                }

                xRow = (iValue >> 16) & 0xffff;
                xCol = iValue & 0xffff;
                status = GvrsElementCount(eCount, xRow, xCol, &iCount);
                if (status) {
                    if (status == GVRSERR_COUNTER_OVERFLOW) {
                        overflowEncountered = 1;
                        continue;
                    }
                    printf("Tabulation failure at %d,%d  0x%04x,0x%04x, status=%d\n", iRow, iCol, xRow, xCol, status);
                    GvrsSummarizeAccessStatistics(gCount, stdout);
                    exit(1);
                }
                if (iValue == fillValueInt) {
                    fillCount++;
                }else if (iCount > maxCount) {
                    maxCount = iCount;
                    maxCountValueInt = iValue;
                    if (eInput->elementType == GvrsElementTypeIntCodedFloat) {
                        // fundamentally, the integer-coded float element type is an integral data type
                        // but it does provide a corresponding floating-point value.  So look up the floating-point
                        // value to use for statistics reporting below.
                        status = GvrsElementReadFloat(eInput, iRow, iCol, &maxCountValueFloat);
                        if (status) {
                            printf("Input read failure at %d,%d\n", iRow, iCol);
                            exit(1);
                        }
                    }
                }
                sumCounts++;
            }
        }
    }
    else if (elementIsFloat) {
        // in the loop below, we wish to extract the bit pattern for each floating-point
        // value fetched from the input data source.  To do so, we declare a pointer
        // of type float (4-byte floating point) and set it to the address of an unsigned
        // four-byte integer.
        elementIsFloat = 1;
        int fillValueIsNan =  isnan(fillValueFloat);
        uint32_t iValue = 0;
        float* fValue = (float*)(&iValue);
        for (iRow = 0; iRow < nRowsInput; iRow++) {
            if ((iRow < 1000 && (iRow % 100) == 0) || (iRow % 1000) == 0) {
                GvrsSummarizeProgress(stdout, time0, "row", iRow, nRowsInput);
            }
            for (iCol = 0; iCol < nColsInput; iCol++) {
                status = GvrsElementReadFloat(eInput, iRow, iCol, fValue);
                if (status) {
                    printf("Input read failure at %d,%d\n", iRow, iCol);
                    exit(1);
                }
            
                // Get bit pattern for floating-point value 
                xRow = (iValue >> 16) & 0xffff;
                xCol = iValue & 0xffff;

                //printf("%12.4f  %08x\n", *fValue, iValue);
                status = GvrsElementCount(eCount, xRow, xCol, &iCount);
                if (status) {
                    if (status == GVRSERR_COUNTER_OVERFLOW) {
                        overflowEncountered = 1;
                        continue;
                    }
                    printf("Tabulation failure at %d,%d  0x%04x,0x%04x, status=%d\n", iRow, iCol, xRow, xCol, status);
                    GvrsSummarizeAccessStatistics(gCount, stdout);
                    exit(1);
                }
                if (fillValueIsNan && isnan(*fValue) || *fValue == fillValueFloat) {
                    fillCount++;
                }else if (iCount > maxCount) {
                    maxCount = iCount;
                    maxCountValueFloat = *fValue;
                }
                sumCounts++;
            }
        }
    }
    else {
        // This condition should not occur in the current implementation.
        // It is included as a fail-safe operation.
        printf("Unsupported format\n");
        exit(1);
    }
    time1 = GvrsTimeMS();
    printf("Counting completed in %lld milliseconds\n", (long long)(time1 - time0));
    GvrsClose(gInput);

    // The access statistics are intended to support software development
    // and performance testing.  They do not contribute information
    // directly related to entropy but are included here as a diagnostic.
    GvrsSummarizeAccessStatistics(gCount, stdout);

    time0 = GvrsTimeMS();

    printf("\nComputing entropy using tabulated counts\n");
    double entropy = 0;
    double sumCountsD = (double)sumCounts;
    int nTilesPopulated = 0;
    int nSymbols = 0;
    int nSymbolsUsedOnce = 0;
    int iTileRow, iTileCol, iValue;
    for (iTileRow = 0; iTileRow < gCount->nRowsOfTiles; iTileRow++) {
        int row0 = iTileRow * gCount->nRowsInTile;
        int row1 = row0 + gCount->nRowsInTile;
        for (iTileCol = 0; iTileCol < gCount->nColsOfTiles; iTileCol++) {
            int tileIndex = iTileRow * gCount->nColsOfTiles + iTileCol;
            if (!GvrsIsTilePopulated(gCount, tileIndex)) {
                continue;
            }
            nTilesPopulated++;
            int col0 = iTileCol * gCount->nColsInTile;
            int col1 = col0 + gCount->nColsInTile;
            for (iRow = row0; iRow < row1; iRow++) {
                for (iCol = col0; iCol < col1; iCol++) {
                    status = GvrsElementReadInt(eCount, iRow, iCol, &iValue);
                    if (status) {
                        printf("Error %d reading counts\n", status);
                        exit(1);
                    }
                    if (iValue > 0) {
                        nSymbols++;
                        if (iValue == 1) {
                            nSymbolsUsedOnce++;
                        }
                        double p = (double)iValue / sumCountsD;
                        double pLog = log(p);
                        entropy += p * pLog;
                    }
                }
            }
        }
    }

    entropy = -entropy/log(2.0);
    double aggregate = entropy * nCellsInput/8.0;
    int nSymbolsUsedMultipleTimes = nSymbols - nSymbolsUsedOnce;

    time1 = GvrsTimeMS();
    printf("Entropy computation completed in %lld milliseconds\n", (long long)(time1 - time0));
    printf("The survey process populated %d of %d tiles in the temporary tabulation file\n", nTilesPopulated, gCount->nBytesForTileData);
    printf("\n");
    printf("\n");
    printf("Entropy computed for input\n");
    printf("   File:    %s\n", inputFile);
    printf("   Element: %s\n", eName);
    printf("\n");
    printf("Entropy rate          %12.6f bits per value\n", entropy);
    printf("Entropy aggregate     %14.1f bytes\n", aggregate);
    printf("Cells in input grid:  %12lld\n", (long long)nCellsInput);
    printf("Unique symbols:       %12d\n", nSymbols);
    printf("  Used once:          %12d\n", nSymbolsUsedOnce);
    printf("  Used multple times: %12d\n", nSymbolsUsedMultipleTimes);

    if (overflowEncountered) {
        printf("Some counters exceeded the maximum integer value during processing\n");
    }
    if (elementIsIcf) {
        printf("  Maximum count:      %12ld,  value: %f, int-coded value: %ld\n", (long)maxCount, maxCountValueFloat, maxCountValueInt);
        printf("  Fill value count:   %12ld,  value: %f\n", (long)fillCount, fillValueFloat);
    }else if (elementIsFloat ) {
        printf("  Maximum count:      %12ld,  value: %f\n", (long)maxCount, maxCountValueFloat);
        printf("  Fill value count:   %12ld,  value: %f\n", (long)fillCount, fillValueFloat);
    }else {
        printf("  Maximum count:      %12ld,  value: %ld\n", (long)maxCount, maxCountValueInt);
        printf("  Fill value count:   %12ld,  value: %ld\n", (long)fillCount, fillValueInt);
    }
    
    printf("\n");
    GvrsClose(gCount);

}