// Copyright Epic Games, Inc. All Rights Reserved.

#include "SettingsManagerCommands.h"

#define LOCTEXT_NAMESPACE "FSettingsManagerModule"

void FSettingsManagerCommands::RegisterCommands()
{
	UI_COMMAND(OpenExportWindow, "Bulk Export Settings", "Bulk export Editor Preferences or Project Settings", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenImportWindow, "Bulk Import Settings", "Bulk import Editor Preferences or Project Settings", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
