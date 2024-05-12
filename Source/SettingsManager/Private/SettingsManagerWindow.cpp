// Fill out your copyright notice in the Description page of Project Settings.

#include "SettingsManagerWindow.h"

#include "DesktopPlatformModule.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "FSettingsManagerModule"

void SSettingsManagerWindow::Construct([[maybe_unused]] const FArguments& InArgs, bool IsForExport, FImportData ImportDataParam)
{
    ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
    check(SettingsModule != nullptr);
    SettingsContainers.Add(SettingsModule->GetContainer("Editor"));
    check(SettingsContainers[0].IsValid());

    SettingsContainers.Add(SettingsModule->GetContainer("Project"));
    check(SettingsContainers[1].IsValid());

    SettingsDataToExport.AddDefaulted(2);
    SettingsDataToImport.AddDefaulted(2);

    if (IsForExport)
    {
        OnSpawnTab<true>();
    }
    else
    {
        ImportData = MoveTemp(ImportDataParam);
        OnSpawnTab<false>();
    }
}

template <bool IsForExport>
void SSettingsManagerWindow::OnSpawnTab()
{
    const TSharedRef<SVerticalBox> EditorPreferencesTab = CreateTab<IsForExport>(0);
    const TSharedRef<SVerticalBox> ProjectSettingsTab = CreateTab<IsForExport>(1);
    ChildSlot
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin{ 30, 10, 10, 10 })
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(3)
                        [
                            SNew(SButton)
                                .HAlign(EHorizontalAlignment::HAlign_Center)
                                .Text(LOCTEXT("EditorPreferencesTabTitle", "Editor Preferences"))
                                .OnClicked_Lambda([this]() { CurrentTabIndex = 0; return FReply::Handled(); })
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(3)
                        [
                            SNew(SButton)
                                .HAlign(EHorizontalAlignment::HAlign_Center)
                                .Text(LOCTEXT("ProjectSettingsTabTitle", "Project Settings"))
                                .OnClicked_Lambda([this]() { CurrentTabIndex = 1; return FReply::Handled(); })
                        ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin{ 20, 5, 20, 5 })
                [
                    SNew(SSeparator)
                        .Orientation(Orient_Vertical)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                [
                    EditorPreferencesTab
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                [
                    ProjectSettingsTab
                ]
        ];
}

template<bool IsForExport>
TSharedRef<SVerticalBox> SSettingsManagerWindow::CreateTab(int TabIndex)
{
    // sort the sections alphabetically
    struct FSectionSortPredicate
    {
        FORCEINLINE bool operator()(ISettingsSectionPtr A, ISettingsSectionPtr B) const
        {
            if (!A.IsValid() && !B.IsValid())
            {
                return false;
            }

            if (A.IsValid() != B.IsValid())
            {
                return B.IsValid();
            }

            return (A->GetDisplayName().CompareTo(B->GetDisplayName()) < 0);
        }
    };
    
    if constexpr (IsForExport)
    {
        TArray<TSharedPtr<ISettingsCategory>> SettingsCategories;
        SettingsContainers[TabIndex]->GetCategories(SettingsCategories);
        for (const TSharedPtr<ISettingsCategory>& Category : SettingsCategories)
        {
            auto& [DisplayName, Sections] = SettingsDataToExport[TabIndex].Add(Category->GetName());
            DisplayName = Category->GetDisplayName();

            TArray<TSharedPtr<ISettingsSection>> SettingsSections;
            Category->GetSections(SettingsSections);
            SettingsSections.Sort(FSectionSortPredicate{});

            for (const TSharedPtr<ISettingsSection>& Section : SettingsSections)
            {
                if (Section->CanExport())
                {
                    Sections.Add(Section->GetName(), { Section->GetDisplayName(), ECheckBoxState::Unchecked });
                }
            }
        }
    }
    else
    {
        for (const auto& [CategoryName, Sections] : ImportData)
        {
            const TSharedPtr<ISettingsCategory>& Category = SettingsContainers[TabIndex]->GetCategory(CategoryName);
            if (!Category.IsValid())
            {
                continue;
            }

            auto& [DisplayName, SectionsData] = SettingsDataToImport[TabIndex].Add(CategoryName);
            DisplayName = Category->GetDisplayName();

            for (const auto& [SectionName, FilePath] : Sections)
            {
                const TSharedPtr<ISettingsSection> Section = Category->GetSection(SectionName);
                if (Section.IsValid() && Section->CanImport())
                {
                    SectionsData.Add(SectionName, { Section->GetDisplayName(), ECheckBoxState::Checked, FilePath });
                }
            }
        }
    }

    const TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox).Visibility(this, &SSettingsManagerWindow::GetTabVisibility, TabIndex);

    TMap<FName, std::conditional_t<IsForExport, FCategoryDataForExport, FCategoryDataForImport>>& SettingsDataToUse =
        [this, TabIndex]() -> auto&
        {
            if constexpr (IsForExport)
            {
                return SettingsDataToExport[TabIndex];
            }
            else
            {
                return SettingsDataToImport[TabIndex];
            }
        }();

    const auto LambdaIsSavedProjectBased = [SettingsContainer = SettingsContainers[TabIndex]](const FName CategoryName, const FName SectionName)
        {
            const TWeakObjectPtr<UObject> SettingsObject = SettingsContainer->GetCategory(CategoryName)->GetSection(SectionName)->GetSettingsObject();
            return SettingsObject.IsValid() && SettingsObject->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig);
        };

    const auto LambdaSelectAllOnCheckStateChanged = 
        [&SettingsDataToUse](ECheckBoxState State)
        {
            for (auto& [CategoryName, CategoryData] : SettingsDataToUse)
            {
                for (auto& [_, SectionData] : CategoryData.Sections)
                {
                    SectionData.CheckBoxState = State;
                }
            }
        };

    const auto LambdaSelectAllIsChecked =
        [&SettingsDataToUse]()
        {
            bool Initialized = false;
            ECheckBoxState State = ECheckBoxState::Unchecked;
            for (const auto& [CategoryName, CategoryData] : SettingsDataToUse)
            {
                for (const auto& [_, SectionData] : CategoryData.Sections)
                {
                    if (!Initialized)
                    {
                        State = SectionData.CheckBoxState;
                        Initialized = true;
                    }
                    else if (State != SectionData.CheckBoxState)
                    {
                        return ECheckBoxState::Undetermined;
                    }
                }
            }
            return State;
        };

    const auto LambdaReverseSavedSettingsWarningVisibility =
        [&SettingsDataToUse, LambdaIsSavedProjectBased, TabIndex]()
        {
            for (const auto& [CategoryName, CategoryData] : SettingsDataToUse)
            {
                for (const auto& [SectionName, SectionData] : CategoryData.Sections)
                {
                    if (SectionData.CheckBoxState != ECheckBoxState::Checked)
                    {
                        continue;
                    }

                    if (const bool IsProjectBased = LambdaIsSavedProjectBased(CategoryName, SectionName);
                        (TabIndex == 0 && IsProjectBased) || (TabIndex == 1 && !IsProjectBased))
                    {
                        return EVisibility::Visible;
                    }
                }
            }

            return EVisibility::Collapsed;
        };

    const auto LambdaDeselectAllReverseSavedSettings =
        [&SettingsDataToUse, LambdaIsSavedProjectBased, TabIndex]()
        {
            for (auto& [CategoryName, CategoryData] : SettingsDataToUse)
            {
                for (auto& [SectionName, SectionData] : CategoryData.Sections)
                {
                    if (SectionData.CheckBoxState == ECheckBoxState::Checked)
                    {
                        if (const bool IsProjectBased = LambdaIsSavedProjectBased(CategoryName, SectionName);
                            (TabIndex == 0 && IsProjectBased) || (TabIndex == 1 && !IsProjectBased))
                        {
                            SectionData.CheckBoxState = ECheckBoxState::Unchecked;
                        }
                    }
                }
            }
            return FReply::Handled();
        };

    const auto LambdaFalsePositiveNoteVisibility = 
        [&SettingsDataToUse, TabIndex]()
        {
            if (!IsForExport)
            {
                return EVisibility::Collapsed;
            }

            if (TabIndex == 0 && SettingsDataToUse.Contains("General"))
            {
                const auto& GeneralCategory = SettingsDataToUse["General"];
                if (GeneralCategory.Sections.Contains("Appearance") && GeneralCategory.Sections["Appearance"].CheckBoxState == ECheckBoxState::Checked)
                {
                    return EVisibility::Visible;
                }
                if (GeneralCategory.Sections.Contains("InputBindings") && GeneralCategory.Sections["InputBindings"].CheckBoxState == ECheckBoxState::Checked)
                {
                    return EVisibility::Visible;
                }
            }

            return EVisibility::Collapsed;
        };

    VerticalBox->AddSlot()
                .AutoHeight()
                .VAlign(EVerticalAlignment::VAlign_Center)
                .Padding(10)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .HAlign(EHorizontalAlignment::HAlign_Left)
                [
                    SNew(SCheckBox)
                        .Padding(FMargin{ 10, 0, 0, 0 })
                        .Content()
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("SelectDeselectAll", "Select / Deselect All"))
                                .ColorAndOpacity(FLinearColor::Green)
                        ]
                        .OnCheckStateChanged_Lambda(LambdaSelectAllOnCheckStateChanged)
                        .IsChecked_Lambda(LambdaSelectAllIsChecked)
                ]
                + SHorizontalBox::Slot()
                .HAlign(EHorizontalAlignment::HAlign_Center)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 10, 0, 5)
                        [
                            SNew(STextBlock)
                                .Justification(ETextJustify::Center)
                                .Text(TabIndex == 0 ? 
                                    LOCTEXT("ProjectSavedSettingsExistWarning", "WARNING: Some of the selected settings are saved at project-level.") :
                                    TabIndex == 1 ?
                                    LOCTEXT("EditorSavedSettingsExistWarning", "WARNING: Some of the selected settings are saved at editor-level.") :
                                    FText::GetEmpty()
                                )
                                .ColorAndOpacity(FLinearColor::Yellow)
                                .Visibility_Lambda(LambdaReverseSavedSettingsWarningVisibility)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 15)
                        [
                            SNew(SButton)
                                .HAlign(EHorizontalAlignment::HAlign_Center)
                                .Text(LOCTEXT("DeselectAllReverseSavedSettingsButton", "Deselect Them All"))
                                .Visibility_Lambda(LambdaReverseSavedSettingsWarningVisibility)
                                .OnClicked_Lambda(LambdaDeselectAllReverseSavedSettings)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 5)
                        [
                            SNew(STextBlock)
                                .Justification(ETextJustify::Center)
                                .Text(LOCTEXT("FalsePositiveNote", "NOTE: Some settings will be indicated as a failure even if they succeed."))
                                .ColorAndOpacity(FColor::Magenta)
                                .Visibility_Lambda(LambdaFalsePositiveNoteVisibility)
                        ]
                ]
                + SHorizontalBox::Slot()
                .HAlign(EHorizontalAlignment::HAlign_Right)
                [
                    SNew(SButton)
                        .VAlign(EVerticalAlignment::VAlign_Center)
                        .Text(IsForExport ? LOCTEXT("ExportButton", "Export") : LOCTEXT("ImportButton", "Import"))
                        .OnClicked_Raw(this, IsForExport ? &SSettingsManagerWindow::DoExport : &SSettingsManagerWindow::DoImport)
                ]
        ];

    const TSharedRef<SVerticalBox> CategoriesAndSections = SNew(SVerticalBox);
    for (auto& [CategoryName, CategoryData] : SettingsDataToUse)
    {
        auto& Sections = CategoryData.Sections;
        CategoriesAndSections->AddSlot()
            .Padding(FMargin{ 10, 5, 5, 5 })
            [
                SNew(SCheckBox)
                    .Padding(FMargin{ 10, 0, 0, 0 })
                    .Content()
                    [
                        SNew(STextBlock)
                            .Text(CategoryData.DisplayName)
                            .ColorAndOpacity(FColor::Turquoise)
                    ]
                    .OnCheckStateChanged_Lambda([&Sections](ECheckBoxState State)
                        {
                            for (auto& [_, SectionData] : Sections)
                            {
                                SectionData.CheckBoxState = State;
                            }
                        })
                    .IsChecked_Lambda([&Sections]()
                        {
                            if (Sections.Num() == 0)
                            {
                                return ECheckBoxState::Unchecked;
                            }

                            const ECheckBoxState State = Sections.begin()->Value.CheckBoxState;
                            for (const auto& [_, SectionData] : Sections)
                            {
                                if (State != SectionData.CheckBoxState)
                                {
                                    return ECheckBoxState::Undetermined;
                                }
                            }
                            return State;
                        })
            ];

        for (auto& [SectionName, SectionData] : Sections)
        {
            CategoriesAndSections->AddSlot()
                .Padding(FMargin{ 40, 5, 5, 5 })
                [
                    SNew(SCheckBox)
                        .Padding(FMargin{ 10, 0, 0, 0 })
                        .Content()
                        [
                            SNew(STextBlock)
                                .Text(SectionData.DisplayName)
                                .ToolTipText_Lambda([CategoryName, SectionName, LambdaIsSavedProjectBased, TabIndex]()
                                    {
                                        if (const bool IsProjectBased = LambdaIsSavedProjectBased(CategoryName, SectionName);
                                            (TabIndex == 0 && IsProjectBased) || (TabIndex == 1 && !IsProjectBased))
                                        {
                                            return TabIndex == 0 ?
                                                LOCTEXT("ProjectSavedSettingsWarning", "This setting is saved at project-level.") :
                                                LOCTEXT("EditorSavedSettingsWarning", "This setting is saved at editor-level.");
                                        }

                                        if (TabIndex == 0 && CategoryName == "General")
                                        {
                                            if (SectionName == "Appearance")
                                            {
                                                return LOCTEXT("GeneralAppearanceFalseNegativeNote", "This setting will be indicated as a failure even if it succeeds.");
                                            }

                                            if (SectionName == "InputBindings")
                                            {
                                                return LOCTEXT("GeneralInputBindingsFalseNegativeNote", "This setting will fail if there's no modification from the engine default.");
                                            }
                                        }

                                        return FText::GetEmpty();
                                    })
                                .ColorAndOpacity_Lambda([CategoryName, SectionName, LambdaIsSavedProjectBased, TabIndex]() -> FSlateColor
                                    {
                                        if (const bool IsProjectBased = LambdaIsSavedProjectBased(CategoryName, SectionName);
                                            (TabIndex == 0 && IsProjectBased) || (TabIndex == 1 && !IsProjectBased))
                                        {
                                            return FLinearColor::Yellow;
                                        }

                                        if (IsForExport && TabIndex == 0 && CategoryName == "General" && (SectionName == "Appearance" || SectionName == "InputBindings"))
                                        {
                                            return FColor::Magenta;
                                        }

                                        return FLinearColor::White;
                                    })
                        ]
                        .OnCheckStateChanged_Lambda([&SectionData](ECheckBoxState State)
                            {
                                SectionData.CheckBoxState = State;
                            })
                        .IsChecked_Lambda([&SectionData]()
                            {
                                return SectionData.CheckBoxState;
                            })
                ];
        }
    }

    const TSharedRef<SScrollBox> Scroll = SNew(SScrollBox);
    Scroll->SetOrientation(EOrientation::Orient_Vertical);
    Scroll->AddSlot()
        [
            CategoriesAndSections
        ];

    VerticalBox->AddSlot()
        [
            Scroll
        ];

    return VerticalBox;
}

