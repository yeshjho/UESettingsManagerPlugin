// Copyright Epic Games, Inc. All Rights Reserved.

#include "SettingsManager.h"

#include "DesktopPlatformModule.h"
#include "SettingsManagerCommands.h"
#include "SettingsManagerStyle.h"

static const FName ExportTabName("ExportTab");
static const FName ImportTabName("ImportTab");

#define LOCTEXT_NAMESPACE "FSettingsManagerModule"

void FSettingsManagerModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

    FSettingsManagerStyle::Initialize();
    FSettingsManagerStyle::ReloadTextures();

    FSettingsManagerCommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
        FSettingsManagerCommands::Get().OpenExportWindow,
        FExecuteAction::CreateRaw(this, &FSettingsManagerModule::ExportButtonClicked),
        FCanExecuteAction());
    PluginCommands->MapAction(
        FSettingsManagerCommands::Get().OpenImportWindow,
        FExecuteAction::CreateRaw(this, &FSettingsManagerModule::ImportButtonClicked),
        FCanExecuteAction());

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSettingsManagerModule::RegisterMenus));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ExportTabName, FOnSpawnTab::CreateRaw(this, &FSettingsManagerModule::OnSpawnTab<true>))
        .SetDisplayName(LOCTEXT("ExportTabTitle", "Bulk Export Settings"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ImportTabName, FOnSpawnTab::CreateRaw(this, &FSettingsManagerModule::OnSpawnTab<false>))
        .SetDisplayName(LOCTEXT("ImportTabTitle", "Bulk Import Settings"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
}

template<bool IsForExport>
TSharedRef<SDockTab> FSettingsManagerModule::OnSpawnTab([[maybe_unused]] const FSpawnTabArgs& SpawnTabArgs)
{
    if constexpr (IsForExport)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SSettingsManagerWindow, true, SSettingsManagerWindow::FImportData{})
            ];
    }
    else
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SSettingsManagerWindow, false, ImportData)
            ];
    }
}

void FSettingsManagerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FSettingsManagerStyle::Shutdown();

	FSettingsManagerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ExportTabName);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ImportTabName);
}

void FSettingsManagerModule::ExportButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ExportTabName);
}

void FSettingsManagerModule::ImportButtonClicked()
{
    const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
    const void* ParentWindowHandle =
        ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid() ?
        ParentWindow->GetNativeWindow()->GetOSWindowHandle() :
        nullptr;

    FString OutFolder;
    if (!FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowHandle,
        LOCTEXT("ImportSettingsDialogTitle", "Import settings from...").ToString(),
        FPaths::GetPath(GEditorSettingsIni), OutFolder))
    {
        return;
    }
    
    ImportData.Empty();

    FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*OutFolder, [this](const TCHAR* Directory, bool bIsDirectory)
        {
            if (!bIsDirectory)
            {
                return true;
            }

            const FName CategoryName = FName{ *FPaths::GetPathLeaf(Directory) };
            TMap<FName, FString>& Category = ImportData.Add(CategoryName);

            FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(Directory,
                [this, &Category](const TCHAR* FileName, bool bIsDirectory)
                {
                    if (!bIsDirectory && FPaths::GetExtension(FileName) == "ini")
                    {
                        Category.Add(FName{ *FPaths::GetBaseFilename(FileName) }, FileName);
                    }
                    
                    return true;
                });

            return true;
        });

    FGlobalTabmanager::Get()->TryInvokeTab(ImportTabName);
}

void FSettingsManagerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
    FToolMenuOwnerScoped OwnerScoped{ this };

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Edit");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("Configuration");
			Section.AddMenuEntryWithCommandList(FSettingsManagerCommands::Get().OpenExportWindow, PluginCommands);
			Section.AddMenuEntryWithCommandList(FSettingsManagerCommands::Get().OpenImportWindow, PluginCommands);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSettingsManagerModule, SettingsManager)