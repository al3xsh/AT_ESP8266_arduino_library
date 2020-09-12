/**
ATESP8266ClientReadBuffer.cpp

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

#include "ATESP8266WiFi.h"
#include "ATESP8266ClientReadBuffer.h"

int ESP8266ClientReadBuffer::available()
{
	// client has already buffered some payload
	if (receiveBufferSize > 0)
	{
		return receiveBufferSize;
	}

	// check if data is available
	int available = esp8266.available();
	if (available == 0)
	{
		// delay for the amount of time it'd take to receive one character
		delayMicroseconds((1 / esp8266._baud) * 10 * 1E6);
		// check again just to be sure
		available = esp8266.available();
	}
	return available; //esp8266.available();
}

int ESP8266ClientReadBuffer::read()
{
	// append to buffer BEFORE we read, so that chances are higher to detect AT commands
	this->fillReceiveBuffer();

	// read from buffer
	if (receiveBufferSize > 0) {
		uint8_t ret = receiveBuffer[0];
		this->truncateReceiveBufferHead(0, 1);
		return ret;
	}

	return -1;
}

void ESP8266ClientReadBuffer::truncateReceiveBufferHead(uint8_t startingOffset, uint8_t truncateLength) 
{
	// shift buffer content; todo: better implementation
	for (uint8_t i = startingOffset; i < receiveBufferSize - truncateLength; i++)
	{
		receiveBuffer[i] = receiveBuffer[i + truncateLength];
	}
	receiveBufferSize -= truncateLength;
}

void ESP8266ClientReadBuffer::fillReceiveBuffer() 
{
	// fill the receive buffer as much as possible from esp8266.read()
	for (uint8_t attemps = 0; attemps < 5; attemps++) 
	{		
		// often 1st available() call does not yield all bytes => outer while
		while (uint8_t availableBytes = esp8266.available() > 0) 
		{
			for (; availableBytes > 0 && receiveBufferSize < ESP8266_CLIENT_MAX_BUFFER_SIZE; availableBytes--) 
			{
				receiveBuffer[receiveBufferSize++] = esp8266.read();
			}
			delay(10);
		}
		delay(10);
	}

	// strip the AT nonsense out of the read data - we only want the payload!
	this->cleanReceiveBufferFromAT();
}

void ESP8266ClientReadBuffer::cleanReceiveBufferFromAT() 
{
	// typical response from AT+CIPSEND looks like \r\n\r\n+IPD,0,4:<payload>
	// get rid of these esp8266 commands
	this->cleanReceiveBufferFromAT("\r\n\r\n+IPD", 5);

	// though ... it looks like there might be only \r\n when we get a 
	// continuation of the payload (i.e. it is split across multiple packets)
}

void ESP8266ClientReadBuffer::cleanReceiveBufferFromAT(const char *atCommand, uint8_t additionalSuffixToKill) 
{
	uint8_t atLen = strlen(atCommand);
	for (uint8_t offset = 0; offset <= receiveBufferSize - atLen; offset++) 
	{
		if (0 == memcmp((receiveBuffer + offset), atCommand, atLen)) 
		{
			// found the at command. KILL IT!
			this->truncateReceiveBufferHead(offset, atLen + additionalSuffixToKill);
			break;
		}
	}
}

