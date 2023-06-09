/*
  serial2socket.cpp -  serial 2 socket functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "esp3dlibconfig.h"

#if defined(ESP3D_WIFISUPPORT)
#include "serial2socket.h"
#include "wificonfig.h"
#include <WebSocketsServer.h>
#include "tcp_server.h"
#include <WiFi.h>

Serial_2_Socket Serial2Socket;


Serial_2_Socket::Serial_2_Socket()
{
    _web_socket = NULL;
    _tcp_socket = NULL;
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
    xMutex = xSemaphoreCreateMutex();
}
Serial_2_Socket::~Serial_2_Socket()
{
    if (_web_socket) {
        detachWS();
    }
    if(_tcp_socket)
    {
        detachTS();
    }
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
    vSemaphoreDelete(xMutex);
}
void Serial_2_Socket::begin(long speed)
{
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}

void Serial_2_Socket::end()
{
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}

long Serial_2_Socket::baudRate()
{
    return 0;
}

bool Serial_2_Socket::attachWS(void * web_socket)
{
    if (web_socket) {
        _web_socket = web_socket;
        _TXbufferSize=0;
        return true;
    }
    return false;
}

bool Serial_2_Socket::detachWS()
{
    _web_socket = NULL;
    return true;
}


bool Serial_2_Socket::attachTS(void * tcp_socket)
{
    if (tcp_socket) {
        _tcp_socket = tcp_socket;
        _TXbufferSize=0;
        return true;
    }
    return false;
}
bool Serial_2_Socket::detachTS()
{
    _tcp_socket = NULL;
    return true;
}


Serial_2_Socket::operator bool() const
{
    return true;
}
int Serial_2_Socket::available()
{
    return _RXbufferSize;
}


size_t Serial_2_Socket::write(uint8_t c)
{
    if((!_web_socket) && (!_tcp_socket) ) {
        return 0;
    }
    write(&c,1);
    return 1;
}

size_t Serial_2_Socket::write(const uint8_t *buffer, size_t size)
{
    if((buffer == NULL) ||((!_web_socket) && (!_tcp_socket))) {
        if(buffer == NULL) {
            log_i("[SOCKET]No buffer");
        }
        if((!_web_socket) && (!_tcp_socket)) {
            log_i("[SOCKET]No socket");
        }

        return 0;
    }
#if defined(ENABLE_SERIAL2SOCKET_OUT)
    if (_TXbufferSize==0) {
        _lastflush = millis();
    }
    //send full line
    if (_TXbufferSize + size > TXBUFFERSIZE) {
        flush();
    }
    //need periodic check to force to flush in case of no end
    xSemaphoreTake(xMutex, portMAX_DELAY);
    uint8_t last_ch = 0;
    for (int i = 0; i < size; i++) {
        _TXbuffer[_TXbufferSize] = buffer[i];
        last_ch = buffer[i];
        _TXbufferSize++;
    }
    xSemaphoreGive(xMutex);
    log_i("[SOCKET]buffer size %d",_TXbufferSize);
    if(last_ch == '\n' || last_ch == 0)
        flush();
    handle_flush();
#endif
    return size;
}

int Serial_2_Socket::peek(void)
{
    if (_RXbufferSize > 0) {
        return _RXbuffer[_RXbufferpos];
    } else {
        return -1;
    }
}

bool Serial_2_Socket::push (const char * data)
{
#if defined(ENABLE_SERIAL2SOCKET_IN)
    xSemaphoreTake(xMutex, portMAX_DELAY);

    int data_size = strlen(data);
    if ((data_size + _RXbufferSize) <= RXBUFFERSIZE) {
        int current = _RXbufferpos + _RXbufferSize;
        if (current > RXBUFFERSIZE) {
            current = current - RXBUFFERSIZE;
        }
        for (int i = 0; i < data_size; i++) {
            if (current > (RXBUFFERSIZE-1)) {
                current = 0;
            }
            _RXbuffer[current] = data[i];
            current ++;
        }
        _RXbufferSize+=strlen(data);

        xSemaphoreGive(xMutex);
        return true;
    }
    log_n("push error");

    xSemaphoreGive(xMutex);
    return false;
#else
    return true;
#endif
}

int Serial_2_Socket::read(void)
{
    xSemaphoreTake(xMutex, portMAX_DELAY);

    if (_RXbufferSize > 0) {
        int v = _RXbuffer[_RXbufferpos];
        _RXbufferpos++;
        if (_RXbufferpos > (RXBUFFERSIZE-1)) {
            _RXbufferpos = 0;
        }
        _RXbufferSize--;
        //ets_printf("%c", v);

        xSemaphoreGive(xMutex);
        return v;
    } else {

        xSemaphoreGive(xMutex);
        return -1;
    }
}

void Serial_2_Socket::handle_flush()
{
    if (_TXbufferSize > 0) {
        if ((_TXbufferSize>=TXBUFFERSIZE) || ((millis()- _lastflush) > FLUSHTIMEOUT)) {
            log_i("[SOCKET]need flush, buffer size %d",_TXbufferSize);
            flush();
        }
    }
}
void Serial_2_Socket::flush(void)
{
    //xSemaphoreTake(xMutex, portMAX_DELAY);
    if (_TXbufferSize > 0) {
        log_i("[SOCKET]flush data, buffer size %d",_TXbufferSize);
        if(_web_socket)
            ((WebSocketsServer *)_web_socket)->broadcastBIN(_TXbuffer,_TXbufferSize);
        #ifdef TCP_SOCKET_FEATURE
        if(_tcp_socket)
            ((TCP_Server *)_tcp_socket)->tcp_print(_TXbuffer,_TXbufferSize);
        #endif
        //refresh timout
        _lastflush = millis();
        //reset buffer
        _TXbufferSize = 0;
    }
    //xSemaphoreGive(xMutex);
}

#endif // ESP3D_WIFISUPPORT
