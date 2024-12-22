/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, MasterCardQueueInitialize)
#endif

/*
单一的默认I/O队列和单一的请求处理函数，EvtIoDefault。KMDF将会将设备所有的请求发送到默认I/O队列，
然后它会调用驱动程序的EvtIoDefault来将每一个请求递交给驱动程序。

*单一的默认I/O队列和多个请求处理函数，例如EvtIoRead、EvtIoWrite和EvtIoDeviceControl。KMDF会将设备所有的请求发送到默认I/O队列。
然后会调用驱动程序的EvtIoRead处理函数来递交读请求、调用EvtIoWrite处理函数来递交写请求、调用EvtIoDeviceControl处理函数来递交设备I/O控制请求。
*/

NTSTATUS
MasterCardQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;

    PAGED_CODE();
    
	/*WDF_IO_QUEUE_CONFIG_INIT(
	&queueConfig,
	WdfIoQueueDispatchSequential
	);

	queueConfig.EvtIoDeviceControl = MasterCardEvtIoDeviceControl;

	status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
	if (!NT_SUCCESS(status)) {
	DbgPrint("WdfIoQueueCreate failed.");
	return status;
	}

	//对于非默认队列，必须指定要分发的I/O请求类型
	//The WdfDeviceConfigureRequestDispatching method causes the framework to queue a specified type of I/O requests to a specified I/O queue.
	status = WdfDeviceConfigureRequestDispatching(
	device,
	queue,
	WdfRequestTypeDeviceControl
	);
	if (!NT_SUCCESS(status)) {
	DbgPrint("WdfDeviceConfigureRequestDispatching failed.");
	return status;
	}*/

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

	//处理I/O请求的例程，根据应用程序传入控制字的不同会执行不同的任务，
	//包括读BAR0物理起始地址、读寄存器、写寄存器
    queueConfig.EvtIoDeviceControl = MasterCardEvtIoDeviceControl; 

	//创建I/O队列，处理I/O请求
    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 WDF_NO_OBJECT_ATTRIBUTES,
                 &queue
                 );
	DebugPrint(LOUD, "WdfIoQueueCreate success.");
    if( !NT_SUCCESS(status) ) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID
MasterCardMapReadBuffer(
IN WDFWORKITEM  WorkItem)
{
	DMA_BUFFER_ITEM* rxBufferItem;
	WDFDEVICE Device = WdfWorkItemGetParentObject(WorkItem);
	NTSTATUS ntStatus = STATUS_SUCCESS;
	WDFREQUEST Request;
	PWORKITEM              item;
	PDEVICE_CONTEXT pDevContext;
	PVOID		outBuffer;

	PAGED_CODE();

	KIRQL curkirql = 0;
	curkirql = KeGetCurrentIrql(); //获取当前IRQL
	KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));

	item = GetWorkItem(WorkItem);
	Request = item->Request;
	pDevContext = item->DevExt;
	outBuffer = item->Buffer;

	KdPrint((_DRIVER_NAME_"oubuffer addr:%p ", outBuffer));
	rxBufferItem = (DMA_BUFFER_ITEM*)outBuffer;
	try {
		rxBufferItem->virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->ReadMdl,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority | MdlMappingNoWrite
			);
	}
	except(EXCEPTION_EXECUTE_HANDLER) {
		DbgPrint("error code");
	}

	rxBufferItem->length = pDevContext->ReadCommonBufferSize;
	rxBufferItem->phyAddress = pDevContext->ReadCommonBufferBaseLA.u.LowPart;
	KdPrint((_DRIVER_NAME_"map addr:%x ", rxBufferItem->virAddress));
	WdfObjectDelete(WorkItem);
	KdPrint((_DRIVER_NAME_"WdfObjectDelete"));
}

NTSTATUS MasterCardGetReadBufWork(PDEVICE_CONTEXT pDevContext, _In_ WDFREQUEST Request)
{

	WDF_WORKITEM_CONFIG workItemConfig;
	WDFWORKITEM workItem;
	WDF_OBJECT_ATTRIBUTES attributes;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWORKITEM              item;
	PVOID     outBuffer;

	ntStatus = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(DMA_BUFFER_ITEM),
		&outBuffer,
		NULL
		);

	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %!STATUS!",
			ntStatus));
		return;
	}

	WDF_WORKITEM_CONFIG_INIT(&workItemConfig, MasterCardMapReadBuffer);
	workItemConfig.AutomaticSerialization = FALSE;
	//WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, WORKITEM);
	attributes.ParentObject = pDevContext->Device;

	ntStatus = WdfWorkItemCreate(&workItemConfig,
		&attributes,
		&workItem);
	if (!NT_SUCCESS(ntStatus))
	{
		TraceEvents(TRACE_LEVEL_INFORMATION,
			TRACE_QUEUE,
			"Failed to create workitem %x\n", ntStatus);
		KdPrint((_DRIVER_NAME_"WdfWorkItemCreate failed ntStatus:%x ", ntStatus));
		return;
	}
