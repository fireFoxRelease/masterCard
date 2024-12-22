/**
 *
 *  The test program is used to test this working condition:
 *  Open card, then send a short bus reset and long bus reset, get bus reset count.
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
    //printf("receiver interrupt number:%d \n", intNum2);
    HR_UINT rstcount = 0;
    hiMil1394GetMsgCnt(pDev, CHN_NO, BUS_RST_CNT, &rstcount);
    printf("rst cnt:%d \n", rstcount);
}

int main() 
{
	INT_PARA intPara;
    int i;
	HR_DEVICE pDev;
	int chn = CHN_NO;
	
    hiMil1394OpenCard(&pDev, 0);
    if (pDev <= 0) 
	{
        printf("open card error\n");
        return 0;
    }

    printf("open card success.\n");
	ErrorCheck(hiMil1394Reset(pDev));
	Sleep(2);
	
    hiMil1394RegisterInterrupt(pDev, intFunc, &intPara); /*Enable software interrupt*/

    hiMil1394NodeModeSet(pDev, chn, 0); //0:cc 1;RN BM:2

    printf("start bus reset\n");
    getchar();
    hiMil1394BusReset(pDev, 0, 0);
	Sleep(1);
	hiMil1394BusReset(pDev, 0, 1);
	Sleep(1);

	hiMil1394UnRegisterInterrupt(pDev);

	
	ErrorCheck(hiMil1394CloseCard(pDev));

    return 0;
}

