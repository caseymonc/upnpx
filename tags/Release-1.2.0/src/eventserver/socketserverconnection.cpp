// **********************************************************************************
//
// BSD License.
// This file is part of upnpx.
//
// Copyright (c) 2010-2011, Bruno Keymolen, email: bruno.keymolen@gmail.com
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, this 
// list of conditions and the following disclaimer in the documentation and/or other 
// materials provided with the distribution.
// Neither the name of "Bruno Keymolen" nor the names of its contributors may be 
// used to endorse or promote products derived from this software without specific 
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.
//
// **********************************************************************************

#include "socketserverconnection.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SERVER_BUFFER_STEP 2048

SocketServerConnection::SocketServerConnection(SOCKET socket, struct sockaddr_in *sender){
	mSocket = socket;
	isActive = true;
	mBuffer = (u8*)malloc(SERVER_BUFFER_STEP);
	mBufferSize = SERVER_BUFFER_STEP;
	memcpy(&mSender, sender, sizeof(struct sockaddr_in));
}



SocketServerConnection::~SocketServerConnection(){
	free(mBuffer);
}



SOCKET SocketServerConnection::GetSocket(){
	return mSocket;
}

u8* SocketServerConnection::GetBuffer(){
	return mBuffer;
}



int SocketServerConnection::ReadDataFromSocket(struct sockaddr_in **sender){
	int len = 0; 
	
	memset(sender, 0, sizeof(struct sockaddr_in));
	
	while(true){
		
		len = recv(mSocket, mBuffer, mBufferSize, 0);
//		len = recvfrom(mSocket, mBuffer, mBufferSize, 0, (struct sockaddr*)sender, &senderlen);		
		*sender = &mSender;
		
		if(len < 0 /* error */ || len == 0 /* closed */){
			return len;
		}
		if(len == mBufferSize){
			//there is more to read (?)
			printf("?????????????????\n");
			return -1;
		}else{
			break;
		}
	}
	
	return len;
}

int SocketServerConnection::SendDataOnSocket(unsigned char *sendbuf, int len){
	int sentlen = send(mSocket, sendbuf, len, 0);
	return sentlen;
}


int SocketServerConnection::ErrorOnSocket(){
	return -1;
}