#if 1
	item = GetWorkItem(workItem);
	item->DevExt = pDevContext;
	item->Request = Request;
	item->Buffer = outBuffer;

	KdPrint((_DRIVER_NAME_"oubuffer addr:%p ", item->Buffer));
	//
	// Execute this work item.
	//
#endif
	WdfWorkItemEnqueue(workItem);

	//return STATUS_PENDING;
	WdfWorkItemFlush(workItem);
	return ntStatus;
}

void GetWriteBufferThread(IN void* pContext)
{
	PDEVICE_CONTEXT pDevContext;
	WDFREQUEST Request;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	DMA_BUFFER_ITEM* txBufferItem;
	PVOID     outBuffer;

	KdPrint((_DRIVER_NAME_"GetWriteBufferThread in "));

	pDevContext = (PDEVICE_CONTEXT)pContext;
	Request = pDevContext->Request;
#if 0
	ntStatus = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(DMA_BUFFER_ITEM),
		&outBuffer,
		NULL
		);

	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
			ntStatus));
		WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(DMA_BUFFER_ITEM));
		return;
	}
	KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer success %x!",
		ntStatus));


	txBufferItem = (DMA_BUFFER_ITEM*)outBuffer;
	RtlZeroMemory(txBufferItem, sizeof(DMA_BUFFER_ITEM));
	try 
	{
		txBufferItem->virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->WriteMdl,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
	}
	except(EXCEPTION_EXECUTE_HANDLER) 
	{
		KdPrint((_DRIVER_NAME_"EXCEPTION_EXECUTE_HANDLER"));
	}

	txBufferItem->length = pDevContext->WriteCommonBufferSize;
	txBufferItem->phyAddress = pDevContext->WriteCommonBufferBaseLA.u.LowPart;
	txBufferItem->phyAddressH = pDevContext->WriteCommonBufferBaseLA.u.HighPart;
	//txBufferItem->phyAddress = pDevContext->WriteCommonBufferBaseLA.u.HighPart;
	KdPrint((_DRIVER_NAME_"MARKWriteCommonBufferBaseLA.u.LowPart:%x ", pDevContext->WriteCommonBufferBaseLA.u.LowPart));
	KdPrint((_DRIVER_NAME_"MARKWriteCommonBufferBaseLA.u.HighPart:%x ", pDevContext->WriteCommonBufferBaseLA.u.HighPart));
	//int *pData32 = (int*)((unsigned long long)pDevContext->Bar0VirtualAddress + 0x04);  //mark
	//pData32[0] = pDevContext->WriteCommonBufferBaseLA.u.HighPart;
	KdPrint((_DRIVER_NAME_"txBufferItem->virAddress:%p ", txBufferItem->virAddress));
#endif
	WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(DMA_BUFFER_ITEM));
}

void GetIOMemoryBufferThread(IN void* pContext)
{
	PDEVICE_CONTEXT pDevContext;
	WDFREQUEST Request;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PVOID     outBuffer;

	KdPrint((_DRIVER_NAME_"GetIOMemoryBufferThread in "));
	pDevContext = (PDEVICE_CONTEXT)pContext;
	Request = pDevContext->Request;

	KIRQL curkirql = 0;
	curkirql = KeGetCurrentIrql(); //获取当前IRQL
	//KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));

	ntStatus = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(IO_MEMORY),
		&outBuffer,
		NULL
		);

	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
			ntStatus));
		WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(IO_MEMORY));
		return ;
	}

	PIO_MEMORY registerTmp = (PIO_MEMORY)outBuffer;

	RtlZeroMemory(registerTmp, sizeof(IO_MEMORY));
	registerTmp->bar[0].phyAddress = pDevContext->Bar0PhysicalAddress;
	try
	{
		registerTmp->bar[0].virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->pMdl0,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
	}
	except(EXCEPTION_EXECUTE_HANDLER)
	{
		KdPrint((_DRIVER_NAME_"EXCEPTION_EXECUTE_HANDLER"));
	}

	registerTmp->bar[0].size = pDevContext->Bar0Length;

