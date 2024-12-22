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

#include "HiPCIeMasterPublic.h"
#define LabView 0
#define IfLabView 1
#define PACKAGE_LEN 2008  //async payload����  ������44�ֽ�
#define ASYN_SEND_MODE 0  //0 ƫ��   1������
#define BUS_SPEED 2      //0 100     1 200     2 400  Mbps
#define ASYNC_NUM 50 // һ�����ڵ�async����   138  151          124   142
#define SEND_TIME 10000      //���ڸ��� stof����100000//1000
//#define WRITE_FILE 1     //�Ƿ�д�ļ�
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

		for (int recvChn = 0; recvChn < 3; recvChn++)
		{
			hiMilMasterGetFrameNum(pDev, recvChn, &counter);
			totalCount += counter;

			for (int r = 0; r < counter; r++)
			{
				hiMilMasterRead(pDev, recvChn, pRecvPack, countTmp);
#if WRITE_FILE
				if (pRecvPack->msgType == 0)
				{
					
					/*if (((diffValueU[chn]>11000) && (diffValue[chn] != 1)) | ((diffValueU[chn] < -11000) && (diffValue[chn] != 1)))
					{
						printf("chn :%d diffValue:%d  diffValueU:%d  sec:%d usec:%d \n", chn, diffValue[chn], diffValueU[chn], pRecvPack->sec, pRecvPack->usec);
						fprintf(in[1][chn], "chn :%d diffValue:%d diffValueU :%d sec:%d usec:%d \n", chn, diffValue[chn], diffValueU[chn], pRecvPack->sec, pRecvPack->usec);
					}*/
					fprintf(
						in[0][recvChn],
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
						in[0][recvChn],
						"sec:%d usec:%d rtc:%d Security:%x  msgId:%d nodeId:%d channel:%d sendoffset:%x  STOFPHMOffset:%x"
						"recvoffset:%x heartbeat:%x statusword:%x payloadlen:%d payload:%x %x %x %x %x %x %x sfotVPC:%x"
						" VPC:%x vpcerr:%x\n", pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->security,
						pRecvPack->msgId, pRecvPack->nodeId, pRecvPack->channel,
						pRecvPack->STOFTransmitOffset, pRecvPack->STOFPHMOffset,
						pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
						pRecvPack->healthStatusWord, pRecvPack->payloadLength,
						pRecvPack->msgData[0], pRecvPack->msgData[1],
						pRecvPack->msgData[2], pRecvPack->msgData[3],
						pRecvPack->msgData[121], pRecvPack->msgData[122],
						pRecvPack->msgData[123], pRecvPack->msgData[124],



						pRecvPack->asyncVPC, pRecvPack->asyncVPCErr);

				}
#endif


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

	hiMilMasterGetFrameNum(pDev1, chn, &counter);
	totalCount += counter;
	NUMdMA++;
	for (int r = 0; r < counter; r++)
	{
		hiMilMasterRead(pDev1, chn, pRecvPack, countTmp);
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
	hiMilMasterGetFrameNum(pDev, chn, &counter);
	//printf("inrFunc hiMilMasterGetFrameNum:%d\n", counter);
	totalCount += counter;
	NUMdMA++;
	for (int r = 0; r < counter; r++)
	{
		hiMilMasterRead(pDev, chn, pRecvPack, countTmp);
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
	//while (getchar() != 'q')
	{

		intNum2 = 0;
		INT_PARA intPara;
		int i;
		int chn1 = 1;
		int chn2 = 2;
		int offset;
		int pthreadret;
		threadFlag = 1;

		in[0][0] = fopen("node0.txt", "w+");
		in[0][1] = fopen("node1.txt", "w+");
		in[0][2] = fopen("node2.txt", "w+");

		hiMilMasterOpenCard(&pDev, 0);
		if (pDev <= 0)
		{
			printf("open card error\n");
			return 0;
		}

		printf("open card success.\n");
		ErrorCheck(hiMilMasterReset(pDev));
		Sleep(1000);

		HR_UINT cardChnNum;
		hiMilMasterGetCardChnNum(pDev, &cardChnNum);
		printf("cardChnNum :%d\n", cardChnNum);

		TY_MILMaster_VERSION readVersion;
		hiMilMasterGetVersion(pDev, &readVersion);
		printf("Logical_Version=%x\n", readVersion.cardVersion.Logical_Version);
		HR_UINT rxEn = 0;
		HR_UINT readVlaue = 0, readValueTmp = 0;
		unsigned int nodeId = 1;



		HR_UINT read = 0;
		hiMilMasterRegisterInterruptLabView(pDev);
		hiMilMasterMsgSpeedSet(pDev, 0, BUS_SPEED);
		hiMilMasterMsgSpeedSet(pDev, chn1, BUS_SPEED);
		hiMilMasterMsgSpeedSet(pDev, chn2, BUS_SPEED);


		TY_STOF_PACKET sendPac;
		TY_ASYNC_PACKET sendAsynData;

		memset(&sendPac, 0, sizeof(TY_STOF_PACKET));
		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));
		hiMilMasterNodeModeSet(pDev, 0, 0); //0:cc 1;RN BM:2
		hiMilMasterNodeModeSet(pDev, chn1, 1); //0:cc 1;RN BM:2
		hiMilMasterNodeModeSet(pDev, chn2, 2); //0:cc 1;RN BM:2

		Sleep(100);
		hiMilMasterNodeIdGet(pDev, chn1, &nodeId);
		printf("nodeID:%d \n", nodeId);
		hiMilMasterNodeIdGet(pDev, chn2, &nodeId);
		printf("nodeID:%d \n", nodeId);



		hiMilMasterSTOFPeriodSet(pDev, 0, 10000);//10ms
		hiMilMasterSTOFSet(pDev, 0, 0, SEND_TIME);//10000
		pthreadret = pthread_create(&waitEvent, NULL, waitReadEvent, pDev);

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
		hiMilMasterSTOFWrite(pDev, 0, &sendPac);



#if 1

		for (int async_num = 0; async_num < ASYNC_NUM; async_num++)
		{
			sendAsynData.msgId = async_num;
			sendAsynData.channel = 1;
			sendAsynData.nodeId = 1;
			sendAsynData.payloadLength = PACKAGE_LEN;
			sendAsynData.msgData[0] = 0x1122;
			sendAsynData.msgData[173] = async_num;
			sendAsynData.STOFTransmitOffset = 60 + async_num * 60;
			sendAsynData.STOFReceiveOffset = 60 + async_num * 60;
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
			hiMilMasterAsyncWrite(pDev, 0, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
			//sendAsynData.msgId = async_num+ ASYNC_NUM;
			//sendAsynData.channel = 1;
			//sendAsynData.nodeId = 1;
			//sendAsynData.payloadLength = PACKAGE_LEN;
			//sendAsynData.msgData[0] = 0x1122;
			//sendAsynData.msgData[173] = async_num;
			//sendAsynData.STOFTransmitOffset = 120 + async_num * 120;
			//sendAsynData.STOFReceiveOffset = 120 + async_num * 120;
			//sendAsynData.STOFPHMOffset = 20;
			//sendAsynData.heartBeatEnable = 1;
			//sendAsynData.heartBeatStep = 0x0101;
			//sendAsynData.heartBeatWord = 1;
			//sendAsynData.healthStatusWord = 1;
			//sendAsynData.softVPCEnable = 1;
			//sendAsynData.softVPCErr = 1;
			//sendAsynData.hardVPCErr = 1;
			//sendAsynData.VPCASYNCValue = 0xff;
			////sendAsynData.msgErr = 1;
			//hiMilMasterAsyncWrite(pDev, chn2, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back




		}

#endif
		hiMilMasterRTCEnable(pDev, chn1, 1);
		hiMilMasterStartStop(pDev, chn2, 1);
		hiMilMasterStartStop(pDev, chn1, 1);
		
		hiMilMasterStartStop(pDev, 0, 1);
	
		printf("already start\n");

		getchar();
		threadFlag = 0;
		pthread_cancel(waitEvent);
		// Sleep(1000);
		hiMilMasterStartStop(pDev, 0, 0);
		Sleep(10);
		
		hiMilMasterStartStop(pDev, chn1, 0);
		hiMilMasterStartStop(pDev, chn2, 0);
		Sleep(1000);


		for (int chn = 0; chn < 3; chn++)
		{
			printf("chn:%d \n", chn);
			for (int i = 0; i < MSG_TYPE_NUM; i++)
			{
				hiMilMasterGetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);
				printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
			}
		}
		hiMilMasterUnRegisterInterrupt(pDev);

		hiMilMasterClear(pDev, 0);
		hiMilMasterClear(pDev, 1);
		hiMilMasterClear(pDev, 2);

		ErrorCheck(hiMilMasterCloseCard(pDev));


		printf("interrupt count:%d totalCount:%d \n", intNum2, totalCount);
		getchar();
		fclose(in[0][0]);
		fclose(in[0][1]);
		fclose(in[0][2]);

	}
	while (getchar() != 'q')
	{
		printf("q and enter to quit!\n");
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
	
		hiMilMasterOpenCard(&pDev, 0);
		if (pDev <= 0)
		{
			printf("open card error\n");
			return 0;
		}
		HR_UINT cardType;
		hiMilMasterGetCardType(pDev, &cardType);
		printf("cardType :%x\n", cardType);
		
		ErrorCheck(hiMilMasterReset(pDev));
		
		Sleep(2);
		
		HR_UINT cardChnNum;
		hiMilMasterGetCardChnNum(pDev, &cardChnNum);
		printf("cardChnNum :%d\n", cardChnNum);
		HR_UINT read = 0;

		Sleep(1000);
		TY_MILMaster_VERSION readVersion;
		hiMilMasterGetVersion(pDev, &readVersion);
		printf("Logical_Version=%x\n",readVersion.cardVersion.Logical_Version);
		HR_UINT rxEn = 0;
		HR_UINT readVlaue = 0, readValueTmp = 0;
		unsigned int nodeId = 1;

		hiMilMasterRegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/
		hiMilMasterMsgSpeedSet(pDev, chn1, BUS_SPEED);
		hiMilMasterMsgSpeedSet(pDev, chn2, BUS_SPEED);
		hiMilMasterMsgSpeedSet(pDev, chn3, BUS_SPEED);
		
		/*hiMilMasterMsgSpeedSet(pDev, 3, BUS_SPEED);*/

		TY_STOF_PACKET sendPac;
		TY_STOF_PACKET sendPac1;
		TY_STOF_PACKET sendPac2;
		TY_ASYNC_PACKET sendAsynData;

		memset(&sendPac, 0, sizeof(TY_STOF_PACKET));
		memset(&sendPac1, 0, sizeof(TY_STOF_PACKET));
		memset(&sendPac2, 0, sizeof(TY_STOF_PACKET));
		memset(&sendAsynData, 0, sizeof(TY_ASYNC_PACKET));

		hiMilMasterNodeModeSet(pDev, chn1, 0); //0:cc 1;RN BM:2
		hiMilMasterNodeModeSet(pDev, chn2, 1); //0:cc 1;RN BM:2
	    hiMilMasterNodeModeSet(pDev, chn3, 2); //0:cc 1;RN BM:2
		
		Sleep(1);
		hiMilMasterNodeIdGet(pDev, chn1, &nodeId);
		printf("nodeID:%d \n", nodeId);
		hiMilMasterNodeIdGet(pDev, chn2, &nodeId);
		printf("nodeID:%d \n", nodeId);
		hiMilMasterNodeIdGet(pDev, chn3, &nodeId);
		printf("nodeID:%d \n", nodeId);
	

		//hiMilMasterBusReset(pDev, 0,0 );
		//Sleep(10);


		hiMilMasterSTOFPeriodSet(pDev, chn1, 10000);
		//hiMilMasterSTOFSet(pDev, chn, 0, 1);
		//hiMilMasterSTOFSet(pDev, chn1, 0, SEND_TIME);
		hiMilMasterSTOFSet(pDev, chn1, 0, SEND_TIME);

		

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
		hiMilMasterSTOFWrite(pDev, chn1, &sendPac);

	
	
#if 1
		
			for (int async_num = 0; async_num < ASYNC_NUM; async_num++)
			{
				sendAsynData.msgId = async_num;
				sendAsynData.channel = 1;
				sendAsynData.nodeId = 1;
				sendAsynData.payloadLength = PACKAGE_LEN;
				sendAsynData.msgData[0] = 0x1122;
				sendAsynData.msgData[173] = async_num;
				sendAsynData.STOFTransmitOffset = 80 + async_num * 160;
				sendAsynData.STOFReceiveOffset = 80 + async_num * 160;
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
				hiMilMasterAsyncWrite(pDev, chn1, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
				sendAsynData.msgId = async_num+ ASYNC_NUM;
				sendAsynData.channel = 1;
				sendAsynData.nodeId = 1;
				sendAsynData.payloadLength = PACKAGE_LEN;
				sendAsynData.msgData[0] = 0x1122;
				sendAsynData.msgData[173] = async_num;
				sendAsynData.STOFTransmitOffset = 160 + async_num * 160;
				sendAsynData.STOFReceiveOffset = 160 + async_num * 160;
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
				hiMilMasterAsyncWrite(pDev, chn2, ASYN_SEND_MODE, &sendAsynData); //0 time 1:back to back
				
				
				
			
			}
		
#endif


#if FILTER
		TY_RECV_FILTER filter;
		filter.chnEn = 1;
		filter.msgIdEn = 1;
		filter.matchChn = 1;
		filter.matchMsgId = 3;
		hiMilMasterFilterSet(pDev, 0, filter);
#endif
		printf("enter to start \n");
		getchar();
		
		hiMilMasterStartStop(pDev, chn3, 1);
		hiMilMasterStartStop(pDev, chn2, 1);
		hiMilMasterStartStop(pDev, chn1, 1);
		//hiMilMasterStartStop(pDev, chn1, 1);


		printf("already start\n");


	
		getchar();

		

		hiMilMasterStartStop(pDev, chn1, 0);
		Sleep(10);
		hiMilMasterStartStop(pDev, chn2, 0);
		Sleep(10);
		hiMilMasterStartStop(pDev, chn3, 0);
			
		
		Sleep(1000);
		hiMilMasterUnRegisterInterrupt(pDev);
		
		Sleep(1000);
		for (int chn = 0; chn < 3; chn++)
		{
			printf(" pDev chn:%d \n", chn);
			for (int i = 0; i < MSG_TYPE_NUM; i++)
			{

				hiMilMasterGetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count);

				printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
				//hiMilMasterGetMsgCnt(pDev, 1, msgStatic[i].msgType, &msgStatic[i].count);
				//printf("%s %d \n", msgStatic[i].msgName, msgStatic[i].count);
			}
		}
		

		/*hiMilMasterReadTest(pDev,2,0x04,&read);
		printf("2 READ DMA UP NUM %d   %d   %d\n", read, NUMdMA, totalCount);


		Sleep(100);
		hiMilMasterReadTest(pDev, 2, 0x00, &read);
		printf("2 READ DMA ~~~~~ NUM %d   \n", read);*/
		//fprintf(in[0], "READ DMA UP NUM %d   %d\n", read, NUMdMA);
		hiMilMasterClear(pDev, 0);
		hiMilMasterClear(pDev, 1);
		hiMilMasterClear(pDev, 2);

		ErrorCheck(hiMilMasterCloseCard(pDev));
	

		printf("interrupt count:%d totalCount:%d \n", intNum2, totalCount);
		getchar();
		fclose(in[0][0]);
		fclose(in[0][1]);
		fclose(in[0][2]);
		
	}
	while (getchar() != 'q')
	{  
		printf("q and enter to quit!\n");
	}
	return 0;
#endif
}

