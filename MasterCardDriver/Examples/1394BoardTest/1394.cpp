#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include "Ini.h"
#include "HiPCIe1394Public.h"
#ifdef __linux
#include "unistd.h"
#include <sys/time.h>
#include "time.h"
#endif
#ifdef WIN32
#include <time.h>
#include <sys/timeb.h>
#endif

#define LabView 1
#define IfLabView 0
#define PACKAGE_LEN 508
#define ASYN_SEND_MODE 0
#define SEND_TIME 10000
#define WRITE_FILE 0
#define FILTER 0
#pragma warning(disable : 6031)

HR_DEVICE pDev;
pthread_t waitEvent;
HR_UINT cardChnNum;

int threadFlag;
int intNum2;
int status;
int totalRecvCount;
int totalAsyncSendCount;
int totalSTOFSendCount;
int totalRecvBytes;
int totalSendBytes;
int txbytes;
int rxbytes;
int chnRNNum;

FILE* in[4];

#define ErrorCheck(fun)                     \
    {                                       \
        status = fun;                       \
        if (status != HR_SUCCESS)           \
        {                                   \
            printf("Error:%08x\n", status); \
        }                                   \
    }
#define Delay(delaytime) usleep(delaytime * 1000)
#define MSG_TYPE_NUM 4

typedef struct chn_config
{
    int chnModel[4];
    int chnBusSpeed[4];
    int RTCEnable[4];
    int VPCEnable;
    int chnRN[4];
    int chnCC;
} TY_CHN_CONFIG;

typedef struct stof_config
{
    int periodTime;
    int styleMode;
    int sendCount;
    int sysCntType;
} TY_STOF_CONFIG;

typedef struct msg_config
{
    int STOFPayload0;
    int STOFPayload1;
    int STOFPayload2;
    int STOFPayload3;
    int STOFPayload4;
    int STOFPayload5;
    int STOFPayload6;
    int STOFPayload7;
    int STOFPayload8;
    int STOFPHMOffset;
    int heartBeatEnable;
    int heartBeatStep;
    int heartBeatWord;
    int softVPCEnable;
    int asynSendMode;
    int channel;
    int msgData[8];
    int msgId;
    int printMethod;
    int VPCASYNCValue;
    int healthStatusWord;
} TY_MSG_CONFIG;

typedef struct all_config
{
    TY_CHN_CONFIG chnConfig;
    TY_STOF_CONFIG stofConfig;
    TY_MSG_CONFIG msgConfig;
} TY_ALL_CONFIG;

typedef struct mag_staic
{
    char msgName[128];
    int msgType;
    unsigned int count;
} MSG_STATIC;

MSG_STATIC msgStatic[MSG_TYPE_NUM] =
{
    {"stof tx packet count", STOF_SEND_CNT, 0},
    {"stof rx packet count", STOF_RECV_CNT, 0},
    {"asyn tx packet count", ASYN_SEND_CNT, 0},
    {"asyn rx packet count", ASYN_RECV_CNT, 0},
    /*{"hcrc error count", RECV_HCRC_ERR_CNT, 0},
    {"dcrc error count", RECV_DCRC_ERR_CNT, 0},
    {"vpc error count", RECV_VPC_ERR_CNT, 0},
    {"stof dcrc error count", STOF_DCRC_ERR_CNT, 0},
    {"stof vpc error count", STOF_DCRC_ERR_CNT, 0},
    {"bus reset count", BUS_RST_CNT, 0},*/
};

int i, j;
int offset;
int pthreadret;
int intNum;
unsigned int nodeId[4];
int scanf1394;
int asyncNum;

const char* configFileName = "config.ini";
const char* chnConfigStr = "ChnConfig";
const char* STOFConfigStr = "STOFConfig";
const char* msgConfigStr = "MsgConfig";

TY_ALL_CONFIG allConfig;
TY_STOF_PACKET sendPac;
TY_ASYNC_PACKET sendAsyncData;

