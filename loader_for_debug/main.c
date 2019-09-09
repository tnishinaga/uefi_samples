#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHAR16_BUF_MAX 512

CHAR16 *APP_PATH = L"hello.efi";


VOID
efi_panic(EFI_STATUS efi_status, INTN line)
{
    Print(L"panic at line:%d\n", line);
    Print(L"EFI_STATUS = %d\n", efi_status);
    while (1);
    // TODO: change while loop to BS->Exit
}

static inline VOID
MY_EFI_ASSERT(EFI_STATUS status, INTN line)
{
    if (EFI_ERROR(status)) {
        efi_panic(status, line);
    }
}

EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *
GetDevicePathFromTextProtocol(VOID)
{
    static EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *DevicePathFromText = NULL;

    if (DevicePathFromText == NULL) {
        Print(L"DevicePathFromText is NULL\n");

        // get EFI_DEVICE_PATH_FROM_TEXT protocol
        MY_EFI_ASSERT(
            uefi_call_wrapper(
                BS->LocateProtocol,
                3,
                &DevicePathFromTextProtocol, /* Protocol */
                NULL, /* Registration */
                (VOID **)&DevicePathFromText
            ),
        __LINE__);
    }
    return DevicePathFromText;
}


EFI_DEVICE_PATH_PROTOCOL *
ConvertTextToDevicePath(CHAR16 *Path)
{
    EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *DevicePathFromText = GetDevicePathFromTextProtocol();
    Print(L"finish DevicePathFromText\n");

    // concatinate devicepath
    EFI_DEVICE_PATH_PROTOCOL *NodePath = uefi_call_wrapper(
        DevicePathFromText->ConvertTextToDevicePath,
        1,
        Path
    );

    return NodePath;
}


EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *
GetDevicePathToTextProtocol(VOID)
{
    static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *DevicePathToText = NULL;

    if (DevicePathToText == NULL) {
        Print(L"DevicePathToText is NULL\n");

        // get EFI_DEVICE_PATH_FROM_TEXT protocol
        MY_EFI_ASSERT(
            uefi_call_wrapper(
                BS->LocateProtocol,
                3,
                &DevicePathToTextProtocol, /* Protocol */
                NULL, /* Registration */
                (VOID **)&DevicePathToText
            ),
        __LINE__);
    }
    return DevicePathToText;
}


CHAR16 *
ConvertDevicePathToText(EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *DevicePathToText = GetDevicePathToTextProtocol();
    CHAR16 *Path = uefi_call_wrapper(
        DevicePathToText->ConvertDevicePathToText,
        3,
        DevicePath, /* Device Path */
        FALSE, /* DisplayOnly */
        FALSE /* AllowShortcuts */
    );
    return Path;
}

EFI_LOADED_IMAGE *
OpenLoadedImageProtocol(EFI_HANDLE Handle, EFI_HANDLE AgentHandle)
{
    static EFI_LOADED_IMAGE *LoadedImage = NULL;
    if (LoadedImage == NULL) {
        MY_EFI_ASSERT(
            uefi_call_wrapper(
                BS->OpenProtocol,
                6,
                Handle, /* Handle */
                &LoadedImageProtocol, /* Protocol */
                (VOID **)&LoadedImage, /* Interface */
                AgentHandle, /* AgentHandle */
                NULL, /* ControllrHandle */
                EFI_OPEN_PROTOCOL_GET_PROTOCOL /* Attributes */
            )
        , __LINE__);
    }
    return LoadedImage;
}

EFI_DEVICE_PATH_PROTOCOL *
GetDevicePathOfThisImage(EFI_HANDLE Handle, EFI_HANDLE AgentHandle)
{
    EFI_LOADED_IMAGE *LoadedImage = OpenLoadedImageProtocol(Handle, AgentHandle);

    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    MY_EFI_ASSERT(
        uefi_call_wrapper(
            BS->OpenProtocol,
            6,
            LoadedImage->DeviceHandle, /* Handle */
            &DevicePathProtocol, /* Protocol */
            (VOID **)&DevicePath, /* Interface */
            AgentHandle, /* AgentHandle */
            NULL, /* ControllrHandle */
            EFI_OPEN_PROTOCOL_GET_PROTOCOL /* Attributes */
        )
    , __LINE__);


    // MY_EFI_ASSERT(
    //     uefi_call_wrapper(
    //         BS->CloseProtocol,
    //         4,
    //         LoadedImageProtocol->DeviceHandle, /* Handle */
    //         &gEfiDevicePathProtocolGuid, /* Protocol */
    //         ImageHandle, /* AgentHandle */
    //         NULL, /* ControllrHandle */
    //     )
    // , __LINE__);

    // MY_EFI_ASSERT(
    //     uefi_call_wrapper(
    //         BS->CloseProtocol,
    //         4,
    //         ImageHandle, /* Handle */
    //         &gEfiLoadedImageProtocolGuid, /* Protocol */
    //         ImageHandle, /* AgentHandle */
    //         NULL, /* ControllrHandle */
    //     )
    // , __LINE__);

    return DevicePath;
}

inline VOID
WaitDebuggerAArch64(VOID *ImageBase)
{
    // Wait to connect from debugger
    // Please read EntryPointAddress from x2
    // and set x0 = 1 to break loop
    __asm__ volatile (
        "mov x0, xzr;"
        "mov x1, %[iamge_base];"
        "loop:;"
        "cbz x0, loop;"
        :
        : [iamge_base] "r" (ImageBase)
        : "x0", "x1"
    );
}


EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Loader Start!\n");

    
    // gnu-efi cannot use EFI_DEVICE_PATH_UTILITIES_PROTOCOL
    // So I use FileDevicePath that is provided from gnu-efi
    EFI_LOADED_IMAGE *LoadedImage = OpenLoadedImageProtocol(ImageHandle, ImageHandle);
    EFI_DEVICE_PATH_PROTOCOL *NextDevicePath = 
        FileDevicePath(LoadedImage->DeviceHandle, APP_PATH);
    Print(L"Path: %s\n", ConvertDevicePathToText(NextDevicePath));


    BOOLEAN BootPolicy = FALSE;
    EFI_HANDLE ParentImageHandle = ImageHandle;
    VOID *SourceBuffer = NULL;
    UINTN SourceSize = 0;

    EFI_HANDLE NextImage;
    {
        EFI_STATUS result = uefi_call_wrapper(
                BS->LoadImage,
                6,
                BootPolicy,
                ParentImageHandle,
                NextDevicePath,
                SourceBuffer,
                SourceSize,
                &NextImage
        );
        switch (result) {
            case EFI_SUCCESS:
                break;
            case EFI_NOT_FOUND:
                Print(L"Error: %s not found\n", ConvertDevicePathToText(NextDevicePath));
                MY_EFI_ASSERT(result, __LINE__);
                break;
            default:
                MY_EFI_ASSERT(result, __LINE__);
        }
    }

    Print(L"Finish LoadImage\n");

    Print(L"Show entry point\n");
    EFI_LOADED_IMAGE *NextLoadedImage = OpenLoadedImageProtocol(NextImage, ImageHandle);
    Print(L"current entry: 0x%016lx\n", LoadedImage->ImageBase);
    Print(L"next entry: 0x%016lx\n", NextLoadedImage->ImageBase);
    WaitDebuggerAArch64(NextLoadedImage->ImageBase);


    MY_EFI_ASSERT(
        uefi_call_wrapper(
            BS->StartImage,
            3,
            NextImage,
            NULL,
            NULL
        )
    , __LINE__);

    Print(L"Finish StartImage\n");

	return EFI_SUCCESS;
}
