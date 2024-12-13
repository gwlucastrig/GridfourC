
#include "GvrsBuilder.h"


void TestLargeFile(const char* outputPath) {

    int status;
    int k;

    
    int nRowsInTile = 128;
    int nColsInTile = 128;

    int nRowsOfTiles = 257;
    int nColsOfTiles = 257;

    int nRowsInGrid = nRowsOfTiles * nRowsInTile;
    int nColsInGrid = nColsOfTiles * nColsInTile;

    long long nCells = (long long)nRowsInGrid * (long long)nColsInGrid;
    long long nBytesInGrid = nCells * 4LL;

    printf("Storage size in Gigabytes = %f\n", nBytesInGrid / (1024.0 * 1024.0 * 1024.0));
 
    GvrsBuilder* builder;
    status = GvrsBuilderInit(&builder, nRowsInGrid, nColsInGrid);
    GvrsBuilderSetTileSize(builder, nRowsInTile, nColsInTile);
    GvrsBuilderSetChecksumEnabled(builder, 1);

    GvrsElementSpec* spec;
    GvrsBuilderAddElementInt(builder, "count", &spec);
    GvrsElementSpecSetRangeInt(spec, 0, INT_MAX);
    GvrsElementSpecSetFillValueInt(spec, 0);

    Gvrs* gvrs;
    GvrsElement* eCount;

    status = GvrsBuilderOpenNewGvrs(builder, outputPath, &gvrs);
    if (status) {
        printf("Test failed opening new file, status %d\n", status);
        exit(1);
    }

    eCount = GvrsGetElementByName(gvrs, "count");
    if (!eCount) {
        printf("Test failed, could not find element by name\n");
        exit(1);
    }

    //nRowsOfTiles -= 8;
    printf("Writing test file %s\n", outputPath);
    k=1;
    for (int iTileRow = 0; iTileRow < nRowsOfTiles; iTileRow++) {
        printf("Processing row %d\n", iTileRow);

        for (int iTileCol = 0; iTileCol < nColsOfTiles; iTileCol++) {
            int gridRow = iTileRow * nRowsInTile;
            int gridCol = iTileCol * nColsInTile;
            status = GvrsElementWriteInt(eCount, gridRow, gridCol, k);
            if (status) {
                printf("Test failed on write operation at % d, % d: status %d\n", gridRow, gridCol, status);
                exit(1);
            }
            k++;
        }
    }



    printf("Closing output file\n");
    
    status = GvrsClose(gvrs);
    printf("Gvrs file closed with status %d\n", status);
    if (status) {
        exit(1);
    }
    status = GvrsOpen(&gvrs, outputPath, "r");
    if (status) {
        printf("Gvrs file open failed with status %d\n", status);
        exit(1);
    }
    eCount = GvrsGetElementByName(gvrs, "count");
    if (!eCount) {
        printf("Test failed, could not find element by name\n");
        exit(1);
    }
    printf("Reading test file");
    k = 1;
    for (int iTileRow = 0; iTileRow < nRowsOfTiles; iTileRow++) {
        for (int iTileCol = 0; iTileCol < nColsOfTiles; iTileCol++) {
            int gridRow = iTileRow * nRowsInTile;
            int gridCol = iTileCol * nColsInTile;
            int iValue;
            status = GvrsElementReadInt(eCount, gridRow, gridCol, &iValue);
            if (status) {
                printf("Test failed on read operation at % d, % d: status %d\n", gridRow, gridCol, status);
                exit(1);
            }
            if (iValue != k) {
                printf("Test failed on read operation at % d, % d: read %d, expected %d\n", gridRow, gridCol, iValue, k);
                exit(1);
            }
            k++;
        }
    }

    GvrsClose(gvrs);

    printf("Reading test successful\n");

}


