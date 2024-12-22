/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/
#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, MasterCardCreateDevice)
#pragma alloc_text(PAGE, MasterCardEvtDevicePrepareHardware)
#pragma alloc_text(PAGE, MasterCardEvtDeviceReleaseHardware)
#pragma alloc_text(PAGE, MasterCardEvtDeviceD0Entry)
#pragma alloc_text(PAGE, MasterCardEvtDeviceD0Exit)
#endif

VOID
NICGetDeviceInfSettings(
__in  PDEVICE_CONTEXT   FdoData
)
{
	//
	// Find out whether the driver is installed as a miniport.
	//


	PciDrvReadRegistryValue(
		FdoData,
		L"TxBufferSize",
		&FdoData->TxBufferSize
		);
	if (!FdoData->TxBufferSize)
		FdoData->TxBufferSize = 0x200000;

	PciDrvReadRegistryValue(
		FdoData,
		L"RxBufferSize",
		&FdoData->RxBufferSize
		);

	if (!FdoData->RxBufferSize)
		FdoData->RxBufferSize = 0x400000;
}

#if 1
NTSTATUS
MasterCardInitializeDMA(IN PDEVICE_CONTEXT DevExt)
{
#if 1
	NTSTATUS    status;
	WDF_OBJECT_ATTRIBUTES attributes;
	ULONG       dteCount;

	PAGED_CODE();

	DevExt->MaximumTransferLength = 8 * 1024;
	//
	// PLx PCI9656 DMA_TRANSFER_ELEMENTS must be 16-byte aligned
	//
	WdfDeviceSetAlignmentRequirement( DevExt->Device,
		FILE_OCTA_ALIGNMENT);
	KdPrint((_DRIVER_NAME_"WdfDeviceSetAlignmentRequirement success"));
	//
	// Create a new DMA Enabler instance.
	// Use Scatter/Gather, 64-bit Addresses, Duplex-type profile.
	//
	{
		WDF_DMA_ENABLER_CONFIG   dmaConfig;

		WDF_DMA_ENABLER_CONFIG_INIT( &dmaConfig,
			WdfDmaProfileScatterGather64Duplex,
			DevExt->MaximumTransferLength);

		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
			" - The DMA Profile is WdfDmaProfileScatterGather64Duplex");

		//
		// Opt-in to DMA version 3, which is required by
		// WdfDmaTransactionSetSingleTransferRequirement
		//
		//dmaConfig.WdmDmaVersionOverride = 3;

		//创建DMA启用程序对象
		status = WdfDmaEnablerCreate(DevExt->Device,
			&dmaConfig,
			WDF_NO_OBJECT_ATTRIBUTES,
			&DevExt->DmaEnabler);

		if (!NT_SUCCESS(status)) {

			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"WdfDmaEnablerCreate failed: %!STATUS!", status);
			return status;
		}

		KdPrint((_DRIVER_NAME_"WdfDmaEnablerCreate success"));

		/*
		dteCount = BYTES_TO_PAGES(((ULONG)
			ROUND_TO_PAGES(MDL_BUFFER_SIZE) + PAGE_SIZE) * MDL_CHAIN_LENGTH) + 1;
		//
		// Set the number of DMA_TRANSFER_ELEMENTs (DTE) to be available.
		//
		DevExt->WriteTransferElements = dteCount;
		DevExt->ReadTransferElements = dteCount;

		KdPrint((_DRIVER_NAME_"WriteTransferElements:%d  ReadTransferElements:%d",
			DevExt->WriteTransferElements, DevExt->ReadTransferElements));
		//
		// The PLX device does not have a hard limit on the number of DTEs
		// it can process for each DMA operation. However, the DTEs are written
		// to common buffers whose size is the effective upper bound to the
		// length of the Scatter/Gather lists we can work with.
		//
		WdfDmaEnablerSetMaximumScatterGatherElements(
			DevExt->DmaEnabler,
			min(DevExt->WriteTransferElements, DevExt->ReadTransferElements));
			*/
	}

	//
	// Allocate common buffer for building writes
	//
	// NOTE: This common buffer will not be cached.
	//       Perhaps in some future revision, cached option could
	//       be used. This would have faster access, but requires
	//       flushing before starting the DMA in PLxStartWriteDma.
	//
	/*DevExt->WriteCommonBufferSize =
		sizeof(DMA_TRANSFER_ELEMENT) * DevExt->WriteTransferElements;*/

	//分配写缓冲区
	DevExt->WriteCommonBufferSize = 0x100000;//0x100000
	//_Analysis_assume_(DevExt->WriteCommonBufferSize > 0);  //wzx
	status = WdfCommonBufferCreate(DevExt->DmaEnabler,
		DevExt->WriteCommonBufferSize,	
		WDF_NO_OBJECT_ATTRIBUTES,
		&DevExt->WriteCommonBuffer);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"WdfCommonBufferCreate (write) failed: %!STATUS!", status);
		return status;
	}

	//获取物理地址和虚拟地址
	DevExt->WriteCommonBufferBase =
		WdfCommonBufferGetAlignedVirtualAddress(DevExt->WriteCommonBuffer);

	DevExt->WriteCommonBufferBaseLA =
		WdfCommonBufferGetAlignedLogicalAddress(DevExt->WriteCommonBuffer);

	RtlZeroMemory(DevExt->WriteCommonBufferBase,
		DevExt->WriteCommonBufferSize);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
		"WriteCommonBuffer 0x%p  (#0x%I64X), length %I64d",
		DevExt->WriteCommonBufferBase,
		DevExt->WriteCommonBufferBaseLA.QuadPart,
		WdfCommonBufferGetLength(DevExt->WriteCommonBuffer));

	KdPrint((_DRIVER_NAME_"MARKMARKDevExt->WriteCommonBufferBaseLA.LowPart:%x", DevExt->WriteCommonBufferBaseLA.u.LowPart));
	KdPrint((_DRIVER_NAME_"MARKMARKDevExt->WriteCommonBufferBaseLA.HighPart:%x", DevExt->WriteCommonBufferBaseLA.u.HighPart));
	//KdPrint((_DRIVER_NAME_"MARKMARKDevExt->WriteCommonBufferBaseLA.HighPart:%x", DevExt->WriteCommonBufferBase));

	//
	// Allocate common buffer for building reads
	//
	// NOTE: This common buffer will not be cached.
	//       Perhaps in some future revision, cached option could
	//       be used. This would have faster access, but requires
	//       flushing before starting the DMA in PLxStartReadDma.
	//
	/*DevExt->ReadCommonBufferSize =
		sizeof(DMA_TRANSFER_ELEMENT) * DevExt->ReadTransferElements;*/

	DevExt->ReadCommonBufferSize = 0x200000;//0x200000


	_Analysis_assume_(DevExt->ReadCommonBufferSize > 0);
	status = WdfCommonBufferCreate(DevExt->DmaEnabler,
		DevExt->ReadCommonBufferSize,
		WDF_NO_OBJECT_ATTRIBUTES,
		&DevExt->ReadCommonBuffer);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"WdfCommonBufferCreate (read) failed %!STATUS!", status);
		return status;
	}

	DevExt->ReadCommonBufferBase =
		
		WdfCommonBufferGetAlignedVirtualAddress(DevExt->ReadCommonBuffer);

	DevExt->ReadCommonBufferBaseLA =
		WdfCommonBufferGetAlignedLogicalAddress(DevExt->ReadCommonBuffer);

	RtlZeroMemory(DevExt->ReadCommonBufferBase,
		DevExt->ReadCommonBufferSize);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
		"ReadCommonBuffer  0x%p  (#0x%I64X), length %I64d",
		DevExt->ReadCommonBufferBase,
		DevExt->ReadCommonBufferBaseLA.QuadPart,
		WdfCommonBufferGetLength(DevExt->ReadCommonBuffer));

	KdPrint((_DRIVER_NAME_"WriteCommonBufferSize:%x  ReadCommonBufferSize:%x",
		DevExt->WriteCommonBufferSize, DevExt->ReadCommonBufferSize));

	DevExt->WriteMdl = IoAllocateMdl(
		DevExt->WriteCommonBufferBase,
		DevExt->WriteCommonBufferSize,
		FALSE,
		FALSE,
		NULL
		);
	if (NULL == DevExt->WriteMdl)
	{
		KdPrint((_DRIVER_NAME_" IoAllocateMdl Write failed\n"));
	}

	DevExt->ReadMdl = IoAllocateMdl(
		DevExt->ReadCommonBufferBase,
		DevExt->ReadCommonBufferSize,
		FALSE,
		FALSE,
		NULL
		);
	if (NULL == DevExt->ReadMdl)
	{
		KdPrint((_DRIVER_NAME_" IoAllocateMdl read failed\n"));
	}

	MmBuildMdlForNonPagedPool(DevExt->WriteMdl);
	MmBuildMdlForNonPagedPool(DevExt->ReadMdl);
	RtlZeroMemory(DevExt->WriteCommonBufferBase, DevExt->WriteCommonBufferSize);
	RtlZeroMemory(DevExt->ReadCommonBufferBase, DevExt->ReadCommonBufferSize);
