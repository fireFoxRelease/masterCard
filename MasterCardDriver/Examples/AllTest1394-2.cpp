/**
 *
 *  CPCI_CAN WINDOWS&Linux Driver Test program Version 1.2  (16/03/2016)
 *  Copyright (c) 1998-2016
 *  Hirain Technology, Inc.
 *  www.hirain.com
 *  support@hiraintech.com
 *  ALL RIGHTS RESERVED
 *
 **/

/**
 *
 *  The test program is used to test this working condition:
 *  Channel 0,1,2,3 send, and channel 0,1,2,3 receive by interrupt.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
//#include <unistd.h>
#include <pthread.h>

#include "HiPCIe1394Public.h"
#define LabView 0
#define IfLabView 1
#define PACKAGE_LEN 2008  //async payload����  ������44�ֽ�
#define ASYN_SEND_MODE 0  //0 ƫ��   1������
#define BUS_SPEED 2      //0 100     1 200     2 400  Mbps
#define ASYNC_NUM 30 // һ�����ڵ�async����   138  151          124   142
#define SEND_TIME 1000      //���ڸ��� stof����100000
#define WRITE_FILE 1     //�Ƿ�д�ļ�
#define FILTER 0          //�Ƿ�������

int status;
INT NUMdMA = 0;
FILE *in[2][4];
HR_DEVICE pDev;
HR_DEVICE pDev1;
pthread_t waitEvent;
int threadFlag;

#define ErrorCheck(func) {status = func; if(status != HR_SUCCESS ){printf("Error:%08x\n",status);}}


int intNum2;
#define Delay(delaytime) usleep(delaytime*1000) /*unit:ms*/

#define MSG_TYPE_NUM 10
typedef struct msg_static
{
	char msgName[128];
	int msgType;
	unsigned int count;
} MSG_STATIC;

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
	{ "stof vpc error count", RECV_STOF_VPC_ERR_CNT, 0 },
	{ "bus reset count", BUS_RST_CNT, 0 }, };

unsigned int intSwap(unsigned int data)
{
	unsigned int value = 0, tmp = 0;

	unsigned int ii, jj;
	for (ii = 0x80000000, jj = 0; ii != 0; ii = ii >> 1, jj++)
	{
		if (data & ii)
		{
			tmp = 1;
		}
		else
		{
			tmp = 0;
		}
		value |= tmp << jj;
		//printf("ii:%x \n", ii);
	}

	return value;
}

int totalCount = 0;
void* waitReadEvent(void* intPara)
{
	intNum2++;
	HR_DEVICE hDev = intPara;

	HR_UINT counter = 0;
	char* pRecvData = NULL;
	TY_RECV_PACKET* pRecvPack = NULL;
	int readCount = 1;
	int& countTmp = readCount;
	pRecvData = (char*)malloc(sizeof(TY_RECV_PACKET)* 1024);
	pRecvPack = (TY_RECV_PACKET*)pRecvData;

	while (threadFlag)
	{
		int i = 0;
		int j = 0;

		for (int recvChn = 0; recvChn < 4; recvChn++)
		{
			hiMil1394GetFrameNum(pDev, recvChn, &counter);
			totalCount += counter;

			for (int r = 0; r < counter; r++)
			{
				hiMil1394Read(pDev, recvChn, pRecvPack, countTmp);


			}
			Sleep(100);
		}

	}

	if (pRecvPack)
	{
		free(pRecvPack);
		pRecvPack = NULL;
	}
	return 0;

}
long long secTmp[3];
long long usecTmp[3];
long long sec[3];
long long usec[3];
long long diffValue[3];
long long diffValueU[3];

