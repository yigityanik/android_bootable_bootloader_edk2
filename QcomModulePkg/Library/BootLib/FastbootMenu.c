/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "AutoGen.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/FastbootWarningIcon.h>
#include <Library/FastbootKeyIcons.h>
#include <Library/FastbootMenu.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MenuKeysDetection.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UpdateDeviceTree.h>
#include <Library/BootLinux.h>
#include <Protocol/EFIVerifiedBoot.h>
#include <Uefi.h>

#ifndef BOOTLOADER_VERSION
#define BOOTLOADER_VERSION "unknown"
#endif


STATIC OPTION_MENU_INFO gMenuInfo;

STATIC MENU_MSG_INFO mFastbootOptionTitle[] = {
    {{"Resume"},
     BIG_FACTOR,
     BGR_GREEN,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     RESTART},

    {{"Power Off"},
     BIG_FACTOR,
     BGR_RED,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     POWEROFF},

    {{"Emergency Download"},
     BIG_FACTOR,
     BGR_YELLOW,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     EDL},

    {{"Recovery Mode"},
     BIG_FACTOR,
     BGR_PINK,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     RECOVER},

    {{"Restart bootloader"},
     BIG_FACTOR,
     BGR_RED,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     FASTBOOT},
};


/**
  Update the fastboot option item
  @param[in] OptionItem  The new fastboot option item
  @param[out] pLocation  The pointer of the location
  @retval EFI_SUCCESS	 The entry point is executed successfully.
  @retval other		 Some error occurs when executing this entry point.
 **/

STATIC EFI_STATUS
DrawFastbootTextAt (CONST CHAR8 *Text,
                    UINT32 X,
                    UINT32 Y,
                    UINT32 Scale,
                    UINT32 FgColor,
                    UINT32 *TextHeight)
{
  return DrawFastbootBitmapText (
      Text,
      X,
      Y,
      Scale == BIG_FACTOR,
      FgColor,
      TextHeight);
}


EFI_STATUS
UpdateFastbootOptionItem (UINT32 OptionItem, UINT32 *pLocation)
{
  EFI_STATUS Status;
  UINT32 Width = GetScreenWidth ();
  UINT32 Height = GetScreenHeight ();
  UINT32 LabelX;
  UINT32 ValueX;
  UINT32 ArrowX;
  UINT32 Y;
  UINT32 Color;

  if (OptionItem >= ARRAY_SIZE (mFastbootOptionTitle))
    return EFI_INVALID_PARAMETER;

  LabelX = Width * 6 / 100;
  ValueX = Width * 22 / 100;
  ArrowX = Width * 90 / 100;
  Y = Height * 92 / 100;
  Color = mFastbootOptionTitle[OptionItem].FgColor;

  /*
   * Clear only the selected item area.
   */
  FillRect (
      0,
      Height * 91 / 100,
      Width,
      Height * 4 / 100,
      BGR_BLACK);

  Status = DrawFastbootTextAt (
      "Selected:",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_SILVER,
      NULL);

  if (EFI_ERROR (Status))
    return Status;

  Status = DrawFastbootTextAt (
      mFastbootOptionTitle[OptionItem].Msg,
      ValueX,
      Y,
      COMMON_FACTOR,
      Color,
      NULL);

  if (EFI_ERROR (Status))
    return Status;

  Status = DrawFastbootTextAt (
      ">",
      ArrowX,
      Y,
      COMMON_FACTOR,
      Color,
      NULL);

  if (pLocation != NULL)
    *pLocation = Y;

  return Status;
}

/**
  Draw the fastboot menu
  @param[out] OptionMenuInfo  Fastboot option info
  @retval     EFI_SUCCESS     The entry point is executed successfully.
  @retval     other           Some error occurs when executing this entry point.
 **/