#if 1
	/*try 
	{
		registerTmp->bar[2].vir_address = MmMapLockedPagesSpecifyCache(
			pDevContext->pMdl1,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
		);
	}
	except (EXCEPTION_EXECUTE_HANDLER) 
	{
		KdPrint((_DRIVER_NAME_"EXCEPTION_EXECUTE_HANDLER"));
	}*/
	registerTmp->bar[2].phyAddress = pDevContext->Bar1PhysicalAddress;
	registerTmp->bar[2].size = pDevContext->Bar1Length;
#endif

	//KdPrint((_DRIVER_NAME_":bar 0 usrviraddr：%x 2 addr:%x ", registerTmp->bar[0].virAddress, registerTmp->bar[2].virAddress));
	WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(IO_MEMORY));

}


void GetReadBufferThread(IN void* pContext)
{
	PDEVICE_CONTEXT pDevContext;
	WDFREQUEST Request;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	DMA_BUFFER_ITEM* rxBufferItem;
	PVOID     outBuffer;

	KdPrint((_DRIVER_NAME_"GetReadBufferThread in "));
	pDevContext = (PDEVICE_CONTEXT)pContext;
	Request = pDevContext->Request;

	ntStatus = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(DMA_BUFFER_ITEM),
		&outBuffer,
		NULL
		);

	if (!NT_SUCCESS(ntStatus))
	{
		KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
			ntStatus));
		WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(DMA_BUFFER_ITEM));
		return;
	}
	KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer success %x!STATUS!",
		ntStatus));

	rxBufferItem = (DMA_BUFFER_ITEM*)outBuffer;
	try {
		rxBufferItem->virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->ReadMdl,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority | MdlMappingNoWrite
			);
	}
	except(EXCEPTION_EXECUTE_HANDLER) {
		DbgPrint("error code");
	}

	rxBufferItem->length = pDevContext->ReadCommonBufferSize;
	rxBufferItem->phyAddress = pDevContext->ReadCommonBufferBaseLA.u.LowPart;
	rxBufferItem->phyAddressH = pDevContext->ReadCommonBufferBaseLA.u.HighPart;

	KdPrint((_DRIVER_NAME_"MARKReadCommonBufferBaseLA.u.LowPart:%x ", pDevContext->ReadCommonBufferBaseLA.u.LowPart));
	KdPrint((_DRIVER_NAME_"MARKReadCommonBufferBaseLA.u.HighPart:%x ", pDevContext->ReadCommonBufferBaseLA.u.HighPart));
	//int *pData32 = (int*)((unsigned long long)pDevContext->Bar0VirtualAddress + 0x14);  //mark
	//pData32[0] = pDevContext->ReadCommonBufferBaseLA.u.HighPart;
	KdPrint((_DRIVER_NAME_"rxBufferItem->virAddress addr:%p ", rxBufferItem->virAddress));

	WdfRequestCompleteWithInformation(Request, ntStatus, sizeof(DMA_BUFFER_ITEM));

}

