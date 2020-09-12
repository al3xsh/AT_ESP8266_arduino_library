/**
ATESP8266WiFi.cpp

Arduino library for managing wifi connections using an ESP8266 in AT mode 
(using AT firmware v1.3.0).

This is heavily based on the Sparkfun ESP8266 shield library - with some 
updates for the latest AT firmware and support for SSL connections. This
is tested using the Sparkfun ESP8266 WiFi Shield, but should work with any
ESP8266 modules running the AT firmware.

To communicated with the ESP8266 using Arduino SoftwareSerial the ESP8266 
should run at 19200 bps max. You may be able to use higher baud rates 
(though you wont get reliable comms at 115200) but errors creep in (and
they are hard to debug!).

Unless you need faster, I recommend 9600 bps.

author: Alex Shenfield
date:   11/09/2020
*/

#include <Arduino.h>

#include "util/ESP8266_AT.h"
#include "ATESP8266WiFi.h"
#include "ATESP8266Client.h"

#define ESP8266_DISABLE_ECHO

////////////////////////
// Buffer Definitions //
////////////////////////

// set the buffer length
#define ESP8266_RX_BUFFER_LEN 128

// define the rx buffer
char esp8266RxBuffer[ESP8266_RX_BUFFER_LEN];
unsigned int bufferHead;

////////////////////
// Initialization //
////////////////////

// set up the sockets
ESP8266Class::ESP8266Class()
{
    for (int i = 0; i < ESP8266_MAX_SOCK_NUM; i++)
    {
        _state[i] = AVAILABLE;
    }
}

// set up the ESP8266
bool ESP8266Class::begin(unsigned long baudRate, esp8266_serial_port serialPort)
{
    // set up the serial port
    _baud = baudRate;
    if (serialPort == ESP8266_SOFTWARE_SERIAL)
    {
        swSerial.begin(baudRate);
        _serial = &swSerial;
    }
    else if (serialPort == ESP8266_HARDWARE_SERIAL)
    {
        Serial.begin(baudRate);
        _serial = &Serial;
    }

    // check communication is working
    if (test())
    {
        // enable multiple connections
        if (!setMux(true))
        {
            return false;
        }
        // disable at command echo
#ifdef ESP8266_DISABLE_ECHO
        if (!echo(false))
        {
            return false;
        }
#endif
        return true;
    }

    return false;
}

///////////////////////
// Basic AT Commands //
///////////////////////

// send the AT command to test communication
bool ESP8266Class::test()
{
    // send AT
    sendCommand(ESP8266_TEST);

    // check for the OK response
    if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
    {
        return true;
    }

    return false;
}

// reset the module
bool ESP8266Class::reset()
{
    // send AT+RST
    sendCommand(ESP8266_RESET);

    // check for the OK response
    if (readForResponse(RESPONSE_READY, COMMAND_RESET_TIMEOUT) > 0)
    {
        return true;
    }

    return false;
}

// enable / disable the AT command echo (useful for debugging, but complicates
// parsing the command responses)
bool ESP8266Class::echo(bool enable)
{
    // send either ATE0 (disable) or ATE1 (enable)
    if (enable)
    {
        sendCommand(ESP8266_ECHO_ENABLE);
    }
    else
    {
        sendCommand(ESP8266_ECHO_DISABLE);
    }

    // check for the OK response
    if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
    {
        return true;
    }

    return false;
}

// set the baud rate
bool ESP8266Class::setBaud(unsigned long baud)
{
    char parameters[16];
    memset(parameters, 0, 16);

    // constrain parameters
    baud = constrain(baud, 110, 115200);

    // put parameters into string
    sprintf(parameters, "%d,8,1,0,0", baud);

    // send AT+UART_DEF=baud,databits,stopbits,parity,flowcontrol
    sendCommand(ESP8266_UART, ESP8266_CMD_SETUP, parameters);

    // check for the OK response
    if (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0)
    {
        return true;
    }

    return false;
}