FReply SSettingsManagerWindow::DoExport()
{
    const TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
    const void* ParentWindowHandle =
        ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid() ?
        ParentWindow->GetNativeWindow()->GetOSWindowHandle() :
        nullptr;

    FString OutFolder;
    if (!FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowHandle,
        LOCTEXT("ExportSettingsDialogTitle", "Export settings to...").ToString(),
        FPaths::GetPath(GEditorSettingsIni), OutFolder))
    {
        return FReply::Handled();
    }

    const ISettingsContainerPtr& SettingsContainer = SettingsContainers[CurrentTabIndex];

    TArray<FText> FailedExports;
    for (const auto& [CategoryName, CategoryData] : SettingsDataToExport[CurrentTabIndex])
    {
        const TSharedPtr<ISettingsCategory> Category = SettingsContainer->GetCategory(CategoryName);
        check(Category.IsValid());

        FString Folder = FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("%s/%s"), *OutFolder, *CategoryName.ToString()));
        if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Folder))
        {
            FailedExports.Add(FText::Format(LOCTEXT("FailedToCreateDirectory", "Failed to create directory {0}"), FText::FromString(OutFolder)));
            continue;
        }

        for (const auto& [SectionName, SectionData] : CategoryData.Sections)
        {
            if (SectionData.CheckBoxState != ECheckBoxState::Checked)
            {
                continue;
            }

            const TSharedPtr<ISettingsSection> Section = Category->GetSection(SectionName);
            check(Section.IsValid());

            FString FileName = FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("%s/%s.ini"), *Folder, *SectionName.ToString()));
            //if (Section->CanSaveDefaults())
            //{
            //    if (const TWeakObjectPtr<UObject> SettingsObject = Section->GetSettingsObject();
            //        SettingsObject.IsValid())
            //    {
            //        if (SettingsObject->TryUpdateDefaultConfigFile(FileName))
            //        {
            //            SettingsObject->ReloadConfig(nullptr, *FileName, UE::LCPF_PropagateToInstances);
            //            continue;
            //        }
            //    }
            //}
            if (!Section->Export(FileName))
            {
                FailedExports.Add(FText::Format(FText::FromString("{0}/{1}"), CategoryData.DisplayName, SectionData.DisplayName));
            }
        }
    }

    if (FailedExports.Num() == 0)
    {
        ShowNotification(LOCTEXT("ExportSettingsSuccess", "Export settings succeeded"), SNotificationItem::CS_Success);
    }
    else
    {
        const FText Msg = FText::Format(LOCTEXT("ExportSettingsFailure", "Export settings failed for:\n{0}"),
            FText::Join(FText::FromString("\n"), FailedExports));
        UE_LOG(LogConfig, Error, TEXT("%s"), *Msg.ToString());
        ShowNotification(Msg, SNotificationItem::CS_Fail);
    }

    return FReply::Handled();
}

