// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SettingsManagerWindow.h"

class FToolBarBuilder;
class FMenuBuilder;
class ISettingsContainer;

class FSettingsManagerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
    virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void ExportButtonClicked();
	void ImportButtonClicked();

private:
	void RegisterMenus();

	template<bool IsForExport>
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);


private:
	SSettingsManagerWindow::FImportData ImportData;
	TSharedPtr<class FUICommandList> PluginCommands;
};