VOID
MasterCardEvtIoDeviceControl(
_In_ WDFQUEUE Queue,
_In_ WDFREQUEST Request,
_In_ size_t OutputBufferLength,
_In_ size_t InputBufferLength,
_In_ ULONG IoControlCode
)
/*++

Routine Description:

This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

Queue -  Handle to the framework queue object that is associated with the
I/O request.

Request - Handle to a framework request object.

OutputBufferLength - Size of the output buffer in bytes

InputBufferLength - Size of the input buffer in bytes

IoControlCode - I/O control code.

Return Value:

VOID

--*/
{
	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
		Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);


	WDFDEVICE device;
	PDEVICE_CONTEXT pDevContext;

	NTSTATUS  status;

	PVOID      inBuffer;
	PVOID     outBuffer;
	ULONG      AddressOffset;

	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	pDevContext = DeviceGetContext(device);

	//KdPrint((_DRIVER_NAME_"IoControlCode：%lx .", IoControlCode));
	switch (IoControlCode) {
		//根据CTL_CODE请求码作相应的处理
	case IOCTL_MAP_TX_DMA_BUFFER:
	{
		DebugPrint(LOUD, "IOCTL_MAP_TX_DMA_BUFFER.");
		DMA_BUFFER_ITEM* txBufferItem;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(DMA_BUFFER_ITEM),
			&outBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
				status));
			break;
		}

		txBufferItem = (DMA_BUFFER_ITEM*)outBuffer;
		txBufferItem->virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->TxBufMdl,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
		txBufferItem->length = pDevContext->TxBufferSize;
		txBufferItem->phyAddress = pDevContext->TxDMABusAddr.u.LowPart;

		WdfRequestCompleteWithInformation(Request, status, sizeof(DMA_BUFFER_ITEM));
		DebugPrint(LOUD, "mark - IOCTL_MAP_TX_DMA_BUFFER.");

	}
	break;
	case IOCTL_MAP_RX_DMA_BUFFER:
	{
		DebugPrint(LOUD, "IOCTL_MAP_RX_DMA_BUFFER.");
		DMA_BUFFER_ITEM* rxBufferItem;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(DMA_BUFFER_ITEM),
			&outBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
				status));
			break;
		}

		KIRQL curkirql = 0;
		KIRQL oldkirql = 0;
		KIRQL irqlTmp = 0;
		curkirql = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));
		if (curkirql > APC_LEVEL){ //若当前IRQL过高则调低到APC_LEVEL
			KeLowerIrql(APC_LEVEL);
		}

		irqlTmp = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", irqlTmp));

		rxBufferItem = (DMA_BUFFER_ITEM*)outBuffer;
		try{
			rxBufferItem->virAddress = MmMapLockedPagesSpecifyCache(
				pDevContext->RxBufMdl,
				UserMode,
				MmCached,
				NULL,
				0,
				NormalPagePriority
				);
		}
		except(EXCEPTION_EXECUTE_HANDLER)
		{
			KdPrint((_DRIVER_NAME_"EXCEPTION_EXECUTE_HANDLER !"));
		}
		KeRaiseIrql(curkirql, &oldkirql); //恢复场地
		rxBufferItem->length = pDevContext->RxBufferSize;
		rxBufferItem->phyAddress = pDevContext->RxDMABusAddr.u.LowPart;

		WdfRequestCompleteWithInformation(Request, status, sizeof(DMA_BUFFER_ITEM));
		DebugPrint(LOUD, "mark - IOCTL_MAP_RX_DMA_BUFFER.");

	}
	break;
	case IOCTL_RELEASE_TX_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_RELEASE_TX_POINTER.");
		void** txBufferVir;
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(void*),
			&inBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		txBufferVir = (void**)inBuffer;
		if (*txBufferVir)
		{
			MmUnmapLockedPages(*txBufferVir, pDevContext->TxBufMdl);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(void*));
		DebugPrint(LOUD, "mark - IOCTL_RELEASE_TX_POINTER.");

	}
	break;
	case IOCTL_RELEASE_RX_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_RELEASE_RX_POINTER.");
		void** rxBufferVir;
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(void*),
			&inBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		rxBufferVir = (void**)inBuffer;
		if (*rxBufferVir)
		{
			MmUnmapLockedPages(*rxBufferVir, pDevContext->RxBufMdl);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(void*));
		DebugPrint(LOUD, "mark - IOCTL_RELEASE_RX_POINTER.");

	}
	break;
	case IOCTL_GET_DEVICE_IDENTIFICATION:
	{
		DebugPrint(LOUD, "IOCTL_GET_DEVICE_IDENTIFICATION.");
		ppci_device_location pLocation;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(pci_device_location),
			&outBuffer,
			NULL
			);
		pLocation = (ppci_device_location)outBuffer;
		pLocation->bus = pDevContext->BusNumber;
		pLocation->function = pDevContext->FunctionNumber;
		pLocation->device = pDevContext->DeviceNumber;
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
				status));
			break;
		}
		WdfRequestCompleteWithInformation(Request, status, sizeof(pci_device_location));
		DebugPrint(LOUD, "mark - IOCTL_GET_DEVICE_IDENTIFICATION.");

	}
	break;
	case IOCTL_IOCTL_READ_REGISTER0:
	{
		//DebugPrint(LOUD, "IOCTL_IOCTL_READ_REGISTER0.");
		PREGISTER_DATA_ITEM pRegisterDatar;
		unsigned int* pDataRead;
		unsigned int* ptr;

		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(REGISTER_DATA_ITEM),
			&inBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		pRegisterDatar = (PREGISTER_DATA_ITEM)inBuffer;
		//KdPrint((_DRIVER_NAME_":pRegisterDatar offset：%x length:%d \n", pRegisterDatar->offset, pRegisterDatar->length));
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(unsigned int),
			&outBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		pDataRead = (unsigned int*)outBuffer;

		//KdPrint((_DRIVER_NAME_" pDevContext->Bar0Length =%x  pRegisterDatar->offset = %x pRegisterDatar->length=%x \n", pDevContext->Bar0Length, pRegisterDatar->offset, pRegisterDatar->length));
		

		if (pDevContext->Bar0Length >= (pRegisterDatar->offset + pRegisterDatar->length))
		{
			ptr = (unsigned int*)((unsigned long long)pDevContext->Bar0VirtualAddress + pRegisterDatar->offset);
			*pDataRead = ptr[0];
			//KdPrint((_DRIVER_NAME_"read Bar0  ptr:%p  data:%x \n", ptr, ptr[0]));
			//KdPrint((_DRIVER_NAME_":read pRegisterDatar offset：%llx length:%d \n", ptr, pRegisterDatar->length));
			//RtlMoveMemory(pDataRead, ptr, pRegisterDatar->length);
		   //KdPrint((_DRIVER_NAME_"prt:%p read Bar0 data:%x \n", ptr, ptr[0]));
			
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(unsigned int));
		//DebugPrint(LOUD, "mark - IOCTL_IOCTL_READ_REGISTER0.");

	}
	break;
	case IOCTL_IOCTL_READ_REGISTER1:
	{
		//DebugPrint(LOUD, "IOCTL_IOCTL_READ_REGISTER1");
		PREGISTER_DATA_ITEM pRegisterDatar;
		unsigned int* pDataRead;
		unsigned int* ptr;

		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(REGISTER_DATA_ITEM),
			&inBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		pRegisterDatar = (PREGISTER_DATA_ITEM)inBuffer;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(unsigned int),
			&outBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %x!STATUS!",
				status));
			break;
		}

		pDataRead = (unsigned int*)outBuffer;

		if (pDevContext->Bar1Length >= (pRegisterDatar->offset + pRegisterDatar->length))
		{
			
			ptr = (unsigned int*)((unsigned long long)pDevContext->Bar1VirtualAddress + pRegisterDatar->offset);
			*pDataRead = ptr[0];  //wzx
			//KdPrint((_DRIVER_NAME_"read Bar2  ptr:%llx  pRegisterDatar->length=%x \n", ptr, pRegisterDatar->length));
			//RtlMoveMemory(pDataRead, ptr, pRegisterDatar->length);   //wzx
			
			//KdPrint((_DRIVER_NAME_":read pRegisterDatar offset：%x length:%d data:%x\n", pRegisterDatar->offset, pRegisterDatar->length, ptr[0]));
		}
		
		WdfRequestCompleteWithInformation(Request, status, sizeof(unsigned int));
		//DebugPrint(LOUD, "mark - IOCTL_IOCTL_READ_REGISTER1");

	}
	break;
	case IOCTL_IOCTL_WRITE_REGISTER0:
	{
		//DebugPrint(LOUD, "IOCTL_IOCTL_WRITE_REGISTER0.");
		PREGISTER_DATA_ITEM pRegisterDatar;
		unsigned int* ptr;

		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(REGISTER_DATA_ITEM),
			&inBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x!STATUS!",
				status));
			break;
		}

		pRegisterDatar = (PREGISTER_DATA_ITEM)inBuffer;
		//KdPrint((_DRIVER_NAME_":pRegisterDatar offset：%llx length:%d data:%x\n", pRegisterDatar->offset, pRegisterDatar->length, pRegisterDatar->data));

		if (pDevContext->Bar0Length >= (pRegisterDatar->offset + pRegisterDatar->length))
		{
			ptr = (unsigned int*)((unsigned long long)pDevContext->Bar0VirtualAddress + pRegisterDatar->offset);
			*ptr = pRegisterDatar->data;
			//KdPrint((_DRIVER_NAME_"write Bar0  ptr:%p  data:%x \n", ptr, ptr[0]));
			//KdPrint((_DRIVER_NAME_":write virtualAddr：%llx,write length:%d data:%x\n", ptr, pRegisterDatar->length, pRegisterDatar->data));
			//RtlMoveMemory((IN VOID UNALIGNED *)ptr, (IN CONST VOID UNALIGNED *) &pRegisterDatar->data, pRegisterDatar->length);
		}
		/*int *test = pDevContext->WriteCommonBufferBase;
		if (pRegisterDatar->offset == 8)
		{
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[0]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[1]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[2]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[3]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[4]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[5]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[6]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[7]));
			KdPrint((_DRIVER_NAME_"write pDevContext->WriteCommonBufferBase:%p  data:%x \n", pDevContext->WriteCommonBufferBase, test[8]));
		}*/

		WdfRequestCompleteWithInformation(Request, status, sizeof(REGISTER_DATA_ITEM));
		//DebugPrint(LOUD, "mark - IOCTL_IOCTL_WRITE_REGISTER0.");

	}
	break;
	case IOCTL_IOCTL_WRITE_REGISTER2:
	{
		//DebugPrint(LOUD, "IOCTL_IOCTL_WRITE_REGISTER2");
		PREGISTER_DATA_ITEM pRegisterDatar;
		unsigned int* ptr;

		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(REGISTER_DATA_ITEM),
			&inBuffer,
			NULL
			);
		if (!NT_SUCCESS(status)) {
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %xSTATUS!",
				status));
			break;
		}

		pRegisterDatar = (PREGISTER_DATA_ITEM)inBuffer;
		//KdPrint((_DRIVER_NAME_":pRegisterDatar offset：%x length:%d data:%x\n", pRegisterDatar->offset, pRegisterDatar->length, pRegisterDatar->data));

		if (pDevContext->Bar1Length >= (pRegisterDatar->offset + pRegisterDatar->length))
		{
			ptr = (unsigned int*)((unsigned long long)pDevContext->Bar1VirtualAddress + pRegisterDatar->offset);
			*ptr = pRegisterDatar->data;
			//ptr = (unsigned int*)((unsigned long)pDevContext->Bar1VirtualAddress + pRegisterDatar->offset);
			//KdPrint((_DRIVER_NAME_"write Bar2  ptr:%p  data:%x \n", ptr, ptr[0]));
			//KdPrint((_DRIVER_NAME_":write virtualAddr：%llx,write length:%d data:%x\n", ptr, pRegisterDatar->length, pRegisterDatar->data));
			//RtlMoveMemory(ptr, &pRegisterDatar->data, pRegisterDatar->length);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(REGISTER_DATA_ITEM));
		//DebugPrint(LOUD, "mark - IOCTL_IOCTL_WRITE_REGISTER2");

	}
	break;
	case IOCTL_GET_BAR_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_GET_BAR_POINTER.");
