/*===============================================================================
Copyright (c) 2020 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"

#include <AppController.h>


using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace winrt::VuforiaSample::implementation
{
    MainPage::MainPage()
    {
        InitializeComponent();
    }


    void MainPage::ClickHandler(IInspectable const& sender, RoutedEventArgs const&)
    {
        int32_t target = -1;
        if (sender == ImageTargetButton())
        {
            target = AppController::IMAGE_TARGET_ID;
        }
        else if (sender == ModelTargetButton())
        {
            target = AppController::MODEL_TARGET_ID;
        }
        this->Frame().Navigate(xaml_typename<VuforiaPage>(), box_value(target));
    }
}