// get the at firmware version, sdk version, and compile time (note: you must pass 
// empty arrays big enough to hold this information!)
int16_t ESP8266Class::getVersion(char * ATversion, char * SDKversion, char * compileTime)
{
    // Send AT+GMR

	// Example Response: AT version:0.30.0.0(Jul  3 2015 19:35:49)\r\n (43 chars)
	//                   SDK version:1.2.0\r\n (19 chars)
	//                   compile time:Jul  7 2015 18:34:26\r\n (36 chars)
	//                   OK\r\n
	// (~101 characters)
    sendCommand(ESP8266_VERSION); 

    // check for OK response
    int16_t rsp = (readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT) > 0);
    if (rsp > 0)
    {
        char *p, *q;

        // look for "AT version" in the rxBuffer
        p = strstr(esp8266RxBuffer, "AT version:");
        if (p == NULL) return ESP8266_RSP_UNKNOWN;
        p += strlen("AT version:");
        q = strchr(p, '\r'); // Look for \r
        if (q == NULL) return ESP8266_RSP_UNKNOWN;
        strncpy(ATversion, p, q - p);

        // look for "SDK version:" in the rxBuffer
        p = strstr(esp8266RxBuffer, "SDK version:");
        if (p == NULL) return ESP8266_RSP_UNKNOWN;
        p += strlen("SDK version:");
        q = strchr(p, '\r'); // Look for \r
        if (q == NULL) return ESP8266_RSP_UNKNOWN;
        strncpy(SDKversion, p, q - p);

        // look for "compile time:" in the rxBuffer
        p = strstr(esp8266RxBuffer, "compile time:");
        if (p == NULL) return ESP8266_RSP_UNKNOWN;
        p += strlen("compile time:");
        q = strchr(p, '\r'); // Look for \r
        if (q == NULL) return ESP8266_RSP_UNKNOWN;
        strncpy(compileTime, p, q - p);
    }

    return rsp;
}

////////////////////
// WiFi Functions //
////////////////////

// getMode()
// Input: None
// Output:
//    - Success: 1, 2, 3 (ESP8266_MODE_STA, ESP8266_MODE_AP, ESP8266_MODE_STAAP)
//    - Fail: <0 (esp8266_cmd_rsp)
int16_t ESP8266Class::getMode()
{
    // sending AT+CWMODE_DEF?
    sendCommand(ESP8266_WIFI_MODE, ESP8266_CMD_QUERY);

	// Example response: +CWMODE_DEF:1\r\n
	//					 OK\r\n

    // check for OK response
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        // then get the number after ':'
        char * p = strchr(esp8266RxBuffer, ':');
        if (p != NULL)
        {
            char mode = *(p + 1);
            if ((mode >= '1') && (mode <= '3'))
            {
                // convert ascii to decimal
                return (mode - 48);
            }
        }

        return ESP8266_RSP_UNKNOWN;
    }

    return rsp;
}

// setMode()
// Input: 1, 2, 3 (ESP8266_MODE_STA, ESP8266_MODE_AP, ESP8266_MODE_STAAP)
// Output:
//    - Success: >0
//    - Fail: <0 (esp8266_cmd_rsp)
int16_t ESP8266Class::setMode(esp8266_wifi_mode mode)
{
    // format the parameter string and send
    char modeChar[2] = { 0, 0 };
    sprintf(modeChar, "%d", mode);
    sendCommand(ESP8266_WIFI_MODE, ESP8266_CMD_SETUP, modeChar);

    // return whether we got an OK response
    return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

// connect()
// Input: ssid
// Output:
//    - Success: >0
//    - Fail: <0 (esp8266_cmd_rsp)
int16_t ESP8266Class::connect(const char * ssid)
{
    connect(ssid, "");
}

// connect()
// Input: ssid and pwd const char's
// Output:
//    - Success: >0
//    - Fail: <0 (esp8266_cmd_rsp)
int16_t ESP8266Class::connect(const char * ssid, const char * pwd)
{
    // send connect command AT+CWJAP_DEF="ssid","pwd"
    _serial->print("AT");
    _serial->print(ESP8266_CONNECT_AP);
    _serial->print("=\"");
    _serial->print(ssid);
    _serial->print("\"");
    if (pwd != NULL)
    {
        _serial->print(',');
        _serial->print("\"");
        _serial->print(pwd);
        _serial->print("\"");
    }
    _serial->print("\r\n");

    // check for ok response
    return readForResponses(RESPONSE_OK, RESPONSE_FAIL, WIFI_CONNECT_TIMEOUT);
}

// get access point information
int16_t ESP8266Class::getAP(char * ssid)
{
    // send "AT+CWJAP_DEF?"
    sendCommand(ESP8266_CONNECT_AP, ESP8266_CMD_QUERY); 

    // example responses: No AP\r\n\r\nOK\r\n
    // - or -
    // +CWJAP:"WiFiSSID","00:aa:bb:cc:dd:ee",6,-45\r\n\r\nOK\r\n

    // check for ok response
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        // look for "No AP"
        if (strstr(esp8266RxBuffer, "No AP") != NULL)
        {
            return 0;
        }

        // look for "+CWJAP"
        char * p = strstr(esp8266RxBuffer, ESP8266_CONNECT_AP);
        if (p != NULL)
        {
            p += strlen(ESP8266_CONNECT_AP) + 2;
            char * q = strchr(p, '"');
            if (q == NULL) return ESP8266_RSP_UNKNOWN;
            strncpy(ssid, p, q - p);
            return 1;
        }
    }

    return rsp;
}

