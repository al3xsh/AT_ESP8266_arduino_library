/**
ESP8266_AT.h

ESP8266 AT Command Definitions for AT firmware v1.3.0.

author: Alex Shenfield
date:   11/09/2020
*/

// Common AT Responses 
const char RESPONSE_OK[] = "OK\r\n";
const char RESPONSE_ERROR[] = "ERROR\r\n";
const char RESPONSE_FAIL[] = "FAIL";
const char RESPONSE_READY[] = "READY!";

// Basic AT Commands 
const char ESP8266_TEST[] = "";	
const char ESP8266_RESET[] = "+RST"; 
const char ESP8266_VERSION[] = "+GMR"; 
const char ESP8266_ECHO_ENABLE[] = "E1";
const char ESP8266_ECHO_DISABLE[] = "E0";
const char ESP8266_UART[] = "+UART_DEF";

// note: the uart_def command writes changes to flash so they are saved between power
// offs

// WiFi Functions 
const char ESP8266_WIFI_MODE[] = "+CWMODE_DEF";
const char ESP8266_CONNECT_AP[] = "+CWJAP_DEF";
const char ESP8266_LIST_AP[] = "+CWLAP";
const char ESP8266_DISCONNECT[] = "+CWQAP";
const char ESP8266_DHCP[] = "+CWDHCP_DEF";
const char ESP8266_SET_STA_MAC[] = "+CIPSTAMAC_DEF"; // Set MAC address of station
const char ESP8266_GET_STA_MAC[] = "+CIPSTAMAC_DEF"; // Get MAC address of station

// TCP/IP Commands
const char ESP8266_TCP_STATUS[] = "+CIPSTATUS"; // Get connection status
const char ESP8266_TCP_CONNECT[] = "+CIPSTART"; // Establish TCP connection or register UDP port
const char ESP8266_TCP_SEND[] = "+CIPSEND"; // Send Data
const char ESP8266_TCP_CLOSE[] = "+CIPCLOSE"; // Close TCP/UDP connection
const char ESP8266_GET_LOCAL_IP[] = "+CIFSR"; // Get local IP address
const char ESP8266_TCP_MULTIPLE[] = "+CIPMUX"; // Set multiple connections mode
const char ESP8266_SERVER_CONFIG[] = "+CIPSERVER"; // Configure as server
const char ESP8266_TRANSMISSION_MODE[] = "+CIPMODE"; // Set transmission mode
const char ESP8266_PING[] = "+PING"; // Function PING

// Extra TCP/IP Commands for SSL
const char ESP8266_TCP_SSL[] = "+CIPSSLSIZE"; // Set the size of the SSL buffer