#if 0
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(IO_MEMORY),
			&outBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %!STATUS!",
				status));
			break;
		}

		PIO_MEMORY registerTmp = (PIO_MEMORY)outBuffer;

		RtlZeroMemory(registerTmp, sizeof(IO_MEMORY));
		registerTmp->bar[0].phyAddress = pDevContext->Bar0PhysicalAddress;
		registerTmp->bar[0].virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->pMdl0,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
		registerTmp->bar[0].size = pDevContext->Bar0Length;
		KdPrint((_DRIVER_NAME_":bar 0 usrviraddr：%x ", registerTmp->bar[0].virAddress));
		registerTmp->bar[2].phyAddress = pDevContext->Bar1PhysicalAddress;
		/*Register->bar[2].vir_address = MmMapLockedPagesSpecifyCache(
			pDevContext->pMdl1,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);*/


		/*try {
			registerTmp->bar[2].vir_address = MmMapLockedPagesSpecifyCache(
			pDevContext->pMdl1,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
			}
			except (EXCEPTION_EXECUTE_HANDLER) {
			DbgPrint("error code");
			}*/
		registerTmp->bar[2].size = pDevContext->Bar1Length;

		WdfRequestCompleteWithInformation(Request, status, sizeof(IO_MEMORY));
#endif

		HANDLE hIOThread;

		status = 0;
		pDevContext->Request = Request;
		//PsCreateSystemThread(&hIOThread, 0, NULL, NtCurrentProcess(), NULL, GetIOMemoryBufferThread, pDevContext);
		DebugPrint(LOUD, "mark - IOCTL_GET_BAR_POINTER.");

	}
	break;
	case IOCTL_RELEASE_BAR_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_RELEASE_BAR_POINTER.");
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(IO_MEMORY),
			&inBuffer,
			NULL
			);
		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %x !",
				status));
			break;
		}

		PIO_MEMORY registerTmp = (PIO_MEMORY)inBuffer;
		if (registerTmp->bar[0].virAddress)
		{
			MmUnmapLockedPages(registerTmp->bar[0].virAddress, pDevContext->pMdl0);
		}

		if (registerTmp->bar[2].virAddress)
		{
			MmUnmapLockedPages(registerTmp->bar[2].virAddress, pDevContext->pMdl1);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(IO_MEMORY));
		DebugPrint(LOUD, "mark - IOCTL_RELEASE_BAR_POINTER.");

	}
	break;
	case IOCTL_SET_EVENT:
	{

		DebugPrint(LOUD, "IOCTL_SET_EVENT.");
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(HANDLE),
			&inBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %xSTATUS!",
				status));
			break;
		}
		//把传递进来的用户层等待事件取出来
		//HANDLE hUserEvent = *(HANDLE *)pIrp->AssociatedIrp.SystemBuffer;
		HANDLE hUserEvent = *(HANDLE *)inBuffer;
		//将用户层事件转化为内核等待对象
		status = ObReferenceObjectByHandle(hUserEvent, EVENT_MODIFY_STATE,
			*ExEventObjectType, KernelMode, (PVOID*)&pDevContext->pEvent, NULL);

		KdPrint(("status = %d\n", status));//status应该为0才对

		ObDereferenceObject(pDevContext->pEvent);

		WdfRequestCompleteWithInformation(Request, status, sizeof(HANDLE));
		DebugPrint(LOUD, "mark - IOCTL_SET_EVENT.");

	}
	break;
	case IOCTL_CLEAR_EVENT:
	{
		status = 0;
		DebugPrint(LOUD, "IOCTL_CLEAR_EVENT.");

		//add by ty
		pDevContext->pEvent = NULL;
		pDevContext->intCount = 0;
		
		WdfRequestCompleteWithInformation(Request, status, sizeof(HANDLE));
		DebugPrint(LOUD, "mark - IOCTL_CLEAR_EVENT.");

	}
	break;

	case IOCTL_MAP_WRITE_BUFFER:
	{
		DebugPrint(LOUD, "IOCTL_MAP_WRITE_BUFFER.");
#if 1
		DMA_BUFFER_ITEM* txBufferItem1;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(DMA_BUFFER_ITEM),
			&outBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %!STATUS!",
				status));
			break;
		}

		KIRQL curkirql = 0;
		KIRQL oldkirql = 0;
		KIRQL irqlTmp = 0;
		curkirql = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));
		if (curkirql > APC_LEVEL)
		{ //若当前IRQL过高则调低到APC_LEVEL
			KeLowerIrql(APC_LEVEL);
		}

		irqlTmp = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", irqlTmp));

		txBufferItem1 = (DMA_BUFFER_ITEM*)outBuffer;
		txBufferItem1->virAddress = MmMapLockedPagesSpecifyCache(
			pDevContext->WriteMdl,
			UserMode,
			MmCached,
			NULL,
			0,
			NormalPagePriority
			);
		KeRaiseIrql(curkirql, &oldkirql); //恢复场地
		txBufferItem1->length = pDevContext->WriteCommonBufferSize;
		txBufferItem1->phyAddress = pDevContext->WriteCommonBufferBaseLA.u.LowPart;
		txBufferItem1->phyAddressH = pDevContext->WriteCommonBufferBaseLA.u.HighPart;
		WdfRequestCompleteWithInformation(Request, status, sizeof(DMA_BUFFER_ITEM));