// disconnect from access point
int16_t ESP8266Class::disconnect()
{
    // send AT+CWQAP
    sendCommand(ESP8266_DISCONNECT); 
    
    // Example response: \r\n\r\nOK\r\nWIFI DISCONNECT\r\n
    // "WIFI DISCONNECT" comes up to 500ms _after_ OK. 
 
    // check for ok response 
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        // check for disconnect message
        rsp = readForResponse("WIFI DISCONNECT", COMMAND_RESPONSE_TIMEOUT);
        if (rsp > 0)
        {
            return rsp;
        }
        return 1;
    }

    return rsp;
}

// status()
// Input: none
// Output:
//    - Success: 2, 3, 4, or 5 (ESP8266_STATUS_GOTIP, ESP8266_STATUS_CONNECTED, ESP8266_STATUS_DISCONNECTED, ESP8266_STATUS_NOWIFI)
//    - Fail: <0 (esp8266_cmd_rsp)
int16_t ESP8266Class::status()
{
    // check the status ...
    int16_t statusRet = updateStatus();
    if (statusRet > 0)
    {
        switch (_status.stat)
        {
        case ESP8266_STATUS_GOTIP: // 2
        case ESP8266_STATUS_DISCONNECTED: // 4
        case ESP8266_STATUS_CONNECTED:    // 3
            return 1;
            break;
        case ESP8266_STATUS_NOWIFI: // 5
            return 0;
            break;
        }
    }
    return statusRet;
}

// actually check the esp8266 status
int16_t ESP8266Class::updateStatus()
{
    // Send AT+CIPSTATUS
    sendCommand(ESP8266_TCP_STATUS); 
                                     
    // Example Response:
    // STATUS:<stat>+CIPSTATUS:<link ID>, <type>, <remote_IP>, <remote_port>, <local_port>,<tetype>
    //
    // <stat> status of ESP8266 station interface
    // Parameters:
    // 2 : ESP8266 station connected to an AP and has obtained IP
    // 3 : ESP8266 station created a TCP or UDP transmission
    // 4 : the TCP or UDP transmission of ESP8266 station disconnected
    // 5 : ESP8266 station did NOT connect to an AP
    // <link ID> ID of the connection (0~4), for multi-connect
    // <type> string, "TCP" or "UDP"
    // <remote_IP> string, remote IP address
    // <remote_port> remote port number
    // <local_port> ESP8266 local port number
    // <tetype>
    // 0 : ESP8266 runs as client
    // 1 : ESP8266 runs as server
    
    // check for OK response
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        char * p = searchBuffer("STATUS:");
        if (p == NULL)
            return ESP8266_RSP_UNKNOWN;

        p += strlen("STATUS:");
        _status.stat = (esp8266_connect_status)(*p - 48);

        for (int i = 0; i<ESP8266_MAX_SOCK_NUM; i++)
        {
            p = strstr(p, "+CIPSTATUS:");
            if (p == NULL)
            {
                // didn't find CIPSTATUS. Set linkID to 255.
                for (int j = i; j<ESP8266_MAX_SOCK_NUM; j++)
                    _status.ipstatus[j].linkID = 255;
                return rsp;
            }
            else
            {
                p += strlen("+CIPSTATUS:");

                // find linkID
                uint8_t linkId = *p - 48;
                if (linkId >= ESP8266_MAX_SOCK_NUM)
                    return rsp;
                _status.ipstatus[linkId].linkID = linkId;

                // find type (udp or tcp) - move the pointer p forward 3
                p += 3;
                if (*p == 'T')
                {
                    _status.ipstatus[linkId].type = ESP8266_TCP;
                }
                else if (*p == 'U')
                {
                    _status.ipstatus[linkId].type = ESP8266_UDP;
                }
                else
                {
                    _status.ipstatus[linkId].type = ESP8266_TYPE_UNDEFINED;
                }

                // find remoteIP - move the pointer p
                p += 6;
                for (uint8_t j = 0; j < 4; j++)
                {
                    char tempOctet[4];
                    memset(tempOctet, 0, 4);

                    size_t octetLength = strspn(p, "0123456789");
                    strncpy(tempOctet, p, octetLength);
                    _status.ipstatus[linkId].remoteIP[j] = atoi(tempOctet);

                    p += (octetLength + 1);
                }

                // find port
                p += 1;
                char tempPort[6];
                memset(tempPort, 0, 6);
                size_t portLen = strspn(p, "0123456789");
                strncpy(tempPort, p, portLen);
                _status.ipstatus[linkId].port = atoi(tempPort);
                p += portLen + 1;

                // find tetype
                if (*p == '0')
                {
                    _status.ipstatus[linkId].tetype = ESP8266_CLIENT;
                }
                else if (*p == '1')
                {
                    _status.ipstatus[linkId].tetype = ESP8266_SERVER;
                }
            }
        }
    }

    return rsp;
}

