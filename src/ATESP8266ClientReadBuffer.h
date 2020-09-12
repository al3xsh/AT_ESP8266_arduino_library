/**
ATESP8266ClientReadBuffer.h

Arduino library for managing wifi connections using an ESP8266 in AT mode 
(using AT firmware v1.3.0).

The Sparkfun ESP8266 client library just uses a Serial.read() to get the data
from the ESP8266. However, this includes the stuff the ESP8266 uses to manage
how many bytes of payload it is receiving. Thus we get a response that looks
like:

+IPD,0,n:<payload>

To make this work properly, we need to strip this stuff out of the response :)

author: Alex Shenfield
date:   11/09/2020
*/

#ifndef _ATESP8266CLIENTBUFFER_H_
#define _ATESP8266CLIENTBUFFER_H_

#include <Arduino.h>

#define ESP8266_CLIENT_MAX_BUFFER_SIZE 256

// so we have a potential problem here - the max packet size is ~1450 bytes ...
// that means we have a tendency to lose data here

class ESP8266ClientReadBuffer {

public:
	int available();
	int read();

protected:
	char receiveBufferSize = 0;
	uint8_t receiveBuffer[ESP8266_CLIENT_MAX_BUFFER_SIZE];

	void fillReceiveBuffer();
	void truncateReceiveBufferHead(uint8_t startingOffset, uint8_t truncateLength);
	void cleanReceiveBufferFromAT();
	void cleanReceiveBufferFromAT(const char *atCommand, uint8_t additionalSuffixToKill);
};

/*
class CircularBuffer
{
public:
    CircularBuffer( uint16_t inputSize );
	~CircularBuffer();
    int16_t getElement( uint16_t ); //zero is the push location
    void pushElement( int16_t );
    int16_t averageLast( uint16_t );
    uint16_t recordLength( void );
private:
	uint16_t cBufferSize;
    int16_t *cBufferData;
    int16_t cBufferLastPtr;
    uint8_t cBufferElementsUsed;
};
*/

#endif
