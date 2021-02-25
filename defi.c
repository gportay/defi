/*
 * Copyright (C) 2021 Gaël PORTAY
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <efi.h>
#include <efilib.h>

/*
 * EFI Load Options
 * UEFI Specification Version 2.8 Section 3.1.3
 *
 * Byte packed structure of variable length fields.
 */

typedef struct _EFI_LOAD_OPTION {
    UINT32                      Attributes;
    UINT16                      FilePathListLength;
    // CHAR16                   Description[];
    // EFI_DEVICE_PATH_PROTOCOL FilePathList[];
    // UINT8                    OptionalData[];
} EFI_LOAD_OPTION;

static
CHAR16 *
LoadOptionDescription (
    IN CONST EFI_LOAD_OPTION *LoadOption
    )
{
    UINTN Size = 0;

    Size += sizeof(LoadOption->Attributes);
    Size += sizeof(LoadOption->FilePathListLength);
    return (CHAR16 *)((VOID *)LoadOption + Size);
}

static
EFI_DEVICE_PATH_PROTOCOL *
LoadOptionFilePathList (
    IN CONST EFI_LOAD_OPTION *LoadOption
    )
{
    CHAR16 *Description;

    Description = LoadOptionDescription (LoadOption);
    return (EFI_DEVICE_PATH_PROTOCOL *)(Description + StrLen(Description) + 1);
}

/*
 * Device Path to Text Protocol
 * UEFI Specification Version 2.8 Section 10.6.2
 */

EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *DevicePathToText;

static
CHAR16*
ConvertDevicePathToText (
    IN CONST EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
    IN BOOLEAN                           DisplayOnly,
    IN BOOLEAN                           AllowShortcuts
    )
{
    if (!DevicePathToText)
        return NULL;

    return (CHAR16 *)uefi_call_wrapper (DevicePathToText->ConvertDevicePathToText, 3,
                                        DevicePath, DisplayOnly, AllowShortcuts);
}

/*
 * Protocol Handler Services
 * UEFI Specification Version 2.8 Section 7.3
 */

EFI_LOADED_IMAGE *LoadedImage;

static
EFI_STATUS
HandleProtocol (
    IN EFI_HANDLE               Handle,
    IN EFI_GUID                 *Protocol,
    OUT VOID                    **Interface
    )
{
    if (!BS)
        return EFI_LOAD_ERROR;

    return uefi_call_wrapper (BS->HandleProtocol, 3, Handle, Protocol,
                              Interface);
}

/*
 * Globally Defined Variables
 * UEFI Specification Version 2.8 Section 3.3
 */

EFI_GUID GlobalVariable = EFI_GLOBAL_VARIABLE;

/*
 * Internal functions
 */

static
EFI_STATUS
Initialize (
    IN EFI_HANDLE           ImageHandle
    )
{
    EFI_STATUS Status;

    if (!LoadedImage)
    {
        Status = HandleProtocol (ImageHandle, &LoadedImageProtocol,
                                 (VOID*)&LoadedImage);
        if (EFI_ERROR(Status))
            Print (L"Warning: Could not HandleProtocol: %r\n", Status);
    }

    if (!DevicePathToText)
    {
        Status = LibLocateProtocol (&DevicePathToTextProtocol,
                                    (VOID **)&DevicePathToText);
        if (EFI_ERROR(Status))
            Print (L"Warning: Could not LibLocateProtocol: %r\n", Status);
    }

    return EFI_SUCCESS;
}

static
VOID
PrintBootManager ()
{
    UINT16 BootCurrent = 0, Timeout = 0, *BootOrder = NULL, BootOrderSize = 0;
    UINTN i, Size;
    VOID *Value;

    /*
     * Firmware Boot Manager
     * UEFI Specification Version 2.8 Section 3.1
     */

    Value = LibGetVariableAndSize (VarBootCurrent, &GlobalVariable, &Size);
    if (Value)
    {
        BootCurrent = *(UINT16 *)Value;
        FreePool (Value);
    }
    Print (L"BootCurrent: %04x\n", BootCurrent);

    Value = LibGetVariableAndSize (VarTimeout, &GlobalVariable, &Size);
    if (Value)
    {
        Timeout= *(UINT16 *)Value;
        FreePool (Value);
    }
    Print (L"Timeout: %d seconds\n", Timeout);

    Value = LibGetVariableAndSize (VarBootOrder, &GlobalVariable, &Size);
    if (Value)
    {
        BootOrder = (UINT16 *)Value;
        BootOrderSize = Size / sizeof(UINT16);
    }
    Print (L"BootOrder: %04x", BootOrder ? BootOrder[0] : 0);
    for (i = 0; i < BootOrderSize; i++)
        Print (L",%04x", BootOrder[i]);
    Print (L"\n");

    for (i = 0; i < 0xffff; i++)
    {
        EFI_DEVICE_PATH_PROTOCOL *FilePathList;
        CHAR16                   *Description;
        EFI_LOAD_OPTION          *LoadOption;
        CHAR16                   *Str;
        CHAR16                   *Text;

        Str = PoolPrint (VarBootOption, i);
        if (!Str)
        {
            Print (L"Warning: Could not PoolPrint: %r\n", EFI_OUT_OF_RESOURCES);
            continue;
        }

        LoadOption = LibGetVariableAndSize (Str, &GlobalVariable, &Size);
        if (!LoadOption)
        {
            FreePool (Str);
            continue;
        }

        Description = LoadOptionDescription (LoadOption);
        FilePathList = LoadOptionFilePathList (LoadOption);
        Text = ConvertDevicePathToText (&FilePathList[0], TRUE, TRUE);

        Print (L"%s%s %s\t%s\n", Str,
               (LoadOption->Attributes | LOAD_OPTION_ACTIVE) ? L"*" : L"",
               Description, Text);

        if (Text)
            FreePool (Text);
        FreePool (LoadOption);
        FreePool (Str);
    }

    FreePool (BootOrder);
}

EFI_STATUS
EFIAPI
efi_main (
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE   *SystemTable
    )
{
    EFI_INPUT_KEY *Key;

    InitializeLib (ImageHandle, SystemTable);
    Initialize (ImageHandle);

    PrintBootManager ();

    Print (L"Hit any key to continue...\n");
    WaitForSingleEvent (ST->ConIn->WaitForKey, 0);
    uefi_call_wrapper (ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);

    return EFI_SUCCESS;
}