// localIP()
// Input: none
// Output:
//    - Success: Device's local IPAddress
//    - Fail: 0
IPAddress ESP8266Class::localIP()
{
    // send AT+CIFSR
    sendCommand(ESP8266_GET_LOCAL_IP); 

    // Example Response: +CIFSR:STAIP,"192.168.0.114"\r\n
    //                   +CIFSR:STAMAC,"18:fe:34:9d:b7:d9"\r\n
    //                   \r\n
    //                   OK\r\n

    // check for OK response
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        // look for "STAIP" in the rxBuffer
        char * p = strstr(esp8266RxBuffer, "STAIP");
        if (p != NULL)
        {
            IPAddress returnIP;

            // move the pointer to the correct position and read the ip address 
            // octet
            p += 7;
            for (uint8_t i = 0; i < 4; i++)
            {
                char tempOctet[4];
                memset(tempOctet, 0, 4);
                size_t octetLength = strspn(p, "0123456789");
                if (octetLength >= 4)
                {
                    return ESP8266_RSP_UNKNOWN;
                }
                strncpy(tempOctet, p, octetLength);
                returnIP[i] = atoi(tempOctet);
                p += (octetLength + 1);
            }

            return returnIP;
        }
    }

    return rsp;
}

// localMAC()
// Input: MAC address character buffer
// Output:
//    - Success: 1
//    - Fail: 0
int16_t ESP8266Class::localMAC(char * mac)
{
    // send AT+CIPSTAMAC?
    sendCommand(ESP8266_GET_STA_MAC, ESP8266_CMD_QUERY);

    // check for OK response
    int16_t rsp = readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
    if (rsp > 0)
    {
        // look for "+CIPSTAMAC" in the response
        char * p = strstr(esp8266RxBuffer, ESP8266_GET_STA_MAC);
        if (p != NULL)
        {
            p += strlen(ESP8266_GET_STA_MAC) + 2;
            char * q = strchr(p, '"');
            if (q == NULL) return ESP8266_RSP_UNKNOWN;
            strncpy(mac, p, q - p);
            return 1;
        }
    }

    return rsp;
}

/////////////////////
// TCP/IP Commands //
/////////////////////