#if 0   //mark
	//
	// Since we are using sequential queue and processing one request
	// at a time, we will create transaction objects upfront and reuse
	// them to do DMA transfer. Transactions objects are parented to
	// DMA enabler object by default. They will be deleted along with
	// along with the DMA enabler object. So need to delete them
	// explicitly.
	//
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);

	status = WdfDmaTransactionCreate(DevExt->DmaEnabler,
		&attributes,
		&DevExt->ReadDmaTransaction);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
			"WdfDmaTransactionCreate(read) failed: %!STATUS!", status);
		return status;
	}

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
	//
	// Create a new DmaTransaction.
	//
	status = WdfDmaTransactionCreate(DevExt->DmaEnabler,
		&attributes,
		&DevExt->WriteDmaTransaction);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
			"WdfDmaTransactionCreate(write) failed: %!STATUS!", status);
		return status;
	}
#endif

	return status;
#endif
}
#endif
NTSTATUS
MasterCardCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;
	ULONG   length;
	ULONG	propertyAddress;

	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

	//const GUID *GUID_DEVINTERFACE_MasterCard;
    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);


	//采用WdfDeviceIoDirect方式
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);//WdfDeviceIoBuffered???重要吗？

	//初始化即插即用和电源管理例程配置结构
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	//设置即插即用基本例程   123
	pnpPowerCallbacks.EvtDevicePrepareHardware = MasterCardEvtDevicePrepareHardware; //获得内存资源、内存物理地址与虚拟地址的映射
	pnpPowerCallbacks.EvtDeviceReleaseHardware = MasterCardEvtDeviceReleaseHardware; //取消物理地址和虚拟地址映射，释放资源
	pnpPowerCallbacks.EvtDeviceD0Entry = MasterCardEvtDeviceD0Entry; //设备进入工作状态后调用，一般不需要处理任何任务
	pnpPowerCallbacks.EvtDeviceD0Exit = MasterCardEvtDeviceD0Exit; //离开工作状态后调用

	//注册即插即用和电源管理例程
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	deviceAttributes.EvtCleanupCallback = MasterCardEvtDeviceCleanup;
	deviceAttributes.SynchronizationScope = WdfSynchronizationScopeDevice;

	//创建设备对象
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {

        //
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. DeviceGetContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        //
        deviceContext = DeviceGetContext(device);
		DebugPrint(LOUD, "WdfDeviceCreate success.");
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
			"PDO 0x%p, FDO 0x%p, DevExt 0x%p",
			WdfDeviceWdmGetPhysicalDevice(device),
			WdfDeviceWdmGetDeviceObject(device), deviceContext);

		//
		// save the device object we created as our physical device object
		//
		deviceContext->PhysicalDeviceObject = \
			WdfDeviceWdmGetPhysicalDevice(device);

		if (NULL == deviceContext->PhysicalDeviceObject)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceWdmGetPhysicalDevice: NULL DeviceObject\n");
			return STATUS_UNSUCCESSFUL;
		}

		//
		// This is our default IoTarget representing the deviceobject
		// we are attached to.
		//
		deviceContext->StackIoTarget = WdfDeviceGetIoTarget(device);
		deviceContext->StackDeviceObject = WdfDeviceWdmGetAttachedDevice(device);

		if (NULL == deviceContext->StackDeviceObject)
		{
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceWdmGetAttachedDevice: NULL DeviceObject\n");
			return STATUS_UNSUCCESSFUL;
		}
        
		KdPrint((_DRIVER_NAME_"PDO 0x%p, FDO 0x%p, stack 0x%p.\n", deviceContext->PhysicalDeviceObject, WdfDeviceWdmGetDeviceObject(device), deviceContext->StackDeviceObject));
        // Initialize the context.
        deviceContext->PrivateDeviceData = 0;
		deviceContext->Device= device;

        //
        // Create a device interface so that applications can find and talk
        // to us.

		//创建驱动程序与应用程序接口
        status = WdfDeviceCreateDeviceInterface(
            device,
			&GUID_DEVINTERFACE_MasterCard,
            NULL // ReferenceString
            );

        if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"<-- DeviceCreateDeviceInterface "
				"failed %!STATUS!", status);
			return status;
		}
		
