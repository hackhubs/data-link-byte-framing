#include <stdio.h>
#include <stdbool.h>


/*  This file implements Data Link framing "Flag bytes with byte stuffing" as shown by
 *  https://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications/ */

#define START_FLAG 0x12
#define END_FLAG 0x13
#define ESCAPE_FLAG 0x7D

#define MAX_FRAME_SIZE 30
#define MAX_ENCODED_FRAME_SIZE (MAX_FRAME_SIZE*2+2)



void printBuffer(const char *bytes, int len){
    printf("Buffer len=%d: {",len);
    for(int i=0;i<len;i++){
        char currentByte = *(bytes+i);
        if (currentByte==START_FLAG){
            printf("START_FLAG, ");
        }
        else if (currentByte==END_FLAG){
            printf("END_FLAG, ");
        }
        else if (currentByte==ESCAPE_FLAG){
            printf("ESCAPE_FLAG, ");
        }
        else{
            printf("0x%X, ", *(bytes+i));
        }
    }
    printf("}\n");
}

bool encodeFrame(const char *bytesToEncode, int len, void (*writeToDevice)(const char *,int)){
    char encodedOutput[MAX_ENCODED_FRAME_SIZE];
    char *currentOutput = encodedOutput;
    const char *currentInput = bytesToEncode;

    if (len>MAX_FRAME_SIZE){
        printf("Frame to encode bigger than MAX_FRAME_SIZE=%d\n",MAX_FRAME_SIZE);
        return false;
    }

    *currentOutput = START_FLAG;
    currentOutput++;

    while(currentInput < bytesToEncode+len){
        if (*currentInput==START_FLAG || *currentInput==END_FLAG || *currentInput==ESCAPE_FLAG){
            *currentOutput = ESCAPE_FLAG;
            currentOutput++;
        }

        *currentOutput = *currentInput;
        currentOutput++;
        currentInput++;
    }

    *currentOutput = END_FLAG;
    currentOutput++;

    (*writeToDevice)(encodedOutput, currentOutput-encodedOutput);
    return true;
}

typedef enum {WAIT_HEADER, IN_MSG, AFTER_ESCAPE} DecodeState;

typedef struct{
    DecodeState state;
    char decodedOutput[MAX_FRAME_SIZE];
    char *currentOutput;
}DecodeData;


