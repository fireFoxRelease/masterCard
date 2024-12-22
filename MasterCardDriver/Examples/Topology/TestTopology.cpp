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
//#include <unistd.h>
//#include <pthread.h>

#include "HiPCIe1394Public.h"
#define CHN_NO 0

int status;

#define ErrorCheck(func) {status = func; if(status != HR_SUCCESS ){printf("Error:%08x\n",status);}}

int intNum2;
#define Delay(delaytime) usleep(delaytime*1000) /*unit:ms*/

void intFunc(INT_PARA intPara)
{
    intNum2++;
    HR_DEVICE pDev = intPara.pDev;
    HR_UINT topoData[256];
    HR_UINT topoDataLen = 256;

    //printf("receiver interrupt number:%d \n", intNum2);
    HR_UINT rstcount = 0;
    hiMil1394GetMsgCnt(pDev, CHN_NO, BUS_RST_CNT, &rstcount);
    printf("rst cnt:%d \n", rstcount);

    HR_UINT freshFlag = 0;
    //hiMil1394TypologyGet(pDev, CHN_NO, &freshFlag, topoDataLen, topoData);
   // printf("fresh flag:%d data:%x %x \n", freshFlag, topoData[0], topoData[1]);
}

int main() 
{
	INT_PARA intPara;
    int i;
    int chn = CHN_NO;
	HR_DEVICE pDev;

	
    hiMil1394OpenCard(&pDev, 0);
    if (pDev <= 0) 
	{
        printf("open card error\n");
        return 0;
    }

    printf("open card success.\n");
	ErrorCheck(hiMil1394Reset(pDev));
	Sleep(1);

    hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/

    hiMil1394NodeModeSet(pDev, chn, 0); //0:cc 1;RN BM:2

    getchar();
    hiMil1394BusReset(pDev, 0, 0);
	Sleep(1000);
	hiMil1394UnRegisterInterrupt(pDev);

	ErrorCheck(hiMil1394CloseCard(pDev));
	getchar();

    return 0;
}