#if 1
		// 
		// Get the BusNumber. Please read the warning to follow.
		/// 
		status = IoGetDeviceProperty(
			deviceContext->PhysicalDeviceObject,
			DevicePropertyBusNumber,
			sizeof(ULONG),
			(PVOID)&(deviceContext->BusNumber),
			&length
			);
		if (!NT_SUCCESS(status)){
			KdPrint((_DRIVER_NAME_"Get DevicePropertyBusNumber failed: 0x%x\n",
				status));
			return status;
		}

		// 
		// Get the DevicePropertyAddress
		// 
		status = IoGetDeviceProperty(
			deviceContext->PhysicalDeviceObject,
			DevicePropertyAddress,
			sizeof(ULONG),
			(PVOID)&propertyAddress,
			&length
			);
		if (!NT_SUCCESS(status)){
			KdPrint((_DRIVER_NAME_"Get DevicePropertyAddress failed: 0x%x\n",
				status));
			return status;
		}

		deviceContext->FunctionNumber = (USHORT)((propertyAddress)& 0x0000FFFF);
		deviceContext->DeviceNumber = (USHORT)(((propertyAddress) >> 16) & 0x0000FFFF);

		KdPrint((_DRIVER_NAME_"bus:%d func:%d device:%d\n", deviceContext->BusNumber, deviceContext->FunctionNumber, deviceContext->DeviceNumber));