int inivalue(TY_ALL_CONFIG* allConfig)
{
    CIni cIni;

    int rtnRead = cIni.Read(configFileName);
    if (!rtnRead)
    {
        printf("read filed.\n");
        return -1;
    }

    int rtnGet;

    rtnGet = cIni.GetValue(chnConfigStr, "chn0Model", allConfig->chnConfig.chnModel[0]);
    if (!rtnGet)
    {
        printf("chnConfig no chn0Model\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn0BusSpeed", allConfig->chnConfig.chnBusSpeed[0]);
    if (!rtnGet)
    {
        printf("chnConfig no chn0BusSpeed\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn0RTCEnable", allConfig->chnConfig.RTCEnable[0]);
    if (!rtnGet)
    {
        printf("chnConfig no chn0RTCEnable\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn1Model", allConfig->chnConfig.chnModel[1]);
    if (!rtnGet)
    {
        printf("chnConfig no chn1Model\n");
        return -1;
    }
    rtnGet = cIni.GetValue(chnConfigStr, "chn1BusSpeed", allConfig->chnConfig.chnBusSpeed[1]);
    if (!rtnGet)
    {
        printf("chnConfig no chn1BusSpeed\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn1RTCEnable", allConfig->chnConfig.RTCEnable[1]);
    if (!rtnGet)
    {
        printf("chnConfig no chn1RTCEnable\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn2Model", allConfig->chnConfig.chnModel[2]);
    if (!rtnGet)
    {
        printf("chnConfig no chn2Model\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn2BusSpeed", allConfig->chnConfig.chnBusSpeed[2]);
    if (!rtnGet)
    {
        printf("chnConfig no chn2BusSpeed\n");
        return -1;
    }

    rtnGet = cIni.GetValue(chnConfigStr, "chn2RTCEnable", allConfig->chnConfig.RTCEnable[2]);
    if (!rtnGet)
    {
        printf("chnConfig no chn2RTCEnable\n");
        return -1;
    }

    if (cardChnNum == 4)
    {
        rtnGet = cIni.GetValue(chnConfigStr, "chn3Model", allConfig->chnConfig.chnModel[3]);
        if (!rtnGet)
        {
            printf("chnConfig no chn3Model\n");
            return -1;
        }

        rtnGet = cIni.GetValue(chnConfigStr, "chn3BusSpeed", allConfig->chnConfig.chnBusSpeed[3]);
        if (!rtnGet)
        {
            printf("chnConfig no chn3BusSpeed\n");
            return -1;
        }

        rtnGet = cIni.GetValue(chnConfigStr, "chn3RTCEnable", allConfig->chnConfig.RTCEnable[3]);
        if (!rtnGet)
        {
            printf("chnConfig no chn3RTCEnable\n");
            return -1;
        }
    }

    rtnGet = cIni.GetValue(STOFConfigStr, "periodTime", allConfig->stofConfig.periodTime);
    if (!rtnGet)
    {
        printf("stofConfig no periodTime\n");
        return -1;
    }

    rtnGet = cIni.GetValue(STOFConfigStr, "styleMode", allConfig->stofConfig.styleMode);
    if (!rtnGet)
    {
        printf("stofConfig no styleMode\n");
        return -1;
    }

    rtnGet = cIni.GetValue(STOFConfigStr, "sysCntType", allConfig->stofConfig.sysCntType);
    if (!rtnGet)
    {
        printf("stofConfig no sysCntType\n");
        return -1;
    }

    rtnGet = cIni.GetValue(STOFConfigStr, "sendCount", allConfig->stofConfig.sendCount);
    if (!rtnGet)
    {
        printf("stofConfig no sendCount\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload0", allConfig->msgConfig.STOFPayload0);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload0\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload1", allConfig->msgConfig.STOFPayload1);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload1\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload2", allConfig->msgConfig.STOFPayload2);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload2\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload3", allConfig->msgConfig.STOFPayload3);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload3\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload4", allConfig->msgConfig.STOFPayload4);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload4\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload5", allConfig->msgConfig.STOFPayload5);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload5\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload6", allConfig->msgConfig.STOFPayload6);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload6\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload7", allConfig->msgConfig.STOFPayload7);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload7\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPayload8", allConfig->msgConfig.STOFPayload8);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPayload8\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "channel", allConfig->msgConfig.channel);
    if (!rtnGet)
    {
        printf("MsgConfig no channel\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[0]", allConfig->msgConfig.msgData[0]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[0]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[1]", allConfig->msgConfig.msgData[1]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[1]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[2]", allConfig->msgConfig.msgData[2]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[2]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[3]", allConfig->msgConfig.msgData[3]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[3]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[4]", allConfig->msgConfig.msgData[4]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[4]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[5]", allConfig->msgConfig.msgData[5]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[5]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[6]", allConfig->msgConfig.msgData[6]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[6]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "msgData[7]", allConfig->msgConfig.msgData[7]);
    if (!rtnGet)
    {
        printf("MsgConfig no msgData[7]\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "STOFPHMOffset", allConfig->msgConfig.STOFPHMOffset);
    if (!rtnGet)
    {
        printf("MsgConfig no STOFPHMOffset\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "heartBeatEnable", allConfig->msgConfig.heartBeatEnable);
    if (!rtnGet)
    {
        printf("MsgConfig no heartBeatEnable\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "heartBeatStep", allConfig->msgConfig.heartBeatStep);
    if (!rtnGet)
    {
        printf("MsgConfig no heartBeatStep\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "heartBeatWord", allConfig->msgConfig.heartBeatWord);
    if (!rtnGet)
    {
        printf("MsgConfig no heartBeatWord\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "softVPCEnable", allConfig->msgConfig.softVPCEnable);
    if (!rtnGet)
    {
        printf("MsgConfig no softVPCEnable\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "asynSendMode", allConfig->msgConfig.asynSendMode);
    if (!rtnGet)
    {
        printf("MsgConfig no asynSendMode\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "asyncNum", asyncNum);
    if (!rtnGet)
    {
        printf("MsgConfig no asyncNum\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "VPCASYNCValue", allConfig->msgConfig.VPCASYNCValue);
    if (!rtnGet)
    {
        printf("MsgConfig no VPCASYNCValue\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "printMethod", allConfig->msgConfig.printMethod);
    if (!rtnGet)
    {
        printf("MsgConfig no printMethod\n");
        return -1;
    }

    rtnGet = cIni.GetValue(msgConfigStr, "healthStatusWord", allConfig->msgConfig.healthStatusWord);
    if (!rtnGet)
    {
        printf("MsgConfig no healthStatusWord\n");
        return -1;
    }

    printf("chn0Model: %d\n", allConfig->chnConfig.chnModel[0]);
    printf("chn0BusSpeed: %d\n", allConfig->chnConfig.chnBusSpeed[0]);
    printf("chn1Model: %d\n", allConfig->chnConfig.chnModel[1]);
    printf("chn1BusSpeed: %d\n", allConfig->chnConfig.chnBusSpeed[1]);
    printf("chn2Model: %d\n", allConfig->chnConfig.chnModel[2]);
    printf("chn2BusSpeed: %d\n", allConfig->chnConfig.chnBusSpeed[2]);
    if (cardChnNum == 4)
    {
        printf("chn3Model: %d\n", allConfig->chnConfig.chnModel[3]);
        printf("chn3BusSpeed: %d\n", allConfig->chnConfig.chnBusSpeed[3]);
    }
    printf("periodTime: %d\n", allConfig->stofConfig.periodTime);
    printf("styleMode: %d\n", allConfig->stofConfig.styleMode);
    printf("sendCount: %d\n", allConfig->stofConfig.sendCount);
    printf("sysCntType: %d\n", allConfig->stofConfig.sysCntType);
    printf("STOFPayload0: %#x\n", allConfig->msgConfig.STOFPayload0);
    printf("STOFPayload1: %#x\n", allConfig->msgConfig.STOFPayload1);
    printf("STOFPayload2: %#x\n", allConfig->msgConfig.STOFPayload2);
    printf("STOFPayload3: %#x\n", allConfig->msgConfig.STOFPayload3);
    printf("STOFPayload4: %#x\n", allConfig->msgConfig.STOFPayload4);
    printf("STOFPayload5: %#x\n", allConfig->msgConfig.STOFPayload5);
    printf("STOFPayload6: %#x\n", allConfig->msgConfig.STOFPayload6);
    printf("STOFPayload7: %#x\n", allConfig->msgConfig.STOFPayload7);
    printf("STOFPayload8: %#x\n", allConfig->msgConfig.STOFPayload8);
    // printf("msgId: %d\n", allConfig->msgConfig.msgId);
    printf("channel: %d\n", allConfig->msgConfig.channel);
    printf("msgData[0]: %#x\n", allConfig->msgConfig.msgData[0]);
    printf("msgData[1]: %#x\n", allConfig->msgConfig.msgData[1]);
    printf("msgData[2]: %#x\n", allConfig->msgConfig.msgData[2]);
    printf("msgData[3]: %#x\n", allConfig->msgConfig.msgData[3]);
    printf("msgData[4]: %#x\n", allConfig->msgConfig.msgData[4]);
    printf("msgData[5]: %#x\n", allConfig->msgConfig.msgData[5]);
    printf("msgData[6]: %#x\n", allConfig->msgConfig.msgData[6]);
    printf("msgData[7]: %#x\n", allConfig->msgConfig.msgData[7]);
    printf("STOFPHMOffset: %d\n", allConfig->msgConfig.STOFPHMOffset);
    printf("heartBeatEnabl: %d\n", allConfig->msgConfig.heartBeatEnable);
    printf("heartBeatStep: %d\n", allConfig->msgConfig.heartBeatStep);
    printf("heartBeatWord: %d\n", allConfig->msgConfig.heartBeatWord);
    printf("softVPCEnable: %d\n", allConfig->msgConfig.softVPCEnable);
    printf("asynSendMode: %d\n", allConfig->msgConfig.asynSendMode);
    printf("asyncNum: %d\n", asyncNum);
    printf("VPCASYNCValue: %d\n", allConfig->msgConfig.VPCASYNCValue);
    printf("printMethod: %d\n", allConfig->msgConfig.printMethod);
    printf("healthStatusWord: %d\n", allConfig->msgConfig.healthStatusWord);
}

void intFunc(INT_PARA intPara)
{
    int ret;
    intNum++;
    HR_DEVICE pDev = intPara.pDev;
    HR_UINT chn = intPara.chn;

    HR_UINT counter = 0;
    char* pRecvData = NULL;
    TY_RECV_PACKET* pRecvPack = NULL;
    int readCount = 1;
    HR_INT& countTmp = readCount;

    pRecvData = (char*)malloc(sizeof(TY_RECV_PACKET) * 1024);
    pRecvPack = (TY_RECV_PACKET*)pRecvData;

    hiMil1394GetFrameNum(pDev, chn, &counter);
    totalRecvCount += counter;
    // printf("totalTecvCont:%d\n", totalRecvCount);

    for (int r = 0; r < counter; r++)
    {
#ifdef WIN32
        int ret = hiMil1394Read(pDev, chn, pRecvPack, countTmp); // ��ȡ��Ϣ֡
#endif
#ifdef __linux
        int ret = hiMil1394Read(pDev, chn, pRecvPack, &countTmp); // ��ȡ��Ϣ֡
#endif

        if (ret == 0)
        {
            if (allConfig.msgConfig.printMethod == 0 || allConfig.msgConfig.printMethod == 1)
            {
                if (pRecvPack->msgType == 0) // STOF��
                {
                    fprintf(
                        in[chn],
                        "STOF:sec:%d usec:%d rtc:%d  payload:%x %x %x %x %x %x %x %x %x VPC:%x\n",
                        pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->STOFPayload0,
                        pRecvPack->STOFPayload1, pRecvPack->STOFPayload2,
                        pRecvPack->STOFPayload3, pRecvPack->STOFPayload4,
                        pRecvPack->STOFPayload5, pRecvPack->STOFPayload6,
                        pRecvPack->STOFPayload7, pRecvPack->STOFPayload8,
                        pRecvPack->STOFVPC);
                }
                else if (pRecvPack->msgType == 1) // anync��
                {
                    fprintf(
                        in[chn],
                        "async:sec:%d usec:%d rtc:%d  msgId:%d nodeId:%d channel:%d sendoffset:%d "
                        "recvoffset:%d heartbeat:%x statusword:%x payloadlen:%d security:%d payload:%x %x %x %x %x %x %x %x"
                        " VPC:%x\n",
                        pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC,
                        pRecvPack->msgId, pRecvPack->nodeId, pRecvPack->channel,
                        pRecvPack->STOFTransmitOffset,
                        pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
                        pRecvPack->healthStatusWord, pRecvPack->payloadLength,
                        pRecvPack->security,
                        pRecvPack->msgData[0], pRecvPack->msgData[1],
                        pRecvPack->msgData[2], pRecvPack->msgData[3],
                        pRecvPack->msgData[4], pRecvPack->msgData[5],
                        pRecvPack->msgData[6], pRecvPack->msgData[7],
                        pRecvPack->asyncVPC);
                }
            }

            if (allConfig.msgConfig.printMethod == 0 || allConfig.msgConfig.printMethod == 2)
            {
                if (pRecvPack->msgType == 0) // STOF��
                {
                    printf(
                        "STOF:sec:%d usec:%d rtc:%d  payload:%x %x %x %x %x %x %x %x %x VPC:%x\n",
                        pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC, pRecvPack->STOFPayload0,
                        pRecvPack->STOFPayload1, pRecvPack->STOFPayload2,
                        pRecvPack->STOFPayload3, pRecvPack->STOFPayload4,
                        pRecvPack->STOFPayload5, pRecvPack->STOFPayload6,
                        pRecvPack->STOFPayload7, pRecvPack->STOFPayload8,
                        pRecvPack->STOFVPC);
                }
                else if (pRecvPack->msgType == 1) // anync��
                {
                    printf(
                        "async:sec:%d usec:%d rtc:%d  msgId:%d nodeId:%d channel:%d sendoffset:%d "
                        "recvoffset:%d heartbeat:%x statusword:%x payloadlen:%d security:%d payload:%x %x %x %x %x %x %x %x"
                        " VPC:%x\n",
                        pRecvPack->sec, pRecvPack->usec, pRecvPack->RTC,
                        pRecvPack->msgId, pRecvPack->nodeId, pRecvPack->channel,
                        pRecvPack->STOFTransmitOffset,
                        pRecvPack->STOFReceiveOffset, pRecvPack->heartBeatWord,
                        pRecvPack->healthStatusWord, pRecvPack->payloadLength,
                        pRecvPack->security,
                        pRecvPack->msgData[0], pRecvPack->msgData[1],
                        pRecvPack->msgData[2], pRecvPack->msgData[3],
                        pRecvPack->msgData[4], pRecvPack->msgData[5],
                        pRecvPack->msgData[6], pRecvPack->msgData[7],
                        pRecvPack->asyncVPC);
                }
            }
        }
    }

    if (pRecvPack)
    {
        free(pRecvPack);
        pRecvPack = NULL;
    }
}

void readMsgCount()
{
    for (int chn = 0; chn < cardChnNum; chn++)
    {
        printf("chn:%d \n", chn);
        for (i = 0; i < MSG_TYPE_NUM; i++)
        {
            hiMil1394GetMsgCnt(pDev, chn, msgStatic[i].msgType, &msgStatic[i].count); // ��ȡ������Ϣ����
            printf("%s:%d   \n", msgStatic[i].msgName, msgStatic[i].count);

            if (i == 0)
            {
                printf("stof txbytes:%d   \n", 36 * msgStatic[i].count);
                totalSTOFSendCount += msgStatic[i].count;
                // totalSendBytes += 36 * msgStatic[i].count;
            }

            if (i == 1)
            {
                printf("stof rxbytes:%d   \n", 36 * msgStatic[i].count);
                // totalRecvBytes += 36 * msgStatic[i].count;
            }

            if (i == 2)
            {
                printf("asyn txbytes:%d   \n", 2000 * msgStatic[i].count);
                totalAsyncSendCount += msgStatic[i].count;
                // totalSendBytes += 2000 * msgStatic[i].count;
            }

            if (i == 3)
            {
                printf("asyn rxbytes:%d   \n\n", 2000 * msgStatic[i].count);
                // totalSendBytes += 2000 * msgStatic[i].count;
            }
        }
    }
}

int main()
{
#ifdef WIN32
    time_t tt;
    struct tm* st;
    SYSTEMTIME t1;

    time(&tt);
    GetSystemTime(&t1);

    printf("start sec: %ld, msec: %ld\n", tt, (int)t1.wMilliseconds);
#endif
#ifdef __linux
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("start sec: %ld, msec: %ld\n", (long long)tv.tv_sec, (long long)tv.tv_usec / 1000);
#endif

    int ret;
    threadFlag = 1;
    hiMil1394OpenCard(&pDev, 0); // �򿪰忨
    if (pDev <= 0)
    {
        printf("open failed.\n");
        return 0;
    }
    else
    {
        // printf("open success.\n");
    }

    ErrorCheck(hiMil1394Reset(pDev)); // ��λ�忨
#ifdef WIN32
    Sleep(2000);
#endif
#ifdef __linux
    sleep(2);
#endif

    hiMil1394GetCardChnNum(pDev, &cardChnNum); // ��ȡ�忨ͨ����
    printf("cardChnNum:%d\n", cardChnNum);
    // cardChnNum = 3;

    inivalue(&allConfig);
    printf("<------------------------------------ Enter any key to start test. ------------------------------------>\n");
    getchar();

    if (allConfig.msgConfig.printMethod == 0 || allConfig.msgConfig.printMethod == 1)
    {
        in[0] = fopen("chn[0].txt", "w+");
        in[1] = fopen("chn[1].txt", "w+");
        in[2] = fopen("chn[2].txt", "w+");
        in[3] = fopen("chn[3].txt", "w+");
    }

    TY_MIL1394_VERSION readVersion;
    hiMil1394GetVersion(pDev, &readVersion); // ��ȡ�忨�汾
    printf("driver version:%c%c%c%c%c%c , hardware version:%x logic version:%x\n", readVersion.drvVersion[0], readVersion.drvVersion[1], readVersion.drvVersion[2], readVersion.drvVersion[3], readVersion.drvVersion[4], readVersion.drvVersion[5], readVersion.cardVersion.Hardware_Version, readVersion.cardVersion.Logical_Version);

    INT_PARA intPara;
    ret = hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); // ע���ж�
    if (ret == 0)
    {
        // printf("interrupt success.\n");
    }
    else
    {
        printf("interrupt failed.\n");
    }

    for (i = 0; i < cardChnNum; i++)
    {
        hiMil1394MsgSpeedSet(pDev, i, allConfig.chnConfig.chnBusSpeed[i]); // ����ͨ������Ϣ��������
        hiMil1394NodeModeSet(pDev, i, allConfig.chnConfig.chnModel[i]);    // ���ýڵ���Ϣģʽ, 0��CC 1��RN 2��BM
        if (allConfig.chnConfig.chnModel[i] == 0)
        {
            allConfig.chnConfig.chnCC = i;
        }
        else if (allConfig.chnConfig.chnModel[i] == 1)
        {
            allConfig.chnConfig.chnRN[chnRNNum] = i;
            hiMil1394NodeIdGet(pDev, i, &nodeId[chnRNNum]); // ��ȡRN�ڵ�Id
            // printf("chnRNNum:%d\n", chnRNNum);
            chnRNNum++;
        }
    }

    hiMil1394STOFPeriodSet(pDev, allConfig.chnConfig.chnCC, allConfig.stofConfig.periodTime); // ����cc�ڵ��STOF����������

    hiMil1394STOFSet(pDev, allConfig.chnConfig.chnCC, allConfig.stofConfig.styleMode, allConfig.stofConfig.sendCount); // ����cc�ڵ��STOF������ģʽ
    hiMil1394STOFVPCEnable(pDev, allConfig.chnConfig.chnCC, allConfig.msgConfig.softVPCEnable);                        // ����VPCʹ��

    sendPac.STOFVPC = ~(sendPac.STOFPayload0 ^ sendPac.STOFPayload1 ^ sendPac.STOFPayload2 ^ sendPac.STOFPayload3 ^ sendPac.STOFPayload4 ^ sendPac.STOFPayload5 ^ sendPac.STOFPayload6 ^ sendPac.STOFPayload7 ^ sendPac.STOFPayload8);
    // printf("vpc:%x \n", sendPac.STOFVPC);
    for (i = 0; i < asyncNum; i++)
    {
        sendAsyncData.nodeId = nodeId[j];
        sendAsyncData.channel = allConfig.msgConfig.channel;
        sendAsyncData.msgId = i;
        sendAsyncData.payloadLength = PACKAGE_LEN;
        sendAsyncData.msgData[0] = allConfig.msgConfig.msgData[0];
        sendAsyncData.msgData[1] = allConfig.msgConfig.msgData[1];
        sendAsyncData.msgData[2] = allConfig.msgConfig.msgData[2];
        sendAsyncData.msgData[3] = allConfig.msgConfig.msgData[3];
        sendAsyncData.msgData[4] = allConfig.msgConfig.msgData[4];
        sendAsyncData.msgData[5] = allConfig.msgConfig.msgData[5];
        sendAsyncData.msgData[6] = allConfig.msgConfig.msgData[6];
        sendAsyncData.msgData[7] = allConfig.msgConfig.msgData[7];
        sendAsyncData.STOFTransmitOffset = 10 + allConfig.msgConfig.msgId * 20;
        sendAsyncData.STOFReceiveOffset = 10 + allConfig.msgConfig.msgId * 20;
        sendAsyncData.STOFPHMOffset = allConfig.msgConfig.STOFPHMOffset;
        sendPac.STOFPayload0 = allConfig.msgConfig.STOFPayload0;
        sendPac.STOFPayload1 = allConfig.msgConfig.STOFPayload1;
        sendPac.STOFPayload2 = allConfig.msgConfig.STOFPayload2;
        sendPac.STOFPayload3 = allConfig.msgConfig.STOFPayload3;
        sendPac.STOFPayload4 = allConfig.msgConfig.STOFPayload4;
        sendPac.STOFPayload5 = allConfig.msgConfig.STOFPayload5;
        sendPac.STOFPayload6 = allConfig.msgConfig.STOFPayload6;
        sendPac.STOFPayload7 = allConfig.msgConfig.STOFPayload7;
        sendPac.STOFPayload8 = allConfig.msgConfig.STOFPayload8;
        sendAsyncData.healthStatusWord = allConfig.msgConfig.healthStatusWord;
        sendAsyncData.heartBeatEnable = allConfig.msgConfig.heartBeatEnable;
        sendAsyncData.heartBeatStep = allConfig.msgConfig.heartBeatStep;
        sendAsyncData.heartBeatWord = allConfig.msgConfig.heartBeatWord;

        sendAsyncData.softVPCEnable = allConfig.msgConfig.softVPCEnable;
        sendAsyncData.VPCASYNCValue = allConfig.msgConfig.VPCASYNCValue;
        hiMil1394AsyncWrite(pDev, allConfig.chnConfig.chnRN[0], allConfig.msgConfig.asynSendMode, &sendAsyncData); // ����rn�ڵ㷢���첽����ģʽ
    }

    ret = hiMil1394STOFWrite(pDev, allConfig.chnConfig.chnCC, allConfig.stofConfig.sysCntType, &sendPac); // ����STOF����system count��ϵͳ���������·�ʽ
    if (ret == 0)
    {
        // printf("STOF write success.\n");
    }
    else
    {
        printf("STOF write failed.\n");
    }

    for (i = 0; i < cardChnNum; i++)
    {
        hiMil1394RTCEnable(pDev, i, allConfig.chnConfig.RTCEnable[i]); // ͨ��RTC�ڵ�ʹ��
        ret = hiMil1394StartStop(pDev, i, 1);                          // ʹ��ͨ��
        if (ret == 0)
        {
            // printf("chn[%d] Start success.\n", chn[i]);
        }
        else
        {
            printf("chn[%d] Start failed.\n", i);
        }
    }

#ifdef WIN32
    Sleep(2000);
#endif
#ifdef __linux
    usleep(2000);
#endif
    printf("already start\n");
    getchar();
    printf("cardChnNum:%d\n", cardChnNum);
    hiMil1394StartStop(pDev, allConfig.chnConfig.chnCC, 0);
    for (i = 0; i < cardChnNum; i++)
    {
        ret = hiMil1394StartStop(pDev, i, 0); // �ر�ͨ��ʹ��
        if (ret == 0)
        {
            // printf("chn[%d] Stop success.\n", chn[i]);
        }
        else
        {
            printf("chn[%d] Stop failed.\n", i);
        }
    }

#ifdef WIN32
    Sleep(3000);
#endif
#ifdef __linux
    sleep(3);
#endif
    hiMil1394UnRegisterInterrupt(pDev); // ע���ж�

    readMsgCount(); // ͳ����Ϣ��������

    printf("packet loss:%d data corrent:%d\n", ((cardChnNum - 1) * (totalAsyncSendCount + totalSTOFSendCount)) - totalRecvCount, (cardChnNum - 1) * (totalAsyncSendCount + totalSTOFSendCount));

    ErrorCheck(hiMil1394CloseCard(pDev));
#ifdef WIN32
    Sleep(2000);
#endif
#ifdef __linux
    sleep(2);
#endif

    if (allConfig.msgConfig.printMethod == 0 || allConfig.msgConfig.printMethod == 1)
    {
        fclose(in[0]);
        fclose(in[1]);
        fclose(in[2]);
        fclose(in[3]);
    }
#ifdef WIN32
    time(&tt);
    GetSystemTime(&t1);
    printf("end sec: %ld, msec: %ld\n", tt, (int)t1.wMilliseconds);
#endif
#ifdef __linux
    gettimeofday(&tv, NULL);
    printf("end sec: %ld, msec: %ld\n", (long long)tv.tv_sec, (long long)tv.tv_usec / 1000);
#endif

    return 0;
}