#endif
	}

#if 0
	HANDLE hWriteThread;

	status = 0;
	pDevContext->Request = Request;

	KIRQL curkirql = 0;
	curkirql = KeGetCurrentIrql(); //获取当前IRQL
	KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));

    PsCreateSystemThread(&hWriteThread, 0, NULL, NtCurrentProcess(), NULL, GetWriteBufferThread, pDevContext);
	DebugPrint(LOUD, "mark - IOCTL_MAP_WRITE_BUFFER.");
#endif
	break;
	case IOCTL_MAP_READ_BUFFER:
	{
		DebugPrint(LOUD, "IOCTL_MAP_READ_BUFFER.");
#if 1 //实现映射的一种方法，但有可能存在隐患
		DMA_BUFFER_ITEM* rxBufferItem1;
		status = WdfRequestRetrieveOutputBuffer(
			Request,
			sizeof(DMA_BUFFER_ITEM),
			&outBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveOutputBuffer failed %!STATUS!",
				status));
			break;
		}

		KIRQL curkirql = 0;
		KIRQL oldkirql = 0;
		KIRQL irqlTmp = 0;
		curkirql = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));
		if (curkirql > APC_LEVEL)
		{ //若当前IRQL过高则调低到APC_LEVEL
			KeLowerIrql(APC_LEVEL);
		}

		irqlTmp = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", irqlTmp));

		rxBufferItem1 = (DMA_BUFFER_ITEM*)outBuffer;
		
			rxBufferItem1->virAddress = MmMapLockedPagesSpecifyCache(
				pDevContext->ReadMdl,
				UserMode,
				MmCached,
				NULL,
				0,
				NormalPagePriority
				);

		KeRaiseIrql(curkirql, &oldkirql); //恢复场地
		rxBufferItem1->length = pDevContext->ReadCommonBufferSize;
		rxBufferItem1->phyAddress = pDevContext->ReadCommonBufferBaseLA.u.LowPart;
		rxBufferItem1->phyAddressH = pDevContext->ReadCommonBufferBaseLA.u.HighPart;
		WdfRequestCompleteWithInformation(Request, status, sizeof(DMA_BUFFER_ITEM));