#endif
		//
		// Initialize the I/O Package and any Queues
		status = MasterCardQueueInitialize(device);
		//
		// Create a WDFINTERRUPT object.
		//
		deviceContext->intCount = 0;
		status = MasterCardInterruptCreate(deviceContext);

		if (!NT_SUCCESS(status)) {
			return status;
		}
		KdPrint((_DRIVER_NAME_" MasterCardInterruptCreate success\n"));

		status = MasterCardInitializeDMA(deviceContext);

		if (!NT_SUCCESS(status)) {
		return status;
		}

		//wzx add
		USHORT PCICR = 0;
		HalGetBusDataByOffset(PCIConfiguration, deviceContext->BusNumber, 2, &PCICR, 4, 2);

		PCICR |= 0x4;
		HalSetBusDataByOffset(PCIConfiguration, deviceContext->BusNumber, 2, &PCICR, 4, 2);

		PCICR = 0;
		HalGetBusDataByOffset(PCIConfiguration, deviceContext->BusNumber, 2, &PCICR, 4, 2);
		KdPrint((_DRIVER_NAME_" HalGetBusDataByOffset PCICR:%x \n", PCICR));

    }
    return status;
}

_Use_decl_annotations_
VOID
MasterCardEvtDeviceCleanup(
_In_ WDFOBJECT Device
)
/*++

Routine Description:

Invoked before the device object is deleted.

Arguments:

Device - A handle to the WDFDEVICE

Return Value:

None

--*/
{
	PDEVICE_CONTEXT devExt;

	PAGED_CODE();

	devExt = DeviceGetContext((WDFDEVICE)Device);

	MasterCardCleanupDeviceExtension(devExt);
}

VOID
MasterCardCleanupDeviceExtension(
	_In_ PDEVICE_CONTEXT DevExt
	)
	/*++

	Routine Description:

	Frees allocated memory that was saved in the
	WDFDEVICE's context, before the device object
	is deleted.

	Arguments:

	DevExt - Pointer to our DEVICE_EXTENSION

	Return Value:

	None

	--*/
{
#ifdef SIMULATE_MEMORY_FRAGMENTATION
	if (DevExt->WriteMdlChain) {
		DestroyMdlChain(DevExt->WriteMdlChain);
		DevExt->WriteMdlChain = NULL;
	}

	if (DevExt->ReadMdlChain) {
		DestroyMdlChain(DevExt->ReadMdlChain);
		DevExt->ReadMdlChain = NULL;
	}
#else
	UNREFERENCED_PARAMETER(DevExt);
#endif
}