void intFunc1(INT_PARA intPara1)
{
	intNum2++;
	HR_DEVICE pDev = intPara1.pDev;
	//printf("receiver interrupt number:%d \n", intNum2);
	HR_UINT chn = intPara1.chn;
	HR_UINT counter = 0;
	char* pRecvData = NULL;
	TY_RECV_PACKET* pRecvPack = NULL;
	int readCount = 1;
	int &countTmp = readCount;

	pRecvData = (char*)malloc(sizeof(TY_RECV_PACKET)* 1024);
	pRecvPack = (TY_RECV_PACKET*)pRecvData;

	hiMil1394GetFrameNum(pDev1, chn, &counter);
	totalCount += counter;
	NUMdMA++;
	for (int r = 0; r < counter; r++)
	{
		hiMil1394Read(pDev1, chn, pRecvPack, countTmp);
#if WRITE_FILE
		if (pRecvPack->msgType == 0)
		{
			sec[chn] = pRecvPack->sec;
			usec[chn] = pRecvPack->usec;
			diffValue[chn] = sec[chn] - secTmp[chn];
			diffValueU[chn] = usec[chn] - usecTmp[chn];
			usecTmp[chn] = pRecvPack->usec;
			secTmp[chn] = pRecvPack->sec;
			/*if (((diffValueU[chn]>11000) && (diffValue[chn] != 1)) | ((diffValueU[chn] < -11000) && (diffValue[chn] != 1)))
			{
				printf("chn :%d diffValue:%d  diffValueU:%d  sec:%d usec:%d \n", chn, diffValue[chn], diffValueU[chn], pRecvPack->sec, pRecvPack->usec);
				fprintf(in[1][chn], "chn :%d diffValue:%d diffValueU :%d sec:%d usec:%d \n", chn, diffValue[chn], diffValueU[chn], pRecvPack->sec, pRecvPack->usec);
			}*/
			fprintf(
				in[1][chn],
				"sec:%d usec:%d rtc:%d  payload:%x %x %x %x %x %d %x %x %x VPC:%x\n",
				pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->STOFPayload0,
				pRecvPack->STOFPayload1, pRecvPack->STOFPayload2,
				pRecvPack->STOFPayload3, pRecvPack->STOFPayload4,
				pRecvPack->STOFPayload5, pRecvPack->STOFPayload6,
				pRecvPack->STOFPayload7, pRecvPack->STOFPayload8,
				pRecvPack->STOFVPC);

		}
		else if (pRecvPack->msgType == 1)
		{
			fprintf(
				in[1][chn],
				"sec:%d usec:%d rtc:%d Security:%x  msgId:%d nodeId:%d channel:%d sendoffset:%x  STOFPHMOffset:%x"
				"recvoffset:%x heartbeat:%x statusword:%x payloadlen:%d payload:%x %x %x %x %x %x %x sfotVPC:%x"
				" VPC:%x vpcerr:%x\n", pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->security,
				pRecvPack->msgId, pRecvPack->nodeId, pRecvPack->channel,
				pRecvPack->STOFTransmitOffset, pRecvPack->STOFPHMOffset,
				pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
				pRecvPack->healthStatusWord, pRecvPack->payloadLength,
				pRecvPack->msgData[0], pRecvPack->msgData[1],
				pRecvPack->msgData[2], pRecvPack->msgData[3],
				pRecvPack->msgData[496], pRecvPack->msgData[497],
				pRecvPack->msgData[498], pRecvPack->msgData[499],
				
				
				
				pRecvPack->asyncVPC, pRecvPack->asyncVPCErr);

		}
#endif
		//  printf("counter:%d chn:%d msgdi:%d msgType:%d\n", counter, pRecvPack->channel, pRecvPack->msgId, pRecvPack->msgType);
	}

	if (pRecvData)
	{
		free(pRecvData);
	}
}


void intFunc(INT_PARA intPara)
{
	intNum2++;
	HR_DEVICE pDev = intPara.pDev;
	//printf("receiver interrupt number:%d \n", intNum2);
	HR_UINT chn = intPara.chn;
	HR_UINT counter = 0;
	char* pRecvData = NULL;
	TY_RECV_PACKET* pRecvPack = NULL;
	int readCount = 1;
	int &countTmp = readCount;

	pRecvData = (char*)malloc(sizeof(TY_RECV_PACKET)* 1024);
	pRecvPack = (TY_RECV_PACKET*)pRecvData;
	//printf("inrFunc\n");
	hiMil1394GetFrameNum(pDev, chn, &counter);
	//printf("inrFunc hiMil1394GetFrameNum:%d\n", counter);
	totalCount += counter;
	NUMdMA++;
	for (int r = 0; r < counter; r++)
	{
		hiMil1394Read(pDev, chn, pRecvPack, countTmp);
#if WRITE_FILE
		if (pRecvPack->msgType == 0)
		{
			fprintf(
				in[0][chn],
				"sec:%d usec:%d rtc:%d  payload:%x %x %x %x %x %d %x %x %x VPC:%x\n",
				pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->STOFPayload0,
				pRecvPack->STOFPayload1, pRecvPack->STOFPayload2,
				pRecvPack->STOFPayload3, pRecvPack->STOFPayload4,
				pRecvPack->STOFPayload5, pRecvPack->STOFPayload6,
				pRecvPack->STOFPayload7, pRecvPack->STOFPayload8,
				pRecvPack->STOFVPC);

		}
		else if (pRecvPack->msgType == 1)
		{
			fprintf(
				in[0][chn],
				"sec:%d usec:%d rtc:%d Security:%x  msgId:%d nodeId:%d channel:%d sendoffset:%x  STOFPHMOffset:%x"
				"recvoffset:%x heartbeat:%x statusword:%x payloadlen:%d payload:%x %x %x %x %x %x %x  sfotVPC:%x"
				" VPC:%x vpcerr:%x\n", pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->security,
				pRecvPack->msgId, pRecvPack->nodeId, pRecvPack->channel,
				pRecvPack->STOFTransmitOffset, pRecvPack->STOFPHMOffset,
				pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
				pRecvPack->healthStatusWord, pRecvPack->payloadLength,
				pRecvPack->msgData[0], pRecvPack->msgData[1],
				pRecvPack->msgData[2], pRecvPack->msgData[3],
				pRecvPack->msgData[496], pRecvPack->msgData[497],
				pRecvPack->msgData[498], pRecvPack->msgData[499],
				
				pRecvPack->asyncVPC, pRecvPack->asyncVPCErr);

		}
#endif
		//  printf("counter:%d chn:%d msgdi:%d msgType:%d\n", counter, pRecvPack->channel, pRecvPack->msgId, pRecvPack->msgType);
	}

	if (pRecvData)
	{
		free(pRecvData);
	}
}




