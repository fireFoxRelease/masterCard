
/**
 *
 *  The test program is used to test this working condition:
 *  Channel 0,1,2 send, and channel 0,1,2,3 receive by interrupt.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
//#include <unistd.h>
//#include <pthread.h>

#include "HiPCIe1394Public.h"

//RCV_PACKET_STRUCT rcv1;
int status;

FILE *in[4];
#define delayTime 1
#define testTime 1000
#define ErrorCheck(func) {status = func; if(status != HR_SUCCESS ){printf("Error:%08x\n",status);}}

int intNum2;
#define Delay(delaytime) usleep(delaytime*1000) /*unit:ms*/

#define MSG_TYPE_NUM 10

typedef struct msg_static
{
    char msgName[128];
    int msgType;
    unsigned int count;
}MSG_STATIC;

MSG_STATIC msgStatic[MSG_TYPE_NUM] =
{
{ "stof send count", STOF_SEND_CNT, 0 },
{ "stof recv count", STOF_RECV_CNT, 0 },
{ "asyn send count", ASYN_SEND_CNT, 0 },
{ "asyn recv count", ASYN_RECV_CNT, 0 },
{ "hcrc error count", RECV_HCRC_ERR_CNT, 0 },
{ "dcrc error count", RECV_DCRC_ERR_CNT, 0 },
{ "vpc error count", RECV_VPC_ERR_CNT, 0 },
{ "stof dcrc error count", STOF_DCRC_ERR_CNT, 0 },
{" stof vpc error count", STOF_DCRC_ERR_CNT, 0 },
{ "bus reset count", BUS_RST_CNT, 0 },
};

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


int main() 
{
     HR_DEVICE pDev;

    INT_PARA intPara;
    int i;

    hiMil1394OpenCard(&pDev, 0);
    if (pDev <= 0) {
        printf("open card error\n");
        return 0;
    }


    printf("open card success.\n");
	ErrorCheck(hiMil1394Reset(pDev));
	Sleep(1);

    int chn = 0;

    hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/

#if 1
    hiMil1394MsgSpeedSet(pDev, chn, 0);
    TY_STOF_PACKET sendPac;
    TY_ASYNC_PACKET sendAsynData;

    memset(&sendPac, 0, sizeof(TY_STOF_PACKET));
    memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

    hiMil1394NodeModeSet(pDev, chn, 0); //0:cc 1;RN BM:2
    hiMil1394STOFPeriodSet(pDev, chn, 10000);
    hiMil1394STOFSet(pDev, chn, 1, 100);

    sendPac.STOFPayload0 = 0x1122;
    sendPac.STOFPayload1 = 0x12345678;
    sendPac.STOFPayload5 = 0x8877;
    sendPac.STOFPayload8 = 0x3344;
    sendPac.STOFVPC = ~(sendPac.STOFPayload0^sendPac.STOFPayload1^sendPac.STOFPayload2^sendPac.STOFPayload3
    		^sendPac.STOFPayload4^sendPac.STOFPayload5^sendPac.STOFPayload6^sendPac.STOFPayload7^sendPac.STOFPayload8);
    //printf("vpc:%x \n", sendPac.STOFVPC);
    hiMil1394STOFWrite(pDev, chn, 0, &sendPac);

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
#endif

	getchar();
	hiMil1394StartStop(pDev, chn, 0);

    hiMil1394UnRegisterInterrupt(pDev);
    for (int i = 0; i < MSG_TYPE_NUM; i++)
    {
        hiMil1394GetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);
        printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
    }

	ErrorCheck(hiMil1394CloseCard(pDev));
    getchar();

    return 0;
}