#endif
#if 0
		HANDLE hReadThread;

		status = 0;
		pDevContext->Request = Request;
		PsCreateSystemThread(&hReadThread, 0, NULL, NtCurrentProcess(), NULL, GetReadBufferThread, pDevContext);
		//status = MasterCardGetReadBufWork(pDevContext, Request); //使用workitem方法，应用层获取到的地址不是实际想要的地址，work进程和应用程序不是同一个进程
		//WdfRequestCompleteWithInformation(Request, status, sizeof(DMA_BUFFER_ITEM)); //在线程中完成IO处理
		DebugPrint(LOUD, "mark - IOCTL_MAP_READ_BUFFER.");
#endif
	}
		break;
	case IOCTL_RELEASE_WRITE_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_RELEASE_WRITE_POINTER.");
		void** txBufferViraddr;
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(void*),
			&inBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %xSTATUS!",
				status));
			break;
		}
		//yyj23/0719
		KIRQL curkirql = 0;
		KIRQL oldkirql = 0;
		KIRQL irqlTmp = 0;
		curkirql = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));
		if (curkirql > APC_LEVEL)
		{ //若当前IRQL过高则调低到APC_LEVEL
			KeLowerIrql(APC_LEVEL);
		}

		irqlTmp = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", irqlTmp));
		//yyj23/0719

		txBufferViraddr = (void**)inBuffer;
		if (*txBufferViraddr)
		{
			MmUnmapLockedPages(*txBufferViraddr, pDevContext->WriteMdl);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(void*));
		DebugPrint(LOUD, "mark - IOCTL_RELEASE_WRITE_POINTER.");

	}
		break;
	case IOCTL_RELEASE_READ_POINTER:
	{
		DebugPrint(LOUD, "IOCTL_RELEASE_READ_POINTER.");
		void** rxBufferViraddr;
		status = WdfRequestRetrieveInputBuffer(
			Request,
			sizeof(void*),
			&inBuffer,
			NULL
			);

		if (!NT_SUCCESS(status))
		{
			KdPrint((_DRIVER_NAME_"WdfRequestRetrieveInputBuffer failed %xSTATUS!",
				status));
			break;
		}
		//yyj23/0719
		KIRQL curkirql = 0;
		KIRQL oldkirql = 0;
		KIRQL irqlTmp = 0;
		curkirql = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", curkirql));
		if (curkirql > APC_LEVEL)
		{ //若当前IRQL过高则调低到APC_LEVEL
			KeLowerIrql(APC_LEVEL);
		}

		irqlTmp = KeGetCurrentIrql(); //获取当前IRQL
		KdPrint((_DRIVER_NAME_"KeGetCurrentIrql irq %d\n", irqlTmp));
		//yyj23/0719


		rxBufferViraddr = (void**)inBuffer;
		if (*rxBufferViraddr)
		{
			MmUnmapLockedPages(*rxBufferViraddr, pDevContext->ReadMdl);
		}

		WdfRequestCompleteWithInformation(Request, status, sizeof(void*));
		DebugPrint(LOUD, "mark - IOCTL_RELEASE_READ_POINTER.");

	}
		break;
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		WdfRequestCompleteWithInformation(Request, status, 0);
		DebugPrint(LOUD, "mark - ERROR.");

		break;
	}

Exit:
	/*KdPrint((_DRIVER_NAME_"ioctl status %x",
		status));*/
	if (!NT_SUCCESS(status)) {
		WdfRequestCompleteWithInformation(
			Request,
			status,
			0
			);

	}

    return;
}

VOID
MasterCardEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.
    //

    return;
}