// establish a tcp connection
int16_t ESP8266Class::tcpConnect(uint8_t linkID, const char * destination, uint16_t port, uint16_t keepAlive)
{
    // send the AT+CIPSTART=0,"TCP","url",80
    print("AT");
    print(ESP8266_TCP_CONNECT);
    print('=');
    print(linkID);
    print(',');
    print("\"TCP\",");
    print("\"");
    print(destination);
    print("\",");
    print(port);
    if (keepAlive > 0)
    {
        print(',');
        // keepAlive is in units of 500 milliseconds.
        // Max is 7200 * 500 = 3600000 ms = 60 minutes.
        print(keepAlive / 500);
    }
    print("\r\n");
    // Example good: CONNECT\r\n\r\nOK\r\n
    // Example bad:  DNS Fail\r\n\r\nERROR\r\n
    // Example meh:  ALREADY CONNECTED\r\n\r\nERROR\r\n
    int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, CLIENT_CONNECT_TIMEOUT);

    if (rsp < 0)
    {
        // we may see "ERROR", but be "ALREADY CONNECTED".
        // search for "ALREADY", and return success if we see it.
        char * p = searchBuffer("ALREADY");
        if (p != NULL)
        {
            return 2;
        }
        // otherwise the connection failed. Return the error code:
        return rsp;
    }
    // return 1 on successful (new) connection
    return 1;
}

// send tcp data
int16_t ESP8266Class::tcpSend(uint8_t linkID, const uint8_t *buf, size_t size)
{
    // check whether we are trying to send more data than we can manage
    if (size > 2048)
    {
        return ESP8266_CMD_BAD;
    }

    // copy the information about the data into the paramter string and send
    // AT+CIPSEND=0,52
    char params[8];
    sprintf(params, "%d,%d", linkID, size);
    sendCommand(ESP8266_TCP_SEND, ESP8266_CMD_SETUP, params);

    // check for the Ok response
    int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
    if (rsp != ESP8266_RSP_FAIL)
    {
        // send all the data
        // E.g.
        // GET / HTTP/1.1
        // Host: example.com
        // Connection: close
        for (size_t i = 0; i < size; i++)
        {
            write(buf[i]);
        }

        // check we have sent the data ok
        rsp = readForResponse("SEND OK", COMMAND_RESPONSE_TIMEOUT);
    
        // return the size of the data sent
        if (rsp > 0) {
            return size;
        }
    }
    return rsp;
}

