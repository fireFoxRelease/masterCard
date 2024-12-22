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
#define CHN_NO 1
#define ASYN_SEND_MODE 0 /*0 send by offset, 1:send back to back*/
#define PAYLOAD_LEN 2000

#define SEND_SPEED 2

FILE *in[3];
#define delayTime 1
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
	//printf("receiver interrupt number:%d \n", intNum2);
	HR_UINT counter = 0;
	int chn = intPara.chn;
	char* pRecvData = NULL;
	TY_RECV_PACKET* pRecvPack = NULL;
	int readCount = 1;
	int &countTmp = readCount;

	pRecvData = (char*) malloc(sizeof(TY_RECV_PACKET) * 1024);
	pRecvPack = (TY_RECV_PACKET*) pRecvData;

	hiMil1394GetFrameNum(pDev, chn, &counter);
	totalCount+= counter;
	for (int r = 0; r < counter; r++)
    {
        hiMil1394Read(pDev, chn, pRecvPack, countTmp);
        if (pRecvPack->msgType == 0)
        {
            fprintf(
                    in[chn],
                    "sec:%d usec:%d payload:%x %x %x %x %x %x %x %x %x VPC:%x\n",
                    pRecvPack->sec, pRecvPack->usec, pRecvPack->STOFPayload0,
                    pRecvPack->STOFPayload1, pRecvPack->STOFPayload2,
                    pRecvPack->STOFPayload3, pRecvPack->STOFPayload4,
                    pRecvPack->STOFPayload5, pRecvPack->STOFPayload6,
                    pRecvPack->STOFPayload7, pRecvPack->STOFPayload8,
                    pRecvPack->STOFVPC);

        }
        else if (pRecvPack->msgType == 1)
        {
            fprintf(
                    in[chn],
                    "sec:%d usec:%d RTC:%d msgId:%d nodeId:%d channel:%d sendoffset:%d "
                        "recvoffset:%d heartbeat:%x statusword:%x payloadlen:%d payload:%x %x %x %x %x %x %x %x"
                        " VPC:%x\n", pRecvPack->sec, pRecvPack->usec,
                    pRecvPack->RTC, pRecvPack->msgId, pRecvPack->nodeId,
                    pRecvPack->channel, pRecvPack->STOFTransmitOffset,
                    pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
                    pRecvPack->healthStatusWord, pRecvPack->payloadLength,
                    pRecvPack->msgData[0], pRecvPack->msgData[1],
                    pRecvPack->msgData[2], pRecvPack->msgData[3],
                    pRecvPack->msgData[4], pRecvPack->msgData[5],
                    pRecvPack->msgData[6], pRecvPack->msgData[7],
                    pRecvPack->asyncVPC);

        }
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
    unsigned int nodeId = 0;

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


	hiMil1394NodeIdGet(pDev, chn, &nodeId);
    printf("nodeID:%d \n", nodeId);

    hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/

    hiMil1394MsgSpeedSet(pDev, chn, SEND_SPEED);
    TY_ASYNC_PACKET sendAsynData;

    memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

    hiMil1394NodeModeSet(pDev, chn, 1); //0:cc 1;RN BM:2

    sendAsynData.msgId = 5;
    sendAsynData.channel = 2;
    sendAsynData.nodeId = nodeId;
    sendAsynData.payloadLength = PAYLOAD_LEN;
    for (int n = 0; n < PAYLOAD_LEN / 4; n++)
    {
        //sendAsynData.msgData[n] = (n + 1) % 255;
        sendAsynData.msgData[n] = 0x55;
    }
    sendAsynData.STOFTransmitOffset = 100;
    sendAsynData.STOFReceiveOffset = 50;
    sendAsynData.STOFPHMOffset = 20;
    sendAsynData.heartBeatEnable = 0;
    sendAsynData.heartBeatStep = 0x0101;
    sendAsynData.heartBeatWord = 100;
    sendAsynData.softVPCEnable = 1;
    sendAsynData.VPCASYNCValue = ~(sendAsynData.msgId^sendAsynData.healthStatusWord^sendAsynData.heartBeatWord^sendAsynData.msgData[0]);
    hiMil1394AsyncWrite(pDev, chn, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back

    memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
    sendAsynData.msgId = 6;
	sendAsynData.channel = 2;
	sendAsynData.nodeId = nodeId;
    sendAsynData.payloadLength = PAYLOAD_LEN;
    for (int n = 0; n < PAYLOAD_LEN / 4; n++)
    {
        sendAsynData.msgData[n] = (n + 3) % 255;
    }
	sendAsynData.STOFTransmitOffset = 200;
	sendAsynData.STOFReceiveOffset = 300;
	sendAsynData.STOFPHMOffset = 20;
	sendAsynData.heartBeatEnable = 0;
    sendAsynData.heartBeatStep = 0x0202;
    sendAsynData.heartBeatWord = 200;
    sendAsynData.softVPCEnable = 1;
	hiMil1394AsyncWrite(pDev, chn, ASYN_SEND_MODE, &sendAsynData);
	printf("press any key to start\n");
    getchar();
	hiMil1394StartStop(pDev, chn, 1);
	printf("press any key to stop\n");

	getchar();
	hiMil1394StartStop(pDev, chn, 0);

    hiMil1394UnRegisterInterrupt(pDev);

    for (int i = 0; i < MSG_TYPE_NUM; i++)
    {
        hiMil1394GetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);
        printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
    }

	ErrorCheck(hiMil1394CloseCard(pDev));
	printf("interrupt count:%d totalCount:%d \n", intNum2, totalCount);
    fclose(in[0]);
    fclose(in[1]);
    fclose(in[2]);

    return 0;
}

