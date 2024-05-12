// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Notifications/SNotificationList.h"

class ISettingsContainer;
/**
 * 
 */
class SSettingsManagerWindow
	: public SCompoundWidget
{
public:
	struct FSectionDataForExport
	{
		FText DisplayName;
		ECheckBoxState CheckBoxState;
	};

	struct FCategoryDataForExport
	{
		FText DisplayName;
		TMap<FName, FSectionDataForExport> Sections;
	};

	struct FSectionDataForImport
	{
		FText DisplayName;
		ECheckBoxState CheckBoxState;
		FString FilePath;
	};

	struct FCategoryDataForImport
	{
		FText DisplayName;
		TMap<FName, FSectionDataForImport> Sections;
	};

    using FImportData = TMap<FName, TMap<FName, FString>>;

public:
	SLATE_BEGIN_ARGS(SSettingsManagerWindow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, bool IsForExport, FImportData ImportDataParam);

private:
	template<bool IsForExport>
	void OnSpawnTab();

	template<bool IsForExport>
	TSharedRef<SVerticalBox> CreateTab(int Index);

	FReply DoExport();
	FReply DoImport();

	EVisibility GetTabVisibility(int Index) const;

	static void ShowNotification(const FText& Text, SNotificationItem::ECompletionState CompletionState);

private:
	TArray<TSharedPtr<ISettingsContainer>> SettingsContainers;

	int CurrentTabIndex = 0;

	FImportData ImportData;
	TArray<TMap<FName, FCategoryDataForExport>> SettingsDataToExport;
	TArray<TMap<FName, FCategoryDataForImport>> SettingsDataToImport;
};
