/*===============================================================================
Copyright (c) 2020 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#pragma once

#include "VuforiaPage.g.h"

#include <AppController.h>
#include "Rendering/DeviceResources.h"
#include "Rendering/StepTimer.h"
#include "Rendering/DXRenderer.h"

#include <atomic>
#include <mutex>
#include <future>


namespace winrt::VuforiaSample::implementation
{
    struct VuforiaPage : VuforiaPageT<VuforiaPage>, public DX::IDeviceNotify
    {
        VuforiaPage();
        ~VuforiaPage();

        void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void OnNavigatingFrom(Windows::UI::Xaml::Navigation::NavigatingCancelEventArgs const& e);

        void OnBackButtonClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void OnPointerPressed(Windows::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        // IDeviceNotify
        void OnDeviceLost();
        void OnDeviceRestored();

    private: // methods

        std::future<void> VuforiaPage::InitVuforia(int32_t target);
        std::future<void> VuforiaPage::DeinitVuforiaAndGoBack();

        void OnSuspending(Windows::Foundation::IInspectable const& sender, Windows::ApplicationModel::SuspendingEventArgs const& e);
        void OnResuming(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& args);
        void OnVisibilityChanged(Windows::UI::Core::CoreWindow const& sender, Windows::UI::Core::VisibilityChangedEventArgs const& args);

        std::future<void> HandleSizeChanged(Windows::UI::Xaml::SizeChangedEventArgs const& args);
        std::future<void> VuforiaPage::HandleTap();

        //// Pauses Vuforia and stops the camera
        std::future<void> VuforiaPage::PauseAR();
        //// Resumes Vuforia, restarts the trackers and the camera
        std::future<void> VuforiaPage::ResumeAR();

        /// Callback from AppController when an error occurs
        std::future<void> PresentError(const char* error);
        /// Callback from AppController when Vuforia initialization completes
        std::future<void> InitDone();

        // DirectX rendering
        void CreateWindowSizeDependentResources();
        void StartRenderLoop();
        void StopRenderLoop();
        /// Updates the application state once per frame.
        void Update();
        /// Process all input from the user before updating game state.
        void ProcessInput();
        /// Render content each frame.
        bool Render();

    private: // data members

        event_token mSuspendingToken;
        event_token mResumingToken;
        event_token mVisibilityChangedToken;

        /// Mutex to manage interaction between UI and render thread
        std::mutex mCriticalSection;

        /// Resources used to render the DirectX content in the XAML page background.
        std::shared_ptr<winrt::DX::DeviceResources> mDeviceResources;

        AppController mController;
        bool mVuforiaInitializing = false;
        bool mVuforiaStarted = false;
        std::atomic<bool> mRenderingConfigurationChanged{ true };
        std::atomic<bool> mSwapChainPanelSizeChanged{ true };
        Windows::Foundation::Size mSwapChainPanelSize;

        // DirectX renderer
        std::unique_ptr<DXRenderer> mRenderer;

        /// Render loop worker task
        Windows::Foundation::IAsyncAction mRenderLoopWorker;
        /// Rendering loop timer
        SampleCommon::StepTimer mTimer;


        /* For suspending and resuming we use coroutines.
         * This is necessary because pausing and resuming Vuforia takes
         * too long to run on the UI thread and the platform doesn't allow this.
         * We need to ensure that these don't end up running concurrently and
         * that we always end up in the desired state even if the user
         * repeatedly suspend/resumes the app.
         */

        /// Flag to indicate when there is a pause operation in progress
        std::atomic<bool> mPauseARInProgress{ false };
        /// Flag to indicate when there is a resume operation in progress
        std::atomic<bool> mResumeARInProgress{ false };
        /// Count the times we have ignored a pause notification
        int mIgnoredPause = 0;
        /// Count the times we have ignored the resume notification
        int mIgnoredResume = 0;
    };
}

namespace winrt::VuforiaSample::factory_implementation
{
    struct VuforiaPage : VuforiaPageT<VuforiaPage, implementation::VuforiaPage>
    {
    };
}