STATIC EFI_STATUS
FastbootMenuShowScreen (OPTION_MENU_INFO *OptionMenuInfo)
{
  EFI_STATUS Status;
  UINT32 Width = GetScreenWidth ();
  UINT32 Height = GetScreenHeight ();
  UINT32 LabelX;
  UINT32 ColonX;
  UINT32 ValueX;
  UINT32 Y;
  UINT32 Step;
  UINT32 InstructionY;
  UINT32 InstructionHeight = 0;
  UINT32 DividerY;

  UINT32 VolumeUpY;
  UINT32 VolumeDownY;
  UINT32 PowerY;
  UINT32 LeftArrowX;
  UINT32 RightArrowX;

  UINT32 OptionItem;
  UINT32 i;

  CHAR8 Product[MAX_RSP_SIZE] = "";
  CHAR8 Serial[MAX_RSP_SIZE] = "";
  CHAR8 SlotSuffixAscii[MAX_SLOT_SUFFIX_SZ] = "";

  Slot CurrentSlot;

  ZeroMem (
      &OptionMenuInfo->Info,
      sizeof (MENU_OPTION_ITEM_INFO));

  OptionMenuInfo->Info.MsgInfo =
      mFastbootOptionTitle;

  for (i = 0;
       i < ARRAY_SIZE (mFastbootOptionTitle);
       i++)
    OptionMenuInfo->Info.OptionItems[i] = i;

  OptionMenuInfo->Info.MenuType =
      DISPLAY_MENU_FASTBOOT;

  OptionMenuInfo->Info.OptionNum =
      ARRAY_SIZE (mFastbootOptionTitle);

  FillRect (
      0,
      0,
      Width,
      Height,
      BGR_BLACK);

  /*
   * Physical key indicators.
   *
   * Left:
   *   Volume Up
   *   Volume Down
   *
   * Right:
   *   Power, vertically centered between Volume Up/Down.
   */
  /*
   * Physical button centers are calibrated independently.
   * Use per-mille values for finer positioning than whole percentages.
   */
  VolumeUpY = Height * 245 / 1000;
  VolumeDownY = Height * 315 / 1000;
  PowerY = Height * 230 / 1000;

  /*
   * Keep the icons close to the panel edges.
   * Coordinates below refer to the top-left corner of each
   * 80x80 bitmap; subtract 40 from Y to center the bitmap
   * on the physical button center.
   */
  LeftArrowX = Width * 1 / 100;
  RightArrowX =
      Width - FASTBOOT_KEY_ICON_WIDTH - (Width * 1 / 100);

  DrawFastbootIcon (
      gFastbootArrowLeft,
      FASTBOOT_KEY_ICON_WIDTH,
      FASTBOOT_KEY_ICON_HEIGHT,
      LeftArrowX,
      VolumeUpY - (FASTBOOT_KEY_ICON_HEIGHT / 2));

  DrawFastbootIcon (
      gFastbootArrowLeft,
      FASTBOOT_KEY_ICON_WIDTH,
      FASTBOOT_KEY_ICON_HEIGHT,
      LeftArrowX,
      VolumeDownY - (FASTBOOT_KEY_ICON_HEIGHT / 2));

  DrawFastbootIcon (
      gFastbootArrowRight,
      FASTBOOT_KEY_ICON_WIDTH,
      FASTBOOT_KEY_ICON_HEIGHT,
      RightArrowX,
      PowerY - (FASTBOOT_KEY_ICON_HEIGHT / 2));

  /*
   * Physical key labels.
   *
   * Volume labels sit to the right of the left-side arrows.
   * Start sits to the left of the right-side power arrow.
   */
  DrawFastbootTextAt (
      "Volume Up",
      LeftArrowX + FASTBOOT_KEY_ICON_WIDTH + 12,
      VolumeUpY - 24,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      "Volume Down",
      LeftArrowX + FASTBOOT_KEY_ICON_WIDTH + 12,
      VolumeDownY - 24,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      "Start",
      RightArrowX - 105,
      PowerY - 24,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  AsciiSPrint (
      Product,
      sizeof (Product),
      "%a",
      PRODUCT_NAME);

  BoardSerialNum (
      Serial,
      sizeof (Serial));

  CurrentSlot = GetCurrentSlotSuffix ();

  UnicodeStrToAsciiStr (
      CurrentSlot.Suffix,
      SlotSuffixAscii);

  if (SlotSuffixAscii[0] == '_' &&
      SlotSuffixAscii[1] != '\0') {
    SlotSuffixAscii[0] = SlotSuffixAscii[1];
    SlotSuffixAscii[1] = '\0';
  }

  /*
   * Bottom aligned compact layout.
   */
  LabelX = Width * 6 / 100;
  ColonX = Width * 30 / 100;
  ValueX = Width * 34 / 100;

  Y = Height * 70 / 100;
  Step = 48;

  Status = DrawFastbootIcon (
      gFastbootWarningIcon,
      FASTBOOT_WARNING_ICON_WIDTH,
      FASTBOOT_WARNING_ICON_HEIGHT,
      LabelX,
      Y - 132);

  if (EFI_ERROR (Status))
    return Status;

  Status = DrawFastbootTextAt (
      "Fastboot Mode",
      LabelX,
      Y,
      BIG_FACTOR,
      BGR_RED,
      NULL);

  if (EFI_ERROR (Status))
    return Status;

  Y += 66;

  /*
   * Product
   */
  DrawFastbootTextAt (
      "Product",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      Product,
      ValueX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  Y += Step;

  /*
   * Bootloader version
   */
  DrawFastbootTextAt (
      "Bootloader",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      BOOTLOADER_VERSION,
      ValueX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  Y += Step;

  /*
   * Serial
   */
  DrawFastbootTextAt (
      "Serial",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      Serial,
      ValueX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  Y += Step;

  /*
   * Secure Boot
   */
  DrawFastbootTextAt (
      "Secure Boot",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      IsSecureBootEnabled () ? "yes" : "no",
      ValueX,
      Y,
      COMMON_FACTOR,
      IsSecureBootEnabled () ?
          BGR_GREEN : BGR_RED,
      NULL);

  Y += Step;

  /*
   * Device state
   */
  DrawFastbootTextAt (
      "Device state",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      IsUnlocked () ? "unlocked" : "locked",
      ValueX,
      Y,
      COMMON_FACTOR,
      IsUnlocked () ?
          BGR_GREEN : BGR_RED,
      NULL);

  Y += Step;

  /*
   * Active slot
   */
  DrawFastbootTextAt (
      "Active slot",
      LabelX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      ":",
      ColonX,
      Y,
      COMMON_FACTOR,
      BGR_WHITE,
      NULL);

  DrawFastbootTextAt (
      SlotSuffixAscii,
      ValueX,
      Y,
      COMMON_FACTOR,
      BGR_GREEN,
      NULL);

  Y += 62;
  InstructionY = Y;

  DrawFastbootTextAt (
      "Press volume keys to select different menu",
      LabelX,
      InstructionY,
      COMMON_FACTOR,
      BGR_RED,
      &InstructionHeight);

  /*
   * Visually center the divider between the bottom edge of the
   * instruction text and the top edge of the selected text.
   */
  DividerY =
      (InstructionY +
       InstructionHeight +
       (Height * 92 / 100)) / 2;

  FillRect (
      Width * 6 / 100,
      DividerY,
      Width * 88 / 100,
      1,
      BGR_DARK_GRAY);

  OptionItem =
      OptionMenuInfo->Info.OptionItems[
          OptionMenuInfo->Info.OptionIndex];

  Status = UpdateFastbootOptionItem (
      OptionItem,
      NULL);

  if (EFI_ERROR (Status))
    return Status;

  /*
   * Firmware signature.
   */
  DrawFastbootTextAt (
      "2026 | yanik",
      Width * 6 / 100,
      Height * 96 / 100,
      COMMON_FACTOR,
      BGR_DARK_GRAY,
      NULL);

  return EFI_SUCCESS;
}

/* Draw the fastboot menu and start to detect the key's status */
VOID DisplayFastbootMenu (VOID)
{
  EFI_STATUS Status;
  OPTION_MENU_INFO *OptionMenuInfo;

  if (IsEnableDisplayMenuFlagSupported ()) {
    OptionMenuInfo = &gMenuInfo;
    DrawMenuInit ();
    OptionMenuInfo->LastMenuType = OptionMenuInfo->Info.MenuType;

    Status = FastbootMenuShowScreen (OptionMenuInfo);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to show fastboot menu on screen: %r\n",
              Status));
      return;
    }

    MenuKeysDetectionInit (OptionMenuInfo);
    DEBUG ((EFI_D_VERBOSE, "Creating fastboot menu keys detect event\n"));
  } else {
    DEBUG ((EFI_D_INFO, "Display menu is not enabled!\n"));
  }
}
