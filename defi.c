/*
 * Copyright (C) 2021 Gaël PORTAY
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main (
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE   *SystemTable
    )
{
    EFI_INPUT_KEY *Key;

    InitializeLib (ImageHandle, SystemTable);

    Print (L"Hello, World!\n");

    Print (L"Hit any key to continue...\n");
    WaitForSingleEvent (ST->ConIn->WaitForKey, 0);
    uefi_call_wrapper (ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);

    return EFI_SUCCESS;
}
