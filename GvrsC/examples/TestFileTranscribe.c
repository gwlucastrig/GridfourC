#include "GvrsBuilder.h"
#include "GvrsCrossPlatform.h"

void TestFileTranscribe(const char* input, const char* output, int compress) ;

int main(int argc, char *argv[]){
   TestFileTranscribe(argv[1], argv[2], 1);
}

void TestFileTranscribe(const char* input, const char* output, int compress) {
    printf("GVRS transcription test, data compression %s\n", compress ? "enabled" : "disabled");
    printf("Opening %s\n", input);
    int status;
    Gvrs* gInput;
    status = GvrsOpen(&gInput, input, "r");
    if(!gInput){
        printf("Error code %d opening file\n", status);
        exit(1);
    }
    int nRows = gInput->nRowsInRaster;
    int nCols = gInput->nColsInRaster;
    GvrsBuilder* builder;
    status    = GvrsBuilderInit(&builder, nRows, nCols);
    GvrsBuilderSetTileSize(builder, gInput->nRowsInTile, gInput->nColsInTile);
    GvrsBuilderSetChecksumEnabled(builder, 1);

    if (compress) {
        GvrsBuilderRegisterStandardDataCompressionCodecs(builder);
    }

    int iElement, nElements;
    GvrsElement** eInput = GvrsGetElements(gInput, &nElements);
         for (iElement = 0; iElement < nElements; iElement++) {
        GvrsElement* e = eInput[iElement];
        // TO DO:  we need to add range and fill stuff here.
        //  Maybe we should also add an element-copy function for convenience of developers.
        GvrsElementSpec* spec;
        switch (e->elementType) {
        case GvrsElementTypeInt:
            GvrsBuilderAddElementInt(builder, e->name, &spec);
            GvrsElementSpecSetRangeInt(spec, e->elementSpec.intSpec.minValue, e->elementSpec.intSpec.maxValue);
            break;
        case GvrsElementTypeIntCodedFloat: {
            GvrsElementSpecIntCodedFloat icfSpec = e->elementSpec.intFloatSpec;
            GvrsBuilderAddElementIntCodedFloat(builder, e->name, icfSpec.scale, icfSpec.offset, &spec);
            break;
        }    
        case GvrsElementTypeFloat:
            GvrsBuilderAddElementFloat(builder, e->name, &spec);
            break;
        case GvrsElementTypeShort:
            GvrsBuilderAddElementShort(builder, e->name, &spec);
            GvrsElementSpecSetRangeInt(spec, e->elementSpec.shortSpec.minValue, e->elementSpec.shortSpec.maxValue);
            GvrsElementSpecSetFillValueInt(spec, e->elementSpec.shortSpec.fillValue);
            break;
        default:
            printf("Unidentified element type %d\n", (int)e->elementType);
            exit(1);
        }

        GvrsElementSpecSetContinuous(spec, e->continuous);
        GvrsElementSpecSetDescription(spec, e->description);
        GvrsElementSpecSetLabel(spec, e->label);
        GvrsElementSpecSetUnitOfMeasure(spec, e->unitOfMeasure);
    }


    Gvrs* gOutput;
    status = GvrsBuilderOpenNewGvrs(builder, output, &gOutput);
    GvrsBuilderFree(builder);
    if(!gOutput){
       printf("Error %d building new GVRS file %s\n", status, output);
       exit(1);
    }

    GvrsSetTileCacheSize(gInput, GvrsTileCacheSizeLarge);
    GvrsSetTileCacheSize(gOutput, GvrsTileCacheSizeLarge);
    
    GvrsElement** eOutput = GvrsGetElements(gOutput, &nElements);

    GvrsLong time0 = GvrsTimeMS();
    GvrsInt iValue;
    for (int iRow = 0; iRow < nRows; iRow++) {
        if ((iRow % 100) == 0) {
            printf("row %d\n", iRow);
        }
 
        for (int iCol = 0; iCol < nCols; iCol++) {
            for (int iElement = 0; iElement < nElements; iElement++) {
                status = GvrsElementReadInt(eInput[iElement], iRow, iCol, &iValue);
                if (status) {
                    printf("Error on input\n");
                    exit(1);
                }
                status = GvrsElementWriteInt(eOutput[iElement], iRow, iCol, iValue);
                if (status) {
                    printf("Error on output %d\n\n", status);
                    exit(1);
                }
            }
        }
    }
    GvrsLong time1 = GvrsTimeMS();
    printf("copy operation completed in %lld ms\n", (long long)(time1 - time0));
    GvrsSummarizeAccessStatistics(gOutput, stdout);
 
    status = GvrsClose(gInput);
    status = GvrsClose(gOutput);
    if(status){
        printf("Transcription failed on close operation with status %d\n", status);
        exit(1);
    }
    time1 = GvrsTimeMS();
    printf("transcription completed in %lld ms\n", (long long)(time1-time0));

    printf("\nInspecting output\n");



    
    status = GvrsOpen(&gInput, input, "r");
    status = GvrsOpen(&gOutput, output, "r");
    eInput = GvrsGetElements(gInput, &nElements);
    eOutput = GvrsGetElements(gOutput, &nElements);
    GvrsSetTileCacheSize(gInput, GvrsTileCacheSizeLarge);
    GvrsSetTileCacheSize(gOutput, GvrsTileCacheSizeLarge);
    
    GvrsMetadataResultSet* resultSet;
    status = GvrsReadMetadataByName(gInput, "*",  &resultSet);
    if (!status) {
        int i;
        for (i = 0; i < resultSet->nRecords; i++) {
            GvrsMetadata* m = resultSet->records[i];
            printf("metadata: %s\n", m->name);
            GvrsMetadataWrite(gOutput, m);
        }
    }
    GvrsMetadataResultSetFree(resultSet);

    GvrsMetadata* mNote;
    status = GvrsMetadataInit("Note", 0, &mNote);
    GvrsMetadataSetAscii(mNote, "Created using transcription test");
    GvrsMetadataSetDescription(mNote, "This is a metadata example");
    GvrsMetadataWrite(gOutput, mNote);
    GvrsMetadataFree(mNote);

    time0 = GvrsTimeMS();
    GvrsInt iValue0, iValue1;
    GvrsLong iSum = 0;
    GvrsInt nSum = 0;
    for (int iRow = 0; iRow < nRows; iRow++) {
        if ((iRow % 1000) == 0) {
            printf("verifying row %d\n", iRow);
        }
        for (int iCol = 0; iCol < nCols; iCol++) {
            for (int iElement = 0; iElement < nElements; iElement++) {
                status = GvrsElementReadInt(eInput[iElement], iRow, iCol, &iValue0);
                if (status) {
                    printf("Error reading source file: %d at %d,%d\n", status, iRow, iCol);
                    exit(1);
                }
                status = GvrsElementReadInt(eOutput[iElement], iRow, iCol, &iValue1);
                if (status) {
                    printf("Error reading transcribed file: %d at %d,%d\n", status, iRow, iCol);
                    exit(1);
                }
                if (iValue0 != iValue1) {
                    printf("Verification failed at %d,%d\n", iRow, iCol);
                    exit(1);
                }
                iSum += iValue1;
                nSum++;
             
            }
        }
    } 

    GvrsClose(gInput);
    GvrsClose(gOutput);
    time1 = GvrsTimeMS();
    printf("Completed inspection in %lld ms, average value %f\n", time1-time0, (double)iSum / (double)nSum);
}
