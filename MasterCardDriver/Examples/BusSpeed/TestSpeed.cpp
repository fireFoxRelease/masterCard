/**
 *
 *  The test program is used to test this working condition:
 *  Channel 0 worked as CC, send STOF and Asynchronous stream package.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "HiPCIe1394Public.h"

int status;
#define CHN_NO 0
#define BUS_SPEED 1

FILE *in[3];
#define delayTime 1
#define ErrorCheck(func) {status = func; if(status != HR_SUCCESS ){printf("Error:%08x\n",status);}}

int intNum2;
#define Delay(delaytime) usleep(delaytime*1000) /*unit:ms*/

unsigned int intSwap(unsigned int data)
{
    unsigned int value = 0, tmp = 0;

    unsigned int ii,jj;
    for(ii = 0x80000000,jj =0;ii!=0;ii=ii>>1,jj++)
    {
    	if(data & ii)
    	{
    		tmp = 1;
    	}
    	else
    	{
            tmp = 0;
    	}
    	value|= tmp<<jj;
    	//printf("ii:%x \n", ii);
    }

    return value;
}

int totalCount = 0;
void intFunc(INT_PARA intPara) 
{
	intNum2++;
    HR_DEVICE pDev = intPara.pDev;
	//printf("receiver interrupt number:%d \n", intNum2);
	HR_UINT chn = intPara.chn;
	HR_UINT counter = 0;
	char* pRecvData = NULL;
	TY_RECV_PACKET* pRecvPack = NULL;
	//HR_UINT & leng;

	pRecvData = (char*) malloc(sizeof(TY_RECV_PACKET) * 1024);
	pRecvPack = (TY_RECV_PACKET*) pRecvData;

	hiMil1394GetFrameNum(pDev, chn, &counter);

	for(int r = 0; r < counter; r++)
	{
		//hiMil1394Read(pDev, chn, pRecvPack, leng);
	}

	if(pRecvData)
	{
	    free(pRecvData);
	}
}

HR_DEVICE pDev;

int main() 
{
	INT_PARA intPara;
    int i;
	int chn = CHN_NO;
    in[0] = fopen("node0.txt", "w+");
    in[1] = fopen("node1.txt", "w+");
    in[2] = fopen("node2.txt", "w+");

    hiMil1394OpenCard(&pDev, 0);

    if (pDev <= 0)
    {
        printf("open card error\n");
        return 0;
    }

    printf("open card success.\n");
	ErrorCheck(hiMil1394Reset(pDev));
	sleep(2);

    hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/

    hiMil1394MsgSpeedSet(pDev, chn, BUS_SPEED);
    TY_ASYNC_PACKET sendAsynData;

    memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

    hiMil1394NodeModeSet(pDev, chn, 1); //0:cc 1;RN BM:2

    sendAsynData.msgId = 5;
    sendAsynData.channel = 2;
    sendAsynData.nodeId = 1;
    sendAsynData.payloadLength = 12;
    sendAsynData.msgData[0] = 0x1122;
    sendAsynData.STOFTransmitOffset = 100;
    sendAsynData.STOFReceiveOffset = 100;
    sendAsynData.STOFPHMOffset = 20;
    sendAsynData.heartBeatEnable = 0;
    sendAsynData.heartBeatStep = 0x0101;
    sendAsynData.heartBeatWord = 100;
    sendAsynData.VPCASYNCValue = ~(sendAsynData.msgId^sendAsynData.healthStatusWord^sendAsynData.heartBeatWord^sendAsynData.msgData[0]);
    hiMil1394AsyncWrite(pDev, chn, 0, &sendAsynData); //0 time 1:back to back

    memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
    sendAsynData.msgId = 6;
	sendAsynData.channel = 2;
	sendAsynData.nodeId = 1;
	sendAsynData.payloadLength = 12;
	sendAsynData.msgData[0] = 0x3344;
	sendAsynData.STOFTransmitOffset = 200;
	sendAsynData.STOFReceiveOffset = 200;
	sendAsynData.STOFPHMOffset = 20;
	sendAsynData.heartBeatEnable = 1;
    sendAsynData.heartBeatStep = 0x0202;
    sendAsynData.heartBeatWord = 200;
	hiMil1394AsyncWrite(pDev, chn, 0, &sendAsynData);

	hiMil1394StartStop(pDev, chn, 1);

	getchar();
	hiMil1394StartStop(pDev, chn, 0);

    hiMil1394UnRegisterInterrupt(pDev);
	ErrorCheck(hiMil1394CloseCard(pDev));

    fclose(in[0]);
    fclose(in[1]);
    fclose(in[2]);

    return 0;
}

