/**
 *
 *  The test program is used to test this working condition:
 *  Open card and get all card informations.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "HiPCIe1394Public.h"

#define CHN_NO 0

int status;

#define ErrorCheck(func) {status = func; if(status != HR_SUCCESS ){printf("Error:%08x\n",status);}}

int intNum2;
#define Delay(delaytime) usleep(delaytime*1000) /*unit:ms*/

int main() 
{
	INT_PARA intPara;
    int i;
	HR_DEVICE pDev;
	int chn = CHN_NO;
	HR_UINT cardCount = 0;
	hiMil1394Scan(&cardCount);
	printf("card num:%d \n", cardCount);
    hiMil1394OpenCard(&pDev, 0);
    if (pDev <= 0) 
	{
        printf("open card error\n");
        return 0;
    }

    printf("open card success.\n");
	ErrorCheck(hiMil1394Reset(pDev));
	sleep(2);
	
	//card type
	HR_UINT cardType;
    hiMil1394GetCardType(pDev, &cardType);
    printf("card type 0x%04x\n", cardType);

    //card channel number
    HR_UINT cardChnNum;
    hiMil1394GetCardChnNum(pDev, &cardChnNum);
    printf("card channel number %d\n", cardChnNum);

    TY_MIL1394_VERSION version;
	hiMil1394GetVersion(pDev, &version);
	printf("driver version:%s card hardware version:V%x logic version:%x \n",
			version.drvVersion, version.cardVersion.Hardware_Version,
			version.cardVersion.Logical_Version);
    
	unsigned int nodeId = 0;
	hiMil1394NodeIdGet(pDev, chn, &nodeId);
	printf("nodeID:%d \n", nodeId);

	hiMil1394ExtTriggerEnable(pDev, 7, 1, 1);
	getchar();
	hiMil1394BusReset(pDev, chn, 0);
    sleep(1);
	unsigned int nodeCnt = 0, nodeNum;
	hiMil1394NodeNumGet(pDev, chn, &nodeCnt, &nodeNum);
	printf("node count:%d nodeID:%d \n", nodeCnt, nodeNum);

	ErrorCheck(hiMil1394CloseCard(pDev));

    return 0;
}

