#include "UI/ModuleOperationsPanelBase.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

void SModuleOperationsPanel::Construct(const FArguments& InArgs) {
    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().FillHeight(
                  1.0f)[SNew(SScrollBox) +
                        SScrollBox::Slot()[SAssignNew(OperationContainer,
                                                      SVerticalBox)]]];

    CreateOperationWidgets();
}