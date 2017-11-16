#  Data Link framing using "Flag bytes with byte stuffing" 

Implemented after https://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications/. Frame size limit is defined by MAX_FRAME_SIZE.

# Frame Encoding

The frame to be encoded is assumed to be complete when calling **encodeFrame**. 
Callback *writeToDevice* receives encoded bytes

# Frame Decoding

DecodeData stores **decodeFrame** state, allowing consecutively calling with any number of bytes. Callback processDecodedFrame received decoded frame if any is formed.


DecodeData should be always initialized once with:
```
    data.currentOutput = data.decodedOutput;
    data.state = WAIT_HEADER;
```
As DecodeData uses pointer aritimetic to store output index, it should *never* be copied.