NTSTATUS
AllocDMABuffer(
__in PDEVICE_CONTEXT     FdoData
)
{
	int i;
	unsigned int step;

	FdoData->TxBufferSize = 0x100000;
	FdoData->RxBufferSize = 0x100000;

	step = FdoData->TxBufferSize / 10;
	for (i = 0; i < 5; i++)
	{
		FdoData->TxDMABuffer = FdoData->AllocateCommonBuffer(
			FdoData->DmaAdapterObject,
			FdoData->TxBufferSize,
			&FdoData->TxDMABusAddr,
			FALSE
			);
		if (FdoData->TxDMABuffer)
			break;

		FdoData->TxBufferSize -= step;
	}
	if (NULL == FdoData->TxDMABuffer)
		goto CleanUp;

	step = FdoData->RxBufferSize / 10;
	for (i = 0; i < 5; i++)
	{
		FdoData->RxDMABuffer = FdoData->AllocateCommonBuffer(
			FdoData->DmaAdapterObject,
			FdoData->RxBufferSize,
			&FdoData->RxDMABusAddr,
			FALSE
			);
		if (FdoData->RxDMABuffer)
			break;

		FdoData->RxBufferSize -= step;
	}
	if (NULL == FdoData->RxDMABuffer)
		goto CleanUp;

	FdoData->TxBufMdl = IoAllocateMdl(
		FdoData->TxDMABuffer,
		FdoData->TxBufferSize,
		FALSE,
		FALSE,
		NULL
		);
	if (NULL == FdoData->TxBufMdl)
		goto CleanUp;

	FdoData->RxBufMdl = IoAllocateMdl(
		FdoData->RxDMABuffer,
		FdoData->RxBufferSize,
		FALSE,
		FALSE,
		NULL
		);
	if (NULL == FdoData->RxBufMdl)
		goto CleanUp;

	MmBuildMdlForNonPagedPool(FdoData->TxBufMdl);
	MmBuildMdlForNonPagedPool(FdoData->RxBufMdl);
	//MmProbeAndLockPages
	RtlZeroMemory(FdoData->TxDMABuffer, FdoData->TxBufferSize);
	RtlZeroMemory(FdoData->RxDMABuffer, FdoData->RxBufferSize);

	KdPrint(("AllocDMABuffer OK! tx:%p, rx:%p!\n", FdoData->TxDMABuffer, FdoData->RxDMABuffer));
	KdPrint(("AllocDMABuffer OK! tx size:%x , rx size:%x\n", FdoData->TxBufferSize, FdoData->RxBufferSize));

	return STATUS_SUCCESS;

CleanUp:
	FreeDMABuffer(FdoData);
	
	return STATUS_INSUFFICIENT_RESOURCES;
}

VOID FreeDMABuffer(__in PDEVICE_CONTEXT FdoData)
{
	if (FdoData->TxDMABuffer)
	{
		(*(FdoData->DmaAdapterObject->DmaOperations->FreeCommonBuffer))(
			FdoData->DmaAdapterObject,
			FdoData->TxBufferSize,
			FdoData->TxDMABusAddr,
			FdoData->TxDMABuffer,
			FALSE
			);
		FdoData->TxDMABuffer = NULL;
	}

	if (FdoData->RxDMABuffer)
	{
		(*(FdoData->DmaAdapterObject->DmaOperations->FreeCommonBuffer))(
			FdoData->DmaAdapterObject,
			FdoData->RxBufferSize,
			FdoData->RxDMABusAddr,
			FdoData->RxDMABuffer,
			FALSE
			);
		FdoData->RxDMABuffer = NULL;
	}

	if (FdoData->TxBufMdl)
	{
		IoFreeMdl(FdoData->TxBufMdl);
		FdoData->TxBufMdl = NULL;
	}

	if (FdoData->RxBufMdl)
	{
		IoFreeMdl(FdoData->RxBufMdl);
		FdoData->RxBufMdl = NULL;
	}
}