// close the connection
int16_t ESP8266Class::close(uint8_t linkID)
{
    // send AT+CIPCLOSE=0
    char params[2];
    sprintf(params, "%d", linkID);
    sendCommand(ESP8266_TCP_CLOSE, ESP8266_CMD_SETUP, params);

    // Eh, client virtual function doesn't have a return value.
    // We'll wait for the OK or timeout anyway.
    return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

int16_t ESP8266Class::setTransferMode(uint8_t mode)
{
    char params[2] = { 0, 0 };
    params[0] = (mode > 0) ? '1' : '0';
    sendCommand(ESP8266_TRANSMISSION_MODE, ESP8266_CMD_SETUP, params);

    return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

// enable / disable multiple connections
int16_t ESP8266Class::setMux(bool enable)
{
    char params[2] = { 0, 0 };
    params[0] = enable ? '1' : '0';
    sendCommand(ESP8266_TCP_MULTIPLE, ESP8266_CMD_SETUP, params);
    return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

// set up a tcp server (need to first set AT+CIPMUX=1)
int16_t ESP8266Class::configureTCPServer(uint16_t port, uint8_t create)
{
    char params[10];
    if (create > 1) create = 1;
    sprintf(params, "%d,%d", create, port);
    sendCommand(ESP8266_SERVER_CONFIG, ESP8266_CMD_SETUP, params);
    return readForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

// send a ping request to a given IP address
int16_t ESP8266Class::ping(IPAddress ip)
{
    char ipStr[17];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return ping(ipStr);
}

// send the ping request
int16_t ESP8266Class::ping(char * server)
{
    // format the parameter string
    char params[strlen(server) + 3];
    sprintf(params, "\"%s\"", server);
    
    // send AT+Ping=<server>
    sendCommand(ESP8266_PING, ESP8266_CMD_SETUP, params);

    // Example responses:
    //  * Good response: +12\r\n\r\nOK\r\n
    //  * Timeout response: +timeout\r\n\r\nERROR\r\n
    //  * Error response (unreachable): ERROR\r\n\r\n
    int16_t rsp = readForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_PING_TIMEOUT);
    
    // parse the ping response time
    if (rsp > 0)
    {
        char * p = searchBuffer("+");
        p += 1;
        char * q = strchr(p, '\r');
        if (q == NULL)
        {
            return ESP8266_RSP_UNKNOWN;
        }
        char tempRsp[10] = {0};
        strncpy(tempRsp, p, q - p);
        return atoi(tempRsp);
    }
    else
    {
        if (searchBuffer("timeout") != NULL)
        {
            return 0;
        }
    }

    return rsp;
}

//////////////////////////////
// Stream Virtual Functions //
//////////////////////////////

size_t ESP8266Class::write(uint8_t c)
{
    _serial->write(c);
}

int ESP8266Class::available()
{
    return _serial->available();
}

int ESP8266Class::read()
{
    return _serial->read();
}

int ESP8266Class::peek()
{
    return _serial->peek();
}

void ESP8266Class::flush()
{
    _serial->flush();
}

//////////////////////////////////////////////////
// Private, Low-Level, Ugly, Hardware Functions //
//////////////////////////////////////////////////

void ESP8266Class::sendCommand(const char * cmd, enum esp8266_command_type type, const char * params)
{
    _serial->print("AT");
    _serial->print(cmd);
    if (type == ESP8266_CMD_QUERY)
        _serial->print('?');
    else if (type == ESP8266_CMD_SETUP)
    {
        _serial->print("=");
        _serial->print(params);
    }
    _serial->print("\r\n");
}

// check the data received from the esp8266 for a specific response
int16_t ESP8266Class::readForResponse(const char * rsp, unsigned int timeout)
{
    // timestamp coming into function (so we can keep track of timeouts)
    unsigned long timeIn = millis();    
    unsigned int received = 0;

    clearBuffer();
    while (timeIn + timeout > millis())
    {
        // if data is available on UART RX
        if (_serial->available()) 
        {
            // read it into the buffer and search the buffer
            // for the queried response string
            received += readByteToBuffer();
            if (searchBuffer(rsp))
            {
                return received;
            }
        }
    }

    // otherwise, we've received some data but don't understand it
    if (received > 0)
    {
        return ESP8266_RSP_UNKNOWN;
    }
    else 
    {
        return ESP8266_RSP_TIMEOUT;
    }
}

// check the data received from the esp8266 for specific responses indicating pass or fail
int16_t ESP8266Class::readForResponses(const char * pass, const char * fail, unsigned int timeout)
{
    // timestamp coming into function (so we can keep track of timeouts)
    unsigned long timeIn = millis();    
    unsigned int received = 0;

    clearBuffer();
    while (timeIn + timeout > millis())
    {
        // if data is available on UART RX
        if (_serial->available()) 
        {
            // read it into the buffer and search the buffer
            // for the queried response string
            received += readByteToBuffer();
            if (searchBuffer(pass))
            {
                return received;
            }
            if (searchBuffer(fail))
            {
                return ESP8266_RSP_FAIL;
            }
        }
    }

    // otherwise, we've received some data but don't understand it
    if (received > 0)
    {
        return ESP8266_RSP_UNKNOWN;
    }
    else 
    {
        return ESP8266_RSP_TIMEOUT;
    }
}

//////////////////
// Buffer Stuff //
//////////////////
void ESP8266Class::clearBuffer()
{
    memset(esp8266RxBuffer, '\0', ESP8266_RX_BUFFER_LEN);
    bufferHead = 0;
}

unsigned int ESP8266Class::readByteToBuffer()
{
    // read the data in
    char c = _serial->read();

    // store the data in the buffer
    esp8266RxBuffer[bufferHead] = c;
    
    //! TODO: Don't care if we overflow. Should we? Set a flag or something?
    bufferHead = (bufferHead + 1) % ESP8266_RX_BUFFER_LEN;

    return 1;
}

char * ESP8266Class::searchBuffer(const char * test)
{
    int bufferLen = strlen((const char *)esp8266RxBuffer);
    
    // if our buffer isn't full, just do an strstr
    if (bufferLen < ESP8266_RX_BUFFER_LEN)
    {
        return strstr((const char *)esp8266RxBuffer, test);
    }
    else
    {    
        //! TODO
        // If the buffer is full, we need to search from the end of the 
        // buffer back to the beginning.
        int testLen = strlen(test);
        for (int i = 0; i<ESP8266_RX_BUFFER_LEN; i++)
        {

        }
    }
}

ESP8266Class esp8266;