void decodeFrame(const char *bytesToDecode, int len, DecodeData *data, void (*processDecodedFrame)(const char *,int)){

    const char *currentInput = bytesToDecode;

    while(currentInput < bytesToDecode+len){
        printf("Received Byte: 0x%X (index=%ld) currentOutputIndex=%ld  currentState:%d\n",*currentInput, currentInput-bytesToDecode, data->currentOutput-data->decodedOutput, data->state);

        if (data->state==WAIT_HEADER){
            if (*currentInput==START_FLAG){
                printf("State:WAIT_HEADER, received START_FLAG\n");
                data->state=IN_MSG;
            }
            else{
                printf("State:WAIT_HEADER, waiting for START_FLAG.. currentByte: 0x%X\n", *currentInput);
                data->state=WAIT_HEADER;
            }
        }
        else if (data->state==IN_MSG){
            if (*currentInput==ESCAPE_FLAG){
                printf("State:IN_MSG, received ESCAPE_FLAG\n");
                data->state=AFTER_ESCAPE;
            }
            else if (*currentInput==END_FLAG){
                printf("State:IN_MSG, received END_FLAG, Frame decoded!\n");(*processDecodedFrame)(data->decodedOutput, data->currentOutput-data->decodedOutput);
                data->state=WAIT_HEADER;
                data->currentOutput = data->decodedOutput;
            }
            else if (*currentInput==START_FLAG){
                /* Something wrong happened!, restarting.. */
                printf("State:IN_MSG, not expecting START_FLAG.. resetting\n");
                data->state=WAIT_HEADER;
                data->currentOutput = data->decodedOutput;
                /* Skip increment currentInput. Maybe currentInput is START_FLAG from next packet */
                continue;
            }
            else{
                if (data->currentOutput-data->decodedOutput<MAX_FRAME_SIZE){
                    printf("State:IN_MSG, appending byte 0x%X to decoded frame\n",*currentInput);
                    data->state=IN_MSG;
                    *data->currentOutput = *currentInput;
                    data->currentOutput++;
                }
                else{
                    printf("State:IN_MSG, Decoding frame bigger than MAX_FRAME_SIZE=%d.. resetting\n",MAX_FRAME_SIZE);
                    /* Something wrong happened!, restarting.. */
                    data->state=WAIT_HEADER;
                    data->currentOutput = data->decodedOutput;
                    /* Skip increment currentInput. Maybe currentInput is START_FLAG from next packet */
                    continue;
                }
            }
        }
        else if (data->state==AFTER_ESCAPE){
            if (*currentInput==START_FLAG || *currentInput==END_FLAG || *currentInput==ESCAPE_FLAG){
                if (data->currentOutput-data->decodedOutput<MAX_FRAME_SIZE){
                    printf("State:AFTER_ESCAPE, appending byte 0x%X to decoded frame\n",*currentInput);
                    data->state=IN_MSG;
                    *data->currentOutput = *currentInput;
                    data->currentOutput++;
                }
                else{
                    /* Something wrong happened!, restarting.. */
                    printf("State:AFTER_ESCAPE, Decoding frame bigger than MAX_FRAME_SIZE=%d.. resetting\n",MAX_FRAME_SIZE);
                    data->state=WAIT_HEADER;
                    data->currentOutput = data->decodedOutput;
                    /* Skip increment currentInput. Maybe currentInput is START_FLAG from next packet */
                    continue;
                }
            }
            else{
                /* Something wrong happened!, restarting.. */
                printf("State:AFTER_ESCAPE, received normal char. Not expected.. resetting\n");
                data->state=WAIT_HEADER;
                data->currentOutput = data->decodedOutput;
                /* Skip increment currentInput. Maybe currentInput is START_FLAG from next packet */
                continue;
            }
        }
        else{
            printf("State:UNKOWN, resetting\n");
            data->state=WAIT_HEADER;
            data->currentOutput = data->decodedOutput;
        }

        /* Next input char */
        currentInput++;
    }

}


void processReceivedFrame(const char *buff,int len){
    printf("processReceivedFrame:");
    printBuffer(buff, len);
}

void writeEncodedFrameToDevice(const char *buff,int len){
    printf("writeBufferToDevice:");
    printBuffer(buff, len);
}


int main()
{
    char frame[] = {0x10,0x11,START_FLAG, 0x42,0x32,ESCAPE_FLAG,ESCAPE_FLAG,0x46, END_FLAG, 0x64};
    printf("Frame to encode:");printBuffer(frame,sizeof(frame));

    encodeFrame(frame,sizeof(frame), &writeEncodedFrameToDevice);

    DecodeData data;
    /* Decode data start values */
    data.currentOutput = data.decodedOutput;
    data.state = WAIT_HEADER;

    char frameToDecode[] = {START_FLAG,START_FLAG,START_FLAG,START_FLAG, 0x10, START_FLAG, 0x10, 0x11, ESCAPE_FLAG, START_FLAG, 0x42, 0x32, ESCAPE_FLAG, ESCAPE_FLAG, ESCAPE_FLAG, ESCAPE_FLAG, 0x46, ESCAPE_FLAG, END_FLAG, 0x64, END_FLAG, 0x45, 0x33};
    decodeFrame(frameToDecode,sizeof(frameToDecode), &data, &processReceivedFrame);


    char testDecodeMaxFrameSize[] = {ESCAPE_FLAG, START_FLAG, 0x79, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x00,0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x31, 0x0, 0x0, 0x55, 0x0, 0x0, 0x20, 0x0, 0x31, 0x0, 0x1, 0x0, START_FLAG, 0x1, END_FLAG};
    decodeFrame(testDecodeMaxFrameSize,sizeof(testDecodeMaxFrameSize), &data, &processReceivedFrame);

    return 0;
}