NTSTATUS
MasterCardEvtDevicePrepareHardware(
IN WDFDEVICE Device,
IN WDFCMRESLIST ResourceList,
IN WDFCMRESLIST ResourceListTranslated
)
{
	ULONG            i;
	NTSTATUS        status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pDeviceContext;
	ULONG           numberOfBARs = 0;
	DEVICE_DESCRIPTION              deviceDescription;
	ULONG                           MaximumPhysicalMapping;
	PDMA_ADAPTER                    DmaAdapterObject;
	ULONG                           MapRegisters;

	PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;//record the Hareware resource that OS dispatched to PCIe


	/*
	在Windows驱动开发中，PCM_PARTIAL_RESOURCE_DESCRIPTOR记录了为PCI设备分配的硬件资源，
	可能有CmResourceTypePort, CmResourceTypeMemory等，
	后者表示一段memory地址空间，顾名思义，是通过memory space访问的，
	前者表示一段I/O地址空间，但其flag有CM_RESOURCE_PORT_MEMORY和CM_RESOURCE_PORT_IO两种，
	分别表示通过memory space访问以及通过I/O space访问，这就是PCI请求与实际分配的差异，
	在x86下，CmResourceTypePort的flag都是CM_RESOURCE_PORT_IO，即表明PCI设备请求的是I/O地址空间，分配的也是I/O地址空间，
	而在ARM或Alpha等下，flag是CM_RESOURCE_PORT_MEMORY，表明即使PCI请求的I/O地址空间，但分配在了memory space，
	我们需要通过memory space访问I/O设备(通过MmMapIoSpace映射物理地址空间到虚拟地址空间，当然，是内核的虚拟地址空间，这样驱动就可以正常访问设备了)。
	*/
	PAGED_CODE();

	UNREFERENCED_PARAMETER(ResourceList);//告诉编译器不要发出Resources没有被引用的警告


	pDeviceContext = DeviceGetContext(Device);
	pDeviceContext->Counter_i = 0;
	RtlZeroMemory(&pDeviceContext->Register, sizeof(IO_MEMORY));
	//get resource
#if 1
	for (i = 0; i < WdfCmResourceListGetCount(ResourceListTranslated); i++) {

		descriptor = WdfCmResourceListGetDescriptor(ResourceListTranslated, i);
		//if failed：
		if (!descriptor) {
			return STATUS_DEVICE_CONFIGURATION_ERROR;
		}

		switch (descriptor->Type) {

		case CmResourceTypeMemory:
			numberOfBARs++;
			//MmMapIoSpace将物理地址转换成系统内核模式地址
			if (numberOfBARs == 1){
				pDeviceContext->Bar0PhysicalAddress = descriptor->u.Memory.Start.LowPart;
				pDeviceContext->Bar0VirtualAddress = MmMapIoSpace(
					descriptor->u.Memory.Start,
					descriptor->u.Memory.Length,
					MmNonCached);
				pDeviceContext->Bar0Length = descriptor->u.Memory.Length;
				KdPrint((_DRIVER_NAME_":bar 0 phyaddr：%llx .viraddr:% llx length:%ld.]\n", pDeviceContext->Bar0PhysicalAddress, pDeviceContext->Bar0VirtualAddress, pDeviceContext->Bar0Length));

			}

			if (numberOfBARs == 2)
			{
				pDeviceContext->Bar1PhysicalAddress = descriptor->u.Memory.Start.LowPart;
				pDeviceContext->Bar1VirtualAddress = MmMapIoSpace(
					descriptor->u.Memory.Start,
					descriptor->u.Memory.Length,
					MmNonCached);
				pDeviceContext->Bar1Length = descriptor->u.Memory.Length;
				KdPrint((_DRIVER_NAME_":bar 2 phyaddr：%llx .viraddr:% llx length:%ld.\n", pDeviceContext->Bar1PhysicalAddress, pDeviceContext->Bar1VirtualAddress, pDeviceContext->Bar1Length));
			}

			break;
		case CmResourceTypePort:
			numberOfBARs++;
			break;
		case CmResourceTypeInterrupt:
			break;
		default:
			break;
		}
		KdPrint((_DRIVER_NAME_"i:%d type:%d \n", i, descriptor->Type));
		}
	
	int *pData32 = (int*)((unsigned long long)pDeviceContext->Bar1VirtualAddress + 0xf0010);  //mark
	KdPrint((_DRIVER_NAME_":addr:%llx cardtype :%llx\n ", pData32, pData32[0]));

	//pData32 = (int*)((unsigned long long)pDeviceContext->Bar0VirtualAddress + 0x04);
	//*pData32 = pDeviceContext->WriteCommonBufferBaseLA.u.HighPart;
	//KdPrint((_DRIVER_NAME_":MARKMARKMARKaddr:%llx  ", *pData32));
	//pData32 = (int*)((unsigned long long)pDeviceContext->Bar0VirtualAddress + 0x1c);
	//*pData32 = pDeviceContext->ReadCommonBufferBaseLA.u.HighPart;
	//pData32 = (int*)((unsigned long long)pDeviceContext->Bar1VirtualAddress + 0x1800c);  //mark
	//KdPrint((_DRIVER_NAME_":addr:%llx cardtype :%llx ", pData32, pData32[0]));
	//pData32[0] = 1;
	//KdPrint((_DRIVER_NAME_":addr:%llx cardtype :%llx ", pData32, pData32[0]));

	//KIRQL curkirql = 0;
	//curkirql = KeGetCurrentIrql(); //获取当前IRQL

	//KdPrint((_DRIVER_NAME_":irql：%d \n", curkirql));

	//pDeviceContext->Regs = (PPCI9656_REGS)pDeviceContext->Bar0VirtualAddress;    //mark

	PMDL mdl;

	mdl = IoAllocateMdl(                      //分配缓冲区
		pDeviceContext->Bar0VirtualAddress,
		pDeviceContext->Bar0Length,
		FALSE,
		FALSE,
		NULL
		);
	//KdPrint((_DRIVER_NAME_":add bar 0 phyaddr：%llx .viraddr:% llx length:%ld.", pDeviceContext->Bar0PhysicalAddress, pDeviceContext->Bar0VirtualAddress, pDeviceContext->Bar0Length));

	if (mdl)
	{
	    KdPrint((_DRIVER_NAME_":IoAllocateMdl success.\n"));
		MmBuildMdlForNonPagedPool(mdl);
		pDeviceContext->pMdl0 = mdl;
	}

	//mdl = IoAllocateMdl(                      //mark//分配缓冲区
	//	pDeviceContext->Bar1VirtualAddress,
	//	pDeviceContext->Bar1Length,
	//	FALSE,
	//	FALSE,
	//	NULL
	//	);
	//KdPrint((_DRIVER_NAME_":add bar 2 phyaddr：%llx .viraddr:% lx length:%ld.", pDeviceContext->Bar1PhysicalAddress, pDeviceContext->Bar1VirtualAddress, pDeviceContext->Bar1Length));

	//if (mdl)
	//{
	//	//KdPrint((_DRIVER_NAME_":IoAllocateMdl success."));
	//	MmBuildMdlForNonPagedPool(mdl);
	//	pDeviceContext->pMdl1 = mdl;	
	//}

	pDeviceContext->Counter_i = i;
#endif

	//
	// Zero out the entire structure first.
	//
	RtlZeroMemory(&deviceDescription, sizeof(DEVICE_DESCRIPTION));

	//
	// DMA_VER2 is defined when the driver is built to work on XP and
	// above. The difference between DMA_VER2 and VER0 is that VER2
	// support BuildScatterGatherList & CalculateScatterGatherList functions.
	// BuildScatterGatherList performs the same operation as
	// GetScatterGatherList, except that it uses the buffer supplied
	// in the ScatterGatherBuffer parameter to hold the scatter/gather
	// list that it creates. In contrast, GetScatterGatherList
	// dynamically allocates a buffer to hold the scatter/gather list.
	// If insufficient memory is available to allocate the buffer,
	// GetScatterGatherList can fail with a STATUS_INSUFFICIENT_RESOURCES
	// error. Drivers that must avoid this scenario can pre-allocate a
	// buffer to hold the scatter/gather list, and use BuildScatterGatherList
	// instead. A driver can use the CalculateScatterGatherList routine
	// to determine the size of buffer to allocate to hold the
	// scatter/gather list.
	//
#if defined(DMA_VER2)
	deviceDescription.Version = DEVICE_DESCRIPTION_VERSION2;
#else
	deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
#endif

	deviceDescription.Master = TRUE;
	deviceDescription.ScatterGather = TRUE;
	deviceDescription.Dma32BitAddresses = TRUE;
	deviceDescription.Dma64BitAddresses = FALSE;
	deviceDescription.InterfaceType = PCIBus;

	//
	// The maximum length of buffer for maxMapRegistersRequired number of
	// map registers would be.
	//
	MaximumPhysicalMapping = 0x200;
	deviceDescription.MaximumLength = MaximumPhysicalMapping;

	DmaAdapterObject = IoGetDmaAdapter(
		pDeviceContext->PhysicalDeviceObject,
		&deviceDescription,
		&MapRegisters
		);
	if (DmaAdapterObject == NULL)
	{
		DebugPrint(ERROR, "IoGetDmaAdapter failed\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
	}

	pDeviceContext->DmaAdapterObject = DmaAdapterObject;
	//MP_SET_FLAG(FdoData, fMP_ADAPTER_SCATTER_GATHER);

	//
	// For convenience, let us save the frequently used DMA operational
	// functions in our device context.
	//
	pDeviceContext->AllocateCommonBuffer =
		*DmaAdapterObject->DmaOperations->AllocateCommonBuffer;
	pDeviceContext->FreeCommonBuffer =
		*DmaAdapterObject->DmaOperations->FreeCommonBuffer;

	//status = AllocDMABuffer(pDeviceContext);
	
	KdPrint((_DRIVER_NAME_":numberOfBARs：%d.\n", numberOfBARs));
	return STATUS_SUCCESS;
}


NTSTATUS
MasterCardEvtDeviceReleaseHardware(
IN WDFDEVICE Device,
IN WDFCMRESLIST ResourceListTranslated
)
{
	DebugPrint(LOUD, "MasterCardEvtDeviceReleaseHardware start.");
	PDEVICE_CONTEXT    pDeviceContext = NULL;
	PDMA_ADAPTER DmaAdapterObject;

	PAGED_CODE();

	pDeviceContext = DeviceGetContext(Device);

	DmaAdapterObject = pDeviceContext->DmaAdapterObject;

	if (pDeviceContext->Bar0VirtualAddress)
	{
		MmUnmapIoSpace(pDeviceContext->Bar0VirtualAddress, pDeviceContext->Bar0Length);
		pDeviceContext->Bar0VirtualAddress = NULL;
		pDeviceContext->Bar0Length = 0;
	}

	if (pDeviceContext->Bar1VirtualAddress)
	{
		MmUnmapIoSpace(pDeviceContext->Bar1VirtualAddress, pDeviceContext->Bar1Length);
		pDeviceContext->Bar1VirtualAddress = NULL;
		pDeviceContext->Bar1Length = 0;
	}

	FreeDMABuffer(pDeviceContext);  //mark
	if (DmaAdapterObject){
		DmaAdapterObject->DmaOperations->PutDmaAdapter(DmaAdapterObject);
		pDeviceContext->DmaAdapterObject = NULL;
		pDeviceContext->AllocateCommonBuffer = NULL;
		pDeviceContext->FreeCommonBuffer = NULL;
	}

	DebugPrint(LOUD, "EvtDeviceReleaseHardware - ends\n");

	return STATUS_SUCCESS;
}

NTSTATUS
MasterCardEvtDeviceD0Entry(
IN  WDFDEVICE Device,
IN  WDF_POWER_DEVICE_STATE PreviousState
)
{
	DebugPrint(LOUD, "MasterCardEvtDeviceD0Entry start.");
	UNREFERENCED_PARAMETER(Device);
	UNREFERENCED_PARAMETER(PreviousState);

	return STATUS_SUCCESS;
}


NTSTATUS
MasterCardEvtDeviceD0Exit(
IN  WDFDEVICE Device,
IN  WDF_POWER_DEVICE_STATE TargetState
)
{
	DebugPrint(LOUD, "MasterCardEvtDeviceD0Exit start.");
	UNREFERENCED_PARAMETER(Device);
	UNREFERENCED_PARAMETER(TargetState);

	PAGED_CODE();

	return STATUS_SUCCESS;
}

BOOLEAN
PciDrvReadRegistryValue(
_In_  PDEVICE_CONTEXT   FdoData,
_In_  PWSTR       Name,
_Out_ PULONG      Value
)
/*++

Routine Description:

Can be used to read any REG_DWORD registry value stored
under Device Parameter.

Arguments:

FdoData - pointer to the device extension
Name - Name of the registry value
Value -


Return Value:

TRUE if successful
FALSE if not present/error in reading registry

--*/
{
	WDFKEY      hKey = NULL;
	NTSTATUS    status;
	BOOLEAN     retValue = FALSE;
	UNICODE_STRING  valueName;



	PAGED_CODE();

	/*TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT,
		"-->PciDrvReadRegistryValue \n");*/

	*Value = 0;

	status = WdfDeviceOpenRegistryKey(FdoData->Device,
		PLUGPLAY_REGKEY_DEVICE,
		STANDARD_RIGHTS_ALL,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hKey);

	if (NT_SUCCESS(status)) {

		RtlInitUnicodeString(&valueName, Name);

		status = WdfRegistryQueryULong(hKey,
			&valueName,
			Value);

		if (NT_SUCCESS(status)) {
			retValue = TRUE;
		}

		WdfRegistryClose(hKey);
	}

	/*TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT,
		"<--PciDrvReadRegistryValue %ws %d \n", Name, *Value);*/

	return retValue;
}