/*===============================================================================
 Copyright (c) 2020 PTC Inc. All Rights Reserved.

 Confidential and Proprietary - Protected under copyright and other laws.
 Vuforia is a trademark of PTC Inc., registered in the United States and other
 countries.
===============================================================================*/

#pragma once
#include "App.xaml.g.h"

namespace winrt::VuforiaSample::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
        void OnSuspending(IInspectable const&, Windows::ApplicationModel::SuspendingEventArgs const&);
        void OnNavigationFailed(IInspectable const&, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs const&);
    };
}
