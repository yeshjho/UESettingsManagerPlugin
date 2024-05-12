// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "SettingsManagerStyle.h"

class FSettingsManagerCommands : public TCommands<FSettingsManagerCommands>
{
public:

	FSettingsManagerCommands()
		: TCommands<FSettingsManagerCommands>(TEXT("SettingsManager"), NSLOCTEXT("Contexts", "SettingsManager", "SettingsManager Plugin"), NAME_None, FSettingsManagerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenExportWindow;
	TSharedPtr< FUICommandInfo > OpenImportWindow;
};