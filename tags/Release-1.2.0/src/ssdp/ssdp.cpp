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


#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "ssdp.h"
#include "tools.h"




SSDP::SSDP():mSocket(INVALID_SOCKET), mReadLoop(0), mTTL(2){
	mDB = new SSDPDB();
	mDB->Start();
	mParser = new SSDPParser(mDB);
}


SSDP::~SSDP(){
	Stop();
	delete(mParser);
	mParser = NULL;
	mDB->Stop();
	delete(mDB);
	mDB = NULL;
}


int SSDP::Start(){
	int ret = 0;
	u32 optval = 0;
	
	if(mSocket != INVALID_SOCKET){
		ret = -9;
		goto EXIT;
	}
	
	//Setup socket
	mSocket = socket(PF_INET, SOCK_DGRAM, 0);
	STATNVAL(mSocket, INVALID_SOCKET, EXIT);
	
	//Set nonblocking
	optval = fcntl( mSocket, F_GETFL, 0 );
	STATNVAL(optval, -1,  CLEAN_AND_EXIT);
	ret = fcntl(mSocket, F_SETFL, optval | O_NONBLOCK);
	STATNVAL(ret, -1,  CLEAN_AND_EXIT);
	
	//Source address
	mSrcaddr.sin_family = PF_INET;
	mSrcaddr.sin_port = htons(SSDP_MCAST_PORT); //can be another (?)
	mSrcaddr.sin_addr.s_addr = INADDR_ANY; //Default multicast nic 

	//Reuse port
	optval = 1;
	ret = setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, 4);
	STATVAL(ret, 0, CLEAN_AND_EXIT);
	optval = 1;
	ret = setsockopt(mSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&optval, 4);
	STATVAL(ret, 0, CLEAN_AND_EXIT);
	

	//Disable loopback
	optval = 0;
	ret = setsockopt(mSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&optval, sizeof(int));
	STATNVAL(ret, SOCKET_ERROR, CLEAN_AND_EXIT);	
	
	//TTL
	optval = mTTL;
	ret = setsockopt(mSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&optval, sizeof(int));
	STATNVAL(ret, SOCKET_ERROR, CLEAN_AND_EXIT);	
	
	
	//Add membership
	mMreq.imr_multiaddr.s_addr = inet_addr(SSDP_MCAST_ADDRESS);
	mMreq.imr_interface.s_addr = INADDR_ANY;
	ret = setsockopt(mSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mMreq, sizeof(struct ip_mreq));
	STATNVAL(ret, SOCKET_ERROR, CLEAN_AND_EXIT);	
	
	
	//Bind to all interface(s)
	ret = bind(mSocket, (struct sockaddr*)&mSrcaddr, sizeof(struct sockaddr));
	if(ret < 0 && (errno == EACCES || errno == EADDRINUSE)){
		printf("address in use\n");
	}
	STATVAL(ret, 0, CLEAN_AND_EXIT);
	
	
	//Destination address
	mDstaddr.sin_family = PF_INET;
	mDstaddr.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDRESS);
	mDstaddr.sin_port = htons(SSDP_MCAST_PORT);
	

	//Start the read thread
	pthread_attr_t  attr;
	ret = pthread_attr_init(&attr);
	if(ret != 0){
		goto CLEAN_AND_EXIT;
	}
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(ret != 0){
		pthread_attr_destroy(&attr);
		goto CLEAN_AND_EXIT;
	}
	
	ret = pthread_create(&mReadThread, &attr, SSDP::sReadLoop, (void*)this);
	
	pthread_attr_destroy(&attr);
	
	
	goto EXIT;
	
	
CLEAN_AND_EXIT:	
	close(mSocket);
	mSocket = INVALID_SOCKET;

EXIT:	
	printf("SSDP::Start %d\n", ret);
	return ret;
}


int SSDP::Stop(){
	mReadLoop = 0;
	
	if(mSocket > 0){
		close(mSocket);
		mSocket = INVALID_SOCKET;
	}
	

	return 0;
}



