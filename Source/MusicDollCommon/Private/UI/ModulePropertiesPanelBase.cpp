#include "UI/ModulePropertiesPanelBase.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

void SModulePropertiesPanel::Construct(const FArguments& InArgs) {
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().FillHeight(
                  1.0f)[SNew(SScrollBox) +
                        SScrollBox::Slot()[SAssignNew(PropertyContainer,
                                                      SVerticalBox)]]];

    CreatePropertyWidgets();
}