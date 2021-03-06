/**
ATESP8266Client.h

Arduino library for managing wifi connections using an ESP8266 in AT mode 
(using AT firmware v1.3.0).

This is heavily based on the Sparkfun ESP8266 shield library - with some 
updates for the latest AT firmware and support for SSL connections. This
is tested using the Sparkfun ESP8266 WiFi Shield, but should work with any
ESP8266 modules running the AT firmware.

This has been adapted to fit the Arduino Client.h api (as mentioned in this
issue: https://github.com/knolleary/pubsubclient/issues/107) and to strip
out the AT responses from the read command.

author: Alex Shenfield
date:   11/09/2020
*/

#include "ATESP8266WiFi.h"
#include <Arduino.h>
#include "util/ESP8266_AT.h"
#include "ATESP8266Client.h"

ESP8266Client::ESP8266Client()
{
}

ESP8266Client::ESP8266Client(uint8_t sock)
{
	_socket = sock;
}

uint8_t ESP8266Client::status()
{
	return esp8266.status();
}
	
int ESP8266Client::connect(IPAddress ip, uint16_t port)
{
	return connect(ip, port, 0);
}

int ESP8266Client::connect(const char *host, uint16_t port)
{
	return connect(host, port, 0);
}

int ESP8266Client::connect(String host, uint16_t port, uint32_t keepAlive)
{
	return connect(host.c_str(), port, keepAlive);
}
	
int ESP8266Client::connect(IPAddress ip, uint16_t port, uint32_t keepAlive) 
{
	char ipAddress[16];
	sprintf(ipAddress, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	
	return connect((const char *)ipAddress, port, keepAlive);
}
	
int ESP8266Client::connect(const char* host, uint16_t port, uint32_t keepAlive) 
{
	_socket = getFirstSocket();
	
    if (_socket != ESP8266_SOCK_NOT_AVAIL)
    {
		esp8266._state[_socket] = TAKEN;
		int16_t rsp = esp8266.tcpConnect(_socket, host, port, keepAlive);
		
		return rsp;
	}
}

size_t ESP8266Client::write(uint8_t c)
{
	return write(&c, 1);
}

size_t ESP8266Client::write(const uint8_t *buf, size_t size)
{
	return esp8266.tcpSend(_socket, buf, size);
}

int ESP8266Client::available()
{
	return receiveBuffer.available();
}

int ESP8266Client::read()
{
	return receiveBuffer.read();
}

int ESP8266Client::read(uint8_t *buf, size_t size)
{
	if (available() + esp8266.available() < size)
		return 0;
	
	for (int i=0; i<size; i++)
	{
		buf[i] = this->read();
	}
	
	return 1;
}

int ESP8266Client::peek()
{
	return esp8266.peek();
}

void ESP8266Client::flush()
{
	esp8266.flush();
}

void ESP8266Client::stop()
{
	esp8266.close(_socket);
	esp8266._state[_socket] = AVAILABLE;
}

uint8_t ESP8266Client::connected()
{
	// If data is available, assume we're connected. Otherwise,
	// we'll try to send the status query, and will probably end 
	// up timing out if data is still coming in.
	if (_socket == ESP8266_SOCK_NOT_AVAIL)
		return 0;
	else if (available() > 0)
		return 1;
	else if (esp8266._status.stat == ESP8266_STATUS_CONNECTED)
		return 1;
	
	return 0;
}

ESP8266Client::operator bool()
{
	return connected();
}

// Private Methods
uint8_t ESP8266Client::getFirstSocket()
{
	/*
	for (int i = 0; i < ESP8266_MAX_SOCK_NUM; i++) 
	{
		if (esp8266._state[i] == AVAILABLE)
		{
			return i;
		}
	}
	return ESP8266_SOCK_NOT_AVAIL;
	*/
	esp8266.updateStatus();
	for (int i = 0; i < ESP8266_MAX_SOCK_NUM; i++) 
	{
		if (esp8266._status.ipstatus[i].linkID == 255)
		{
			return i;
		}
	}
	return ESP8266_SOCK_NOT_AVAIL;
}