int main()
{

#if LabView
	while (getchar() != 'q')
	{

		intNum2 = 0;
		INT_PARA intPara;
		int i;
		int chn1 = 1;
		int chn2 = 2;
		int offset;
		int pthreadret;
		threadFlag = 1;

		in[0] = fopen("node0.txt", "w+");
		in[1] = fopen("node1.txt", "w+");
		in[2] = fopen("node2.txt", "w+");
		in[3] = fopen("node3.txt", "w+");

		hiMil1394OpenCard(&pDev, 0);
		if (pDev <= 0)
		{
			printf("open card error\n");
			return 0;
		}

		printf("open card success.\n");
		ErrorCheck(hiMil1394Reset(pDev));
		Sleep(1000);

		HR_UINT cardChnNum;
		hiMil1394GetCardChnNum(pDev, &cardChnNum);
		printf("cardChnNum :%d\n", cardChnNum);

		TY_MIL1394_VERSION readVersion;
		hiMil1394GetVersion(pDev, &readVersion);
		printf("Logical_Version=%x\n", readVersion.cardVersion.Logical_Version);
		HR_UINT rxEn = 0;
		HR_UINT readVlaue = 0, readValueTmp = 0;
		unsigned int nodeId = 1;



		HR_UINT read = 0;
		hiMil1394RegisterInterruptLabView(pDev);
		hiMil1394MsgSpeedSet(pDev, chn1, BUS_SPEED);
		hiMil1394MsgSpeedSet(pDev, chn2, BUS_SPEED);


		TY_STOF_PACKET sendPac;
		TY_ASYNC_PACKET sendAsynData;

		memset(&sendPac, 0, sizeof(TY_STOF_PACKET));
		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

		hiMil1394NodeModeSet(pDev, chn1, 1); //0:cc 1;RN BM:2
		hiMil1394NodeModeSet(pDev, chn2, 0); //0:cc 1;RN BM:2

		Sleep(100);
		hiMil1394NodeIdGet(pDev, chn1, &nodeId);
		printf("nodeID:%d \n", nodeId);
		hiMil1394NodeIdGet(pDev, chn2, &nodeId);
		printf("nodeID:%d \n", nodeId);



		hiMil1394STOFPeriodSet(pDev, chn2, 10000);//10ms
		hiMil1394STOFSet(pDev, chn2, 0, SEND_TIME);//10000
		pthreadret = pthread_create(&waitEvent, NULL, waitReadEvent, pDev);

		sendPac.STOFPayload0 = 0x1122;
		sendPac.STOFPayload1 = 0x12345678;
		sendPac.STOFPayload5 = 0x0;
		sendPac.STOFPayload8 = 0x3344;
		sendPac.VPCErr = 1;
		sendPac.heartBeatEnable = 1;
		sendPac.STOFVPC = ~(sendPac.STOFPayload0 ^ sendPac.STOFPayload1
			^ sendPac.STOFPayload2 ^ sendPac.STOFPayload3
			^ sendPac.STOFPayload4 ^ sendPac.STOFPayload5
			^ sendPac.STOFPayload6 ^ sendPac.STOFPayload7
			^ sendPac.STOFPayload8);
		printf("vpc:%x \n", sendPac.STOFVPC);
		hiMil1394STOFWrite(pDev, chn2, &sendPac);


		//hiMil1394RTCEnable(pDev, chn2, 1);

		for (int async_num = 0; async_num < ASYNC_NUM; async_num++)
		{
			sendAsynData.msgId = async_num;
			sendAsynData.channel = 1;
			sendAsynData.nodeId = 1;
			sendAsynData.payloadLength = PACKAGE_LEN;

			sendAsynData.STOFTransmitOffset = 10 + async_num * 20;
			sendAsynData.STOFReceiveOffset = 10 + async_num * 20;
			sendAsynData.msgData[0] = 0x5a5a;
			sendAsynData.msgData[498] = async_num;
			sendAsynData.STOFPHMOffset = 20;
			sendAsynData.heartBeatEnable = 1;
			sendAsynData.heartBeatStep = 0x0101;
			sendAsynData.heartBeatWord = 1;
			sendAsynData.softVPCEnable = 1;
			hiMil1394AsyncWrite(pDev, chn1, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
		}
		hiMil1394RTCEnable(pDev, chn1, 1);
		hiMil1394StartStop(pDev, 0, 1);
		hiMil1394StartStop(pDev, chn1, 1);
		hiMil1394StartStop(pDev, chn2, 1);
	
		printf("already start\n");

		getchar();
		threadFlag = 0;
		pthread_cancel(waitEvent);
		// Sleep(1000);
		hiMil1394StartStop(pDev, 0, 0);
	
		hiMil1394StartStop(pDev, chn2, 0);
		hiMil1394StartStop(pDev, chn1, 0);
		Sleep(1000);


		for (int chn = 0; chn < 4; chn++)
		{
			printf("chn:%d \n", chn);
			for (int i = 0; i < MSG_TYPE_NUM; i++)
			{
				hiMil1394GetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);
				printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
			}
		}
		hiMil1394UnRegisterInterrupt(pDev);

		ErrorCheck(hiMil1394CloseCard(pDev));

		getchar();
		fclose(in[0]);
		fclose(in[1]);
		fclose(in[2]);
		fclose(in[3]);
	}

	return 0;
#endif
#if IfLabView
	//while (getchar() != 'q')
	{
		intNum2 = 0;
		INT_PARA intPara, intPara1;
		int i;
		int chn1 = 0;
		int chn2 = 1;
		int chn3 = 2;
		int offset;

		in[0][0] = fopen("node0.txt", "w+");
		in[0][1] = fopen("node1.txt", "w+");
		in[0][2] = fopen("node2.txt", "w+");
		//  in[3] = fopen("node3.txt", "w+");
		in[1][0] = fopen("node10.txt", "w+");
		in[1][1] = fopen("node11.txt", "w+");
		in[1][2] = fopen("node12.txt", "w+");
		hiMil1394OpenCard(&pDev, 0);
		if (pDev <= 0)
		{
			printf("open card error\n");
			return 0;
		}
		hiMil1394OpenCard(&pDev1, 1);
		if (pDev <= 0)
		{
			printf("open card error\n");
			return 0;
		}
		HR_UINT cardType;
		hiMil1394GetCardType(pDev, &cardType);
		printf("cardType :%x\n", cardType);
		
		ErrorCheck(hiMil1394Reset(pDev));
		ErrorCheck(hiMil1394Reset(pDev1));
		Sleep(2);
		
		HR_UINT cardChnNum;
		hiMil1394GetCardChnNum(pDev, &cardChnNum);
		printf("cardChnNum :%d\n", cardChnNum);
		HR_UINT read = 0;

		Sleep(1000);
		TY_MIL1394_VERSION readVersion;
		hiMil1394GetVersion(pDev, &readVersion);
		printf("Logical_Version=%x\n",readVersion.cardVersion.Logical_Version);
		HR_UINT rxEn = 0;
		HR_UINT readVlaue = 0, readValueTmp = 0;
		unsigned int nodeId = 1;

		hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/
		hiMil1394MsgSpeedSet(pDev, chn1, BUS_SPEED);
		hiMil1394MsgSpeedSet(pDev, chn2, BUS_SPEED);
		hiMil1394MsgSpeedSet(pDev, chn3, BUS_SPEED);
		hiMil1394RegisterInterrupt(pDev1, intFunc1, &intPara1); /*Enable software interrupt*/
		hiMil1394MsgSpeedSet(pDev1, chn1, BUS_SPEED);
		hiMil1394MsgSpeedSet(pDev1, chn2, BUS_SPEED);
		hiMil1394MsgSpeedSet(pDev1, chn3, BUS_SPEED);
		/*hiMil1394MsgSpeedSet(pDev, 3, BUS_SPEED);*/

		TY_STOF_PACKET sendPac;
		TY_STOF_PACKET sendPac1;
		TY_STOF_PACKET sendPac2;
		TY_ASYNC_PACKET sendAsynData;

		memset(&sendPac, 0, sizeof(TY_STOF_PACKET));
		memset(&sendPac1, 0, sizeof(TY_STOF_PACKET));
		memset(&sendPac2, 0, sizeof(TY_STOF_PACKET));
		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

		hiMil1394NodeModeSet(pDev, chn1, 0); //0:cc 1;RN BM:2
		hiMil1394NodeModeSet(pDev, chn2, 0); //0:cc 1;RN BM:2
	    hiMil1394NodeModeSet(pDev, chn3, 0); //0:cc 1;RN BM:2
		hiMil1394NodeModeSet(pDev1, chn1, 2); //0:cc 1;RN BM:2
		hiMil1394NodeModeSet(pDev1, chn2, 2); //0:cc 1;RN BM:2
		hiMil1394NodeModeSet(pDev1, chn3, 2); //0:cc 1;RN BM:2
		Sleep(1);
		hiMil1394NodeIdGet(pDev, chn1, &nodeId);
		printf("nodeID:%d \n", nodeId);
		hiMil1394NodeIdGet(pDev, chn2, &nodeId);
		printf("nodeID:%d \n", nodeId);
		/*hiMil1394NodeIdGet(pDev, chn3, &nodeId);
		printf("nodeID:%d \n", nodeId);*/
		hiMil1394NodeIdGet(pDev1, chn1, &nodeId);
		printf("1 nodeID:%d \n", nodeId);
		hiMil1394NodeIdGet(pDev1, chn2, &nodeId);
		printf("1 nodeID:%d \n", nodeId);
		/*hiMil1394NodeIdGet(pDev1, chn3, &nodeId);
		printf(" 1 nodeID:%d \n", nodeId);*/
		/*  hiMil1394NodeIdGet(pDev, 3, &nodeId);
		  printf("nodeID:%d \n", nodeId);*/

		//hiMil1394BusReset(pDev, 0,0 );
		//Sleep(10);


		hiMil1394STOFPeriodSet(pDev, chn1, 10000);
		//hiMil1394STOFSet(pDev, chn, 0, 1);
		hiMil1394STOFSet(pDev, chn1, 0, SEND_TIME);

		hiMil1394STOFPeriodSet(pDev, chn2, 10000);
		//hiMil1394STOFSet(pDev, chn, 0, 1);
		hiMil1394STOFSet(pDev, chn2, 0, SEND_TIME);
		hiMil1394STOFPeriodSet(pDev, chn3, 10000);
		//hiMil1394STOFSet(pDev, chn, 0, 1);
		hiMil1394STOFSet(pDev, chn3, 0, SEND_TIME);

		sendPac.STOFPayload0 = 0x1;
		sendPac.STOFPayload1 = 0x1;
		sendPac.STOFPayload5 = 0x1;
		sendPac.STOFPayload8 = 0x1;
		sendPac.VPCErr = 1;
		sendPac.heartBeatEnable = 1;
		sendPac.STOFVPC = ~(sendPac.STOFPayload0 ^ sendPac.STOFPayload1
			^ sendPac.STOFPayload2 ^ sendPac.STOFPayload3
			^ sendPac.STOFPayload4 ^ sendPac.STOFPayload5
			^ sendPac.STOFPayload6 ^ sendPac.STOFPayload7
			^ sendPac.STOFPayload8);
		sendPac.STOFVPC = 0;
		hiMil1394STOFWrite(pDev, chn1, &sendPac);

		sendPac1.STOFPayload0 = 0x2;
		sendPac1.STOFPayload1 = 0x2;
		sendPac1.STOFPayload5 = 0x2;
		sendPac1.STOFPayload8 = 0x2;
		sendPac1.VPCErr = 1;
		sendPac1.heartBeatEnable = 1;
		sendPac1.STOFVPC = ~(sendPac1.STOFPayload0 ^ sendPac1.STOFPayload1
			^ sendPac1.STOFPayload2 ^ sendPac1.STOFPayload3
			^ sendPac1.STOFPayload4 ^ sendPac1.STOFPayload5
			^ sendPac1.STOFPayload6 ^ sendPac1.STOFPayload7
			^ sendPac1.STOFPayload8);
		sendPac1.STOFVPC = 0;
		hiMil1394STOFWrite(pDev, chn2, &sendPac1);

		sendPac2.STOFPayload0 = 0x3;
		sendPac2.STOFPayload1 = 0x3;
		sendPac2.STOFPayload5 = 0x3;
		sendPac2.STOFPayload8 = 0x3;
		sendPac2.VPCErr = 1;
		sendPac2.heartBeatEnable = 1;
		sendPac2.STOFVPC = ~(sendPac2.STOFPayload0 ^ sendPac2.STOFPayload1
			^ sendPac2.STOFPayload2 ^ sendPac2.STOFPayload3
			^ sendPac2.STOFPayload4 ^ sendPac2.STOFPayload5
			^ sendPac2.STOFPayload6 ^ sendPac2.STOFPayload7
			^ sendPac2.STOFPayload8);
		sendPac2.STOFVPC = 0;
		hiMil1394STOFWrite(pDev, chn3, &sendPac2);
		//hiMil1394RTCEnable(pDev, chn1, 1);
		//hiMil1394RTCEnable(pDev, chn2, 1);
		//hiMil1394RTCEnable(pDev, chn3, 1);
		//hiMil1394RTCEnable(pDev1, chn1, 1);
		//hiMil1394RTCEnable(pDev1, chn2, 1);
		//hiMil1394RTCEnable(pDev1, chn3, 1);
#if 0

		for (int async_num = 0; async_num < ASYNC_NUM; async_num++)
		{
			sendAsynData.msgId = async_num;
			sendAsynData.channel = 1;
			sendAsynData.nodeId = 1;
			sendAsynData.payloadLength = PACKAGE_LEN;
			sendAsynData.msgData[0] = 0x1122;
			sendAsynData.msgData[498] = async_num;
			sendAsynData.STOFTransmitOffset = 10+async_num*20;
			sendAsynData.STOFReceiveOffset = 10+async_num*20;
			sendAsynData.STOFPHMOffset = 20;
			sendAsynData.heartBeatEnable = 1;
			sendAsynData.heartBeatStep = 0x0101;
			sendAsynData.heartBeatWord = 1;
			sendAsynData.softVPCEnable = 1;
			sendAsynData.softVPCErr = 1;
			sendAsynData.hardVPCErr = 1;
			sendAsynData.VPCASYNCValue = 0xff;
			//sendAsynData.msgErr = 1;
			hiMil1394AsyncWrite(pDev, chn2, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
		}
#endif
		
#if 1
		for (int chnTmp = 0; chnTmp < 3; chnTmp++)
		{

			for (int async_num = 0; async_num < ASYNC_NUM; async_num++)
			{
				sendAsynData.msgId = async_num;
				sendAsynData.channel = 1;
				sendAsynData.nodeId = 1;
				sendAsynData.payloadLength = PACKAGE_LEN;
				sendAsynData.msgData[0] = 0x1122;
				sendAsynData.msgData[173] = async_num;
				sendAsynData.STOFTransmitOffset = 50 + async_num * 70;
				sendAsynData.STOFReceiveOffset = 50 + async_num * 70;
				sendAsynData.STOFPHMOffset = 20;
				sendAsynData.heartBeatEnable = 1;
				sendAsynData.heartBeatStep = 0x0101;
				sendAsynData.heartBeatWord = 1;
				sendAsynData.healthStatusWord = 1;
				sendAsynData.softVPCEnable = 1;
				sendAsynData.softVPCErr = 1;
				sendAsynData.hardVPCErr = 1;
				sendAsynData.VPCASYNCValue = 0xff;
				//sendAsynData.msgErr = 1;
				hiMil1394AsyncWrite(pDev, chnTmp, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
				
				
				//sendAsynData.msgId = async_num + ASYNC_NUM;
				//
				//sendAsynData.payloadLength = PACKAGE_LEN;
				//sendAsynData.msgData[0] = 0x11;
				//sendAsynData.msgData[173] = async_num+41;
				//sendAsynData.STOFTransmitOffset = 150 + async_num * 160;
				//sendAsynData.STOFReceiveOffset = 150 + async_num * 160;
				//sendAsynData.STOFPHMOffset = 20;
				//sendAsynData.heartBeatEnable = 1;
				//sendAsynData.heartBeatStep = 0x0101;
				//sendAsynData.heartBeatWord = 1;
				//sendAsynData.healthStatusWord = 1;
				//sendAsynData.softVPCEnable = 1;
				//sendAsynData.softVPCErr = 1;
				//sendAsynData.hardVPCErr = 1;
				//sendAsynData.VPCASYNCValue = 0x11;
				////sendAsynData.msgErr = 1;
				//hiMil1394AsyncWrite(pDev1, chnTmp, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
			
			}
			
		}
#endif
#if 0
		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));



		sendAsynData.msgId = 1;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 0;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x1;
		sendAsynData.msgData[1] = 2;
		//sendAsynData.msgData[499] = 1;
		sendAsynData.STOFTransmitOffset = 1200;
		sendAsynData.STOFReceiveOffset = 0;
		sendAsynData.STOFPHMOffset = 0;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0102;//0x0101
		sendAsynData.heartBeatWord = 1;
		sendAsynData.healthStatusWord = 1;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 1;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0;
		hiMil1394AsyncWrite(pDev, chn1, ASYN_SEND_MODE, &sendAsynData);

		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		sendAsynData.msgId = 2;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 1;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x01;
		sendAsynData.msgData[0] = 0xffff00000;
		sendAsynData.msgData[499] = 0xffff;
		sendAsynData.STOFTransmitOffset = 1400;
		sendAsynData.STOFReceiveOffset = 0;
		sendAsynData.STOFPHMOffset = 0;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0101;//0x0101
		sendAsynData.heartBeatWord = 200;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 1;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0;
		hiMil1394AsyncWrite(pDev, chn2, ASYN_SEND_MODE, &sendAsynData);

		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		sendAsynData.msgId = 3;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 1;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x3;
		sendAsynData.STOFTransmitOffset = 1600;
		sendAsynData.STOFReceiveOffset = 300;
		sendAsynData.STOFPHMOffset = 20;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0201;//0x0302
		sendAsynData.heartBeatWord = 1;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 0;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0xff;
		hiMil1394AsyncWrite(pDev, chn3, ASYN_SEND_MODE, &sendAsynData);

		sendAsynData.msgId = 5;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 0;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x1;
		sendAsynData.msgData[1] = 2;
		//sendAsynData.msgData[499] = 1;
		sendAsynData.STOFTransmitOffset = 200;
		sendAsynData.STOFReceiveOffset = 0;
		sendAsynData.STOFPHMOffset = 0;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0102;//0x0101
		sendAsynData.heartBeatWord = 1;
		sendAsynData.healthStatusWord = 1;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 1;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0;
		hiMil1394AsyncWrite(pDev1, chn1, ASYN_SEND_MODE, &sendAsynData);

		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		sendAsynData.msgId = 6;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 1;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x01;
		sendAsynData.msgData[0] = 0xffff00000;
		sendAsynData.msgData[499] = 0xffff;
		sendAsynData.STOFTransmitOffset = 400;
		sendAsynData.STOFReceiveOffset = 0;
		sendAsynData.STOFPHMOffset = 0;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0101;//0x0101
		sendAsynData.heartBeatWord = 200;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 1;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0;
		hiMil1394AsyncWrite(pDev1, chn2, ASYN_SEND_MODE, &sendAsynData);

		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		sendAsynData.msgId = 7;
		sendAsynData.channel = 2;
		sendAsynData.nodeId = 1;
		sendAsynData.payloadLength = PACKAGE_LEN;
		sendAsynData.msgData[0] = 0x3;
		sendAsynData.STOFTransmitOffset = 600;
		sendAsynData.STOFReceiveOffset = 300;
		sendAsynData.STOFPHMOffset = 20;
		sendAsynData.heartBeatEnable = 1;
		sendAsynData.heartBeatStep = 0x0201;//0x0302
		sendAsynData.heartBeatWord = 1;
		sendAsynData.softVPCEnable = 1;
		sendAsynData.softVPCErr = 0;
		sendAsynData.hardVPCErr = 0;
		sendAsynData.VPCASYNCValue = 0xff;
		hiMil1394AsyncWrite(pDev1, chn3, ASYN_SEND_MODE, &sendAsynData);
		////hiMil1394RTCEnable(pDev, chn2, 1);
		//memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		
#endif
		

#if FILTER
		TY_RECV_FILTER filter;
		filter.chnEn = 1;
		filter.msgIdEn = 1;
		filter.matchChn = 1;
		filter.matchMsgId = 3;
		hiMil1394FilterSet(pDev, 0, filter);
#endif
		printf("enter to start \n");
		getchar();
		hiMil1394StartStop(pDev1, chn3, 1);
		hiMil1394StartStop(pDev1, chn2, 1);
		hiMil1394StartStop(pDev1, chn1, 1);
		
		hiMil1394StartStop(pDev, chn3, 1);
		hiMil1394StartStop(pDev, chn2, 1);
		hiMil1394StartStop(pDev, chn1, 1);
		//hiMil1394StartStop(pDev, chn1, 1);


		printf("already start\n");

		//   getchar(); 
		//HR_UINT topoData[256];
		//HR_UINT topoDataLen = 256;
		//HR_UINT rstcount = 0;
		//HR_UINT freshFlag = 0;
		//hiMil1394TypologyGet(pDev, 2, &freshFlag, topoDataLen, topoData);
		//printf("fresh flag:%d data:%x %x %x %x %x %x \n", freshFlag, topoData[0], topoData[1], topoData[2], topoData[3], topoData[4], topoData[5]);
		
		//printf("enter to stop  msgid 5 \n");
		//getchar();
		//sendAsynData.msgId = 5;
		//sendAsynData.channel = 2;
		//sendAsynData.nodeId = 0;
		//sendAsynData.payloadLength = PACKAGE_LEN;
		//sendAsynData.msgData[0] = 0x1;
		//sendAsynData.msgData[1] = 2;
		//sendAsynData.msgData[499] = 1;
		//sendAsynData.STOFTransmitOffset = 200;
		//sendAsynData.STOFReceiveOffset = 0;
		//sendAsynData.STOFPHMOffset = 0;
		//sendAsynData.heartBeatEnable = 1;
		//sendAsynData.heartBeatStep = 0x0102;//0x0101
		//sendAsynData.heartBeatWord = 1;
		//sendAsynData.healthStatusWord = 1;
		//sendAsynData.softVPCEnable = 1;
		//sendAsynData.softVPCErr = 0;
		//sendAsynData.hardVPCErr = 1;
		//sendAsynData.msgErr = 1;
		//sendAsynData.VPCASYNCValue = 0;
		//
		//hiMil1394AsyncWrite(pDev1, chn1, ASYN_SEND_MODE, &sendAsynData);
		////sendPac.VPCErr = 1;
		///*sendPac.heartBeatEnable = 1;
		//sendPac.msgErr = 1;
		//hiMil1394STOFWrite(pDev, chn1, &sendPac);*/
		//Sleep(5);
		//fprintf(in[1][0], "stop  msg \n");
		//fprintf(in[1][1], "stop  msg \n");
		//fprintf(in[0][1], "stop  msg \n");
		//printf("enter msg 5 start\n");

		//getchar();
		//
		//fprintf(in[1][0], "start  msg \n");
		//fprintf(in[1][1], "start  msg \n");
		//fprintf(in[0][1], "start  msg \n");
		//sendAsynData.msgId = 5;
		//sendAsynData.channel = 2;
		//sendAsynData.nodeId = 0;
		//sendAsynData.payloadLength = PACKAGE_LEN;
		//sendAsynData.msgData[0] = 0x1;
		//sendAsynData.msgData[1] = 2;
		//sendAsynData.msgData[499] = 1;
		//sendAsynData.STOFTransmitOffset = 200;
		//sendAsynData.STOFReceiveOffset = 0;
		//sendAsynData.STOFPHMOffset = 0;
		//sendAsynData.heartBeatEnable = 1;
		//sendAsynData.heartBeatStep = 0x0102;//0x0101
		//sendAsynData.heartBeatWord = 1;
		//sendAsynData.healthStatusWord = 1;
		//sendAsynData.softVPCEnable = 1;
		//sendAsynData.softVPCErr = 1;
		//sendAsynData.hardVPCErr = 0;
		//sendAsynData.msgErr = 0;
		//sendAsynData.VPCASYNCValue = 0;
		//hiMil1394AsyncWrite(pDev1, chn1, ASYN_SEND_MODE, &sendAsynData);
		///*sendPac.msgErr = 0;
		//sendPac.heartBeatEnable = 1;
		//sendPac.VPCErr = 0;
		//hiMil1394STOFWrite(pDev, chn1, &sendPac);*/
		printf("enter msg  end\n");
		getchar();

		

		hiMil1394StartStop(pDev, chn1, 0);

		hiMil1394StartStop(pDev, chn2, 0);
			hiMil1394StartStop(pDev, chn3, 0);
			Sleep(11);
			hiMil1394StartStop(pDev1, chn1, 0);

			hiMil1394StartStop(pDev1, chn2, 0);
			hiMil1394StartStop(pDev1, chn3, 0);

		Sleep(1000);
		hiMil1394UnRegisterInterrupt(pDev);
		hiMil1394UnRegisterInterrupt(pDev1);
		Sleep(1000);
		for (int chn = 0; chn < 3; chn++)
		{
			printf(" pDev chn:%d \n", chn);
			for (int i = 0; i < MSG_TYPE_NUM; i++)
			{

				hiMil1394GetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);

				printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
				//hiMil1394GetMsgCnt(pDev, 1, msgStatic[i].msgType, &msgStatic[i].count);
				//printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
			}
		}
		for (int chn = 0; chn < 3; chn++)
		{
			printf("pDev 1 chn:%d \n", chn);
			for (int i = 0; i < MSG_TYPE_NUM; i++)
			{

				hiMil1394GetMsgCnt(pDev1, chn, msgStatic[i].msgType, &msgStatic[i].count);

				printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
				//hiMil1394GetMsgCnt(pDev, 1, msgStatic[i].msgType, &msgStatic[i].count);
				//printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
			}
		}

		hiMil1394ReadTest(pDev,2,0x04,&read);
		printf("2 READ DMA UP NUM %d   %d   %d\n", read, NUMdMA, totalCount);


		Sleep(100);
		hiMil1394ReadTest(pDev, 2, 0x00, &read);
		printf("2 READ DMA ~~~~~ NUM %d   \n", read);
		//fprintf(in[0], "READ DMA UP NUM %d   %d\n", read, NUMdMA);
		hiMil1394Clear(pDev, 0);
		hiMil1394Clear(pDev, 1);
		hiMil1394Clear(pDev, 2);
		hiMil1394Clear(pDev1, 0);
		hiMil1394Clear(pDev1, 1);
	    hiMil1394Clear(pDev1, 2);
		ErrorCheck(hiMil1394CloseCard(pDev));
		ErrorCheck(hiMil1394CloseCard(pDev1));

		 printf("interrupt count:%d totalCount:%d \n", intNum2, totalCount);
		getchar();
		fclose(in[0][0]);
		fclose(in[0][1]);
		fclose(in[0][2]);
		fclose(in[1][0]);
		fclose(in[1][1]);
		fclose(in[1][2]);
		// fclose(in[3]);
	}
	while (getchar() != 'q')
	{  
		printf("q and enter to quit!\n");
	}
	return 0;
#endif
}