//Multicast M-Search
int SSDP::Search(){
	u32 seconds = 3;
	char target[]="ssdp:all";
	char os[]="OSX/10.6";
	char product[]="Mediacloud/0.1";
	
	char buf[20048]; 
	
	sprintf(buf, "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: %d\r\nST: %s\r\nUSER-AGENT: %s UPnP/1.1 %s\r\n\r\n", seconds, target,os, product);
	
	if(mSocket != INVALID_SOCKET){
		sendto(mSocket, buf, strlen(buf), 0, (struct sockaddr*)&mDstaddr , sizeof(struct sockaddr));
	}else{
		printf("invalid socket\n");
	}
	return 0;
}


int SSDP::AddObserver(SSDPObserver* observer){
	RemoveObserver(observer);
	mObservers.push_back(observer);
	return 0;
}


int SSDP::RemoveObserver(SSDPObserver* observer){
	u8 found = 0;
	int tel = 0;
	std::vector<SSDPObserver*>::iterator it;
	for(it=mObservers.begin(); it<mObservers.end(); it++){
		if(observer == *it){
			//Remove this one and stop
			found = 1;
			break;
		}
		tel++;
	}
	if(found){
		mObservers.erase(mObservers.begin()+tel);
	}
	return 0;
}



int SSDP::ReadLoop(){
	int ret = 0;
	mReadLoop = 1;
	
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	
	u8 buf[4096];
	int bufsize = 4096;
	
	struct sockaddr_in sender;
	socklen_t senderlen = sizeof(struct sockaddr);
	
	//Read UDP answers
	while(mReadLoop){
		//printf("SSDP::ReadLoop, enter 'select'\n");
		
		//(Re)set file descriptor
		FD_ZERO(&mReadFDS);
		FD_ZERO(&mExceptionFDS);
		FD_SET(mSocket, &mReadFDS);
		FD_SET(mSocket, &mExceptionFDS);
		
		ret = select(mSocket+1, &mReadFDS, NULL, &mExceptionFDS, &timeout);
		if(ret == SOCKET_ERROR){
			printf("OOPS!");
			break;
		}else if(ret != 0){
			//Exceptions ?
			if(FD_ISSET(mSocket, &mExceptionFDS)){
				printf("Error on socket, continue\n");
			}else if(FD_ISSET(mSocket, &mReadFDS)){
				//Data
				//printf("Data\n");
				ret = recvfrom(mSocket, buf, bufsize, 0, (struct sockaddr*)&sender, &senderlen);
				if(ret != SOCKET_ERROR){
					//Be sure to only deliver full messages (!)
					IncommingMessage((struct sockaddr*)&sender, buf, ret);
				}
			}
			
		}
	}
EXIT:
	return ret;
}



int SSDP::IncommingMessage(struct sockaddr* sender, u8* buf, u32 len){
	u8 *address;
	u16 port;
	
	address = (u8*)sender->sa_data+2;
	memcpy(&port, sender->sa_data, 2);
	port = ntohs(port);

//	printf("receive from: %d.%d.%d.%d, port %d\n", address[0], address[1], address[2], address[3] , port);
	
	mParser->ReInit();
    mParser->Parse(sender, buf, len);
	
	//We have the type and all headers, check the mandatory headers for Advertisement & Search
	
	//Inform the observer(s)
	std::vector<SSDPObserver*>::const_iterator it;
	for(it=mObservers.begin(); it<mObservers.end(); it++){
		((SSDPObserver*)*it)->SSDPMessage(mParser);
	}
	
	//hexdump(buf, len);
	return 0;
}


SSDPDB* SSDP::GetDB(){
	return mDB;
}



/** 
 * Static
 */

void* SSDP::sReadLoop(void* data){
	
	SSDP* pthis = (SSDP*)data;
	pthis->ReadLoop();

	return NULL;
}