FReply SSettingsManagerWindow::DoImport()
{
    const ISettingsContainerPtr SettingsContainer = SettingsContainers[CurrentTabIndex];

    TArray<FText> FailedImports;
    for (const auto& [CategoryName, CategoryData] : SettingsDataToImport[CurrentTabIndex])
    {
        const TSharedPtr<ISettingsCategory> Category = SettingsContainer->GetCategory(CategoryName);
        check(Category.IsValid());

        for (const auto& [SectionName, SectionData] : CategoryData.Sections)
        {
            if (SectionData.CheckBoxState != ECheckBoxState::Checked)
            {
                continue;
            }

            const TSharedPtr<ISettingsSection> Section = Category->GetSection(SectionName);
            check(Section.IsValid());

            //if (Section->CanResetDefaults())
            //{
            //    if (const TWeakObjectPtr<UObject> SettingsObject = Section->GetSettingsObject();
            //        SettingsObject.IsValid())
            //    {
            //        const FString ConfigName = SettingsObject->GetClass()->GetConfigName();

            //        GConfig->EmptySection(*SettingsObject->GetClass()->GetPathName(), ConfigName);
            //        GConfig->Flush(false);

            //        FConfigContext::ForceReloadIntoGConfig().Load(*FPaths::GetBaseFilename(ConfigName));

            //        SettingsObject->ReloadConfig(nullptr, FileName, UE::LCPF_PropagateToInstances | UE::LCPF_PropagateToChildDefaultObjects);

            //        return true;
            //    }
            //}

            if (!Section->Import(SectionData.FilePath) || !Section->Save())
            {
                FailedImports.Add(FText::Format(FText::FromString("{0}/{1}"), CategoryData.DisplayName, SectionData.DisplayName));
            }
        }
    }

    if (FailedImports.Num() == 0)
    {
        ShowNotification(LOCTEXT("ImportSettingsSuccess", "Import settings succeeded"), SNotificationItem::CS_Success);
    }
    else
    {
        const FText Msg = FText::Format(LOCTEXT("ImportSettingsFailure", "Import settings failed for:\n{0}"),
            FText::Join(FText::FromString("\n"), FailedImports));
        UE_LOG(LogConfig, Error, TEXT("%s"), *Msg.ToString());
        ShowNotification(Msg, SNotificationItem::CS_Fail);
    }

    return FReply::Handled();
}

EVisibility SSettingsManagerWindow::GetTabVisibility(int Index) const
{
    return Index == CurrentTabIndex ? EVisibility::Visible : EVisibility::Collapsed;
}

void SSettingsManagerWindow::ShowNotification(const FText& Text, SNotificationItem::ECompletionState CompletionState)
{
    FNotificationInfo Notification(Text);
    Notification.ExpireDuration = 10.f;

    FSlateNotificationManager::Get().AddNotification(Notification)->SetCompletionState(CompletionState);
}

#undef LOCTEXT_NAMESPACE
