/*===============================================================================
Copyright (c) 2020 PTC Inc. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "pch.h"
#include "VuforiaPage.h"
#include "VuforiaPage.g.cpp"

#include <Log.h>

#include <Vuforia/Tool.h>
#include <Vuforia/UWP/DXRenderer.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.System.Threading.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;


namespace winrt::VuforiaSample::implementation
{

    VuforiaPage::VuforiaPage()
    {
        InitializeComponent();

        auto app = Application::Current;
        mSuspendingToken =
            app().Suspending({ this, &VuforiaPage::OnSuspending });
        mResumingToken =
            app().Resuming({ this, &VuforiaPage::OnResuming });

        // Register event handlers for page lifecycle.
        mVisibilityChangedToken =
            Window::Current().CoreWindow().VisibilityChanged({ this, &VuforiaPage::OnVisibilityChanged });

        // Register DisplayInformation Events
        DisplayInformation display = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();

        display.DpiChanged([&](DisplayInformation const& display, IInspectable const&)
        {
            std::lock_guard<std::mutex> lock(mCriticalSection);
            // Note: The value for LogicalDpi retrieved here may not match the effective DPI of the app
            // if it is being scaled for high resolution devices. Once the DPI is set on DeviceResources,
            // you should always retrieve it using the GetDpi method.
            // See DeviceResources.cpp for more details.
            mDeviceResources->SetDpi(display.LogicalDpi());
            CreateWindowSizeDependentResources();
        });

        display.OrientationChanged([&](DisplayInformation const& display, IInspectable const&)
        {
            std::lock_guard<std::mutex> lock(mCriticalSection);
            mDeviceResources->SetCurrentOrientation(display.CurrentOrientation());
            CreateWindowSizeDependentResources();
        });

        display.DisplayContentsInvalidated([&](DisplayInformation const&, IInspectable const&)
        {
            std::lock_guard<std::mutex> lock(mCriticalSection);
            mDeviceResources->ValidateDevice();
        });


        // Register SwapChainPanel Events
        swapChainPanel().CompositionScaleChanged([&](SwapChainPanel const& panel, IInspectable const&)
        {
            std::lock_guard<std::mutex> lock(mCriticalSection);
            mDeviceResources->SetCompositionScale(panel.CompositionScaleX(), panel.CompositionScaleY());
            CreateWindowSizeDependentResources();
        });

        swapChainPanel().SizeChanged([&]([[maybe_unused]] IInspectable const& sender, SizeChangedEventArgs const& args)
        {
            HandleSizeChanged(args);
        });


        // At this point we have access to the device.
        // We can create the device-dependent resources.
        mDeviceResources = std::make_shared<DX::DeviceResources>();
        mDeviceResources->SetSwapChainPanel(swapChainPanel());
        mDeviceResources->SetDpi(display.LogicalDpi());

        // Register to be notified if the Device is lost or recreated
        mDeviceResources->RegisterDeviceNotify(this);

        // Init the scene renderer
        mRenderer = std::make_unique<DXRenderer>(mDeviceResources);

        // We set the desired frame rate here
        float fps = 60;
        mTimer.SetFixedTimeStep(true);
        mTimer.SetTargetElapsedSeconds(1.0 / fps);
    }


    VuforiaPage::~VuforiaPage()
    {
        mRenderer->ReleaseDeviceDependentResources();
        auto app = Application::Current;
        app().Suspending(mSuspendingToken);
        app().Resuming(mResumingToken);
        Window::Current().CoreWindow().VisibilityChanged(mVisibilityChangedToken);
    }


    void VuforiaPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        LOG("OnNavigatedTo");

        auto target = unbox_value<int32_t>(e.Parameter());
        if (target == 0)
        {
            LOG("Selected Image Target");
        }
        else if (target == 1)
        {
            LOG("Selected Model Target");
        }

        // Disable back button during initialization
        BackButton().IsEnabled(false);

        StartRenderLoop();
        InitVuforia(target);
    }


    void VuforiaPage::OnNavigatingFrom(Windows::UI::Xaml::Navigation::NavigatingCancelEventArgs const& e)
    {
        LOG("OnNavigatingFrom");

        StopRenderLoop();
        mRenderer->ReleaseDeviceDependentResources();

        // If Vuforia is initializing or started we need to clean up
        // This takes time so cancel this navigation and show the 
        // spinner while we clean up.
        if (mVuforiaInitializing || mVuforiaStarted)
        {
            e.Cancel(true);
            InitProgressRing().Visibility(Visibility::Visible);
            DeinitVuforiaAndGoBack();
        }
    }


    void VuforiaPage::OnBackButtonClicked(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        Frame().Navigate(xaml_typename<MainPage>());
        BackButton().IsEnabled(false);
    }


    void VuforiaPage::OnPointerPressed(Windows::UI::Xaml::Input::PointerRoutedEventArgs const&)
    {
        HandleTap();
    }


    void VuforiaPage::OnDeviceLost()
    {
        LOG("OnDeviceLost");

        mRenderer->ReleaseDeviceDependentResources();
    }
    
    
    void VuforiaPage::OnDeviceRestored()
    {
        LOG("OnDeviceRestored");

        mRenderer->CreateDeviceDependentResources();
        CreateWindowSizeDependentResources();
    }


    std::future<void> VuforiaPage::InitVuforia(int32_t target)
    {
        co_await winrt::resume_background();

        AppController::InitConfig config;
        config.vuforiaInitFlags = Vuforia::INIT_FLAGS::DX_11;
        config.showErrorCallback = std::bind(&VuforiaPage::PresentError, this, std::placeholders::_1);
        config.initDoneCallback = std::bind(&VuforiaPage::InitDone, this);

        mVuforiaInitializing = true;
        mController.initAR(config, target);
    }


    std::future<void> VuforiaPage::DeinitVuforiaAndGoBack()
    {
        LOG("DeinitVuforiaAndGoBack");

        // Vuforia operations must not occur on the UI thread
        co_await winrt::resume_background();
        if (mVuforiaStarted)
        {
            mController.stopAR();
            mVuforiaStarted = false;
        }
        mController.deinitAR();
        mVuforiaInitializing = false;

        co_await winrt::resume_foreground(Dispatcher());

        Frame().Navigate(xaml_typename<MainPage>());
    }


    void VuforiaPage::OnSuspending([[maybe_unused]] Windows::Foundation::IInspectable const& sender, [[maybe_unused]] Windows::ApplicationModel::SuspendingEventArgs const& e)
    {
        LOG("OnSuspending");

        std::scoped_lock<std::mutex> lock(mCriticalSection);

        mDeviceResources->Trim();

        auto deferral = e.SuspendingOperation().GetDeferral();

        concurrency::create_task([this, deferral]()
        {
            // Stop rendering when the app is suspended.
            StopRenderLoop();
            PauseAR();
            deferral.Complete();
        });
    }
 
 
    void VuforiaPage::OnResuming([[maybe_unused]] Windows::Foundation::IInspectable const& sender, [[maybe_unused]] Windows::Foundation::IInspectable const& args)
    {
        LOG("OnResuming");

        ResumeAR();
        StartRenderLoop();
    }


    void VuforiaPage::OnVisibilityChanged([[maybe_unused]] Windows::UI::Core::CoreWindow const& sender, Windows::UI::Core::VisibilityChangedEventArgs const& args)
    {
        LOG("OnVisibilityChanged");

        if (args.Visible())
        {
            LOG("Window changed to Visible");
            ResumeAR();
            StartRenderLoop();
        }
        else
        {
            LOG("Window changed to Not Visible");
            StopRenderLoop();
            PauseAR();
        }
    }


    std::future<void> VuforiaPage::HandleSizeChanged(Windows::UI::Xaml::SizeChangedEventArgs const& args)
    {
        // This callback happens a lot during window resizing so don't lock the mutex here

        // Record the new values, they will be applied in the Render loop
        mSwapChainPanelSize = args.NewSize();

        // Wait before setting the flag to throttle the number of updates
        co_await winrt::resume_background();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        mSwapChainPanelSizeChanged = true;
    }


    std::future<void> VuforiaPage::HandleTap()
    {
        static std::atomic<int> tapCount = 0;
        tapCount++;

        if (tapCount == 1)
        {
            co_await winrt::resume_background();

            // Wait for half-second, if no second tap comes,
            // then we have confirmed a single-tap
            // else we have a double-tap
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            if (tapCount == 1)
            {
                // tapCount is still 1, nothing changed,
                // so we have confirmed a single-tap,
                // so we consume the single-tap event:
                tapCount = 0;

                mController.cameraPerformAutoFocus();
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                mController.cameraRestoreAutoFocus();
            }
            else if (tapCount >= 2)
            {
                // We process the double-tap event
                tapCount = 0;

                co_await winrt::resume_foreground(Dispatcher());
                Frame().Navigate(xaml_typename<MainPage>());
                BackButton().IsEnabled(false);
            }
        }
    }


    std::future<void> VuforiaPage::PauseAR()
    {
        LOG("PauseAR");

        if (mVuforiaStarted)
        {
            // For an explanation of the following see the comment above
            // the declaration of mPauseARInProgress

            // Check for existing running pause task
            if (mPauseARInProgress)
            {
                mIgnoredPause++;
            }
            else
            {
                mPauseARInProgress = true;

                co_await winrt::resume_background();

                // Wait for any resume operation to complete
                while (mResumeARInProgress)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                // Only perform actions if we want to pause even after further events
                // Check that the same number or more resume events have been received
                if (mIgnoredPause >= mIgnoredResume)
                {
                    mController.pauseAR();
                }
                else
                {
                    mIgnoredResume--;
                }

                mPauseARInProgress = false;
            }
        }

        co_return;
    }


    std::future<void> VuforiaPage::ResumeAR()
    {
        LOG("ResumeAR");

        if (mVuforiaStarted)
        {
            // For an explanation of the following see the comment above
            // the declaration of mPauseARInProgress

            // Check for existing running pause task
            if (mResumeARInProgress)
            {
                mIgnoredResume++;
            }
            else
            {
                mResumeARInProgress = true;
                InitProgressRing().Visibility(Visibility::Visible);

                co_await winrt::resume_background();

                // Wait for any pause operation to complete
                while (mPauseARInProgress)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                // Only perform actions if we want to resume even after further events
                // Check that the same number or more pause events have been received
                if (mIgnoredResume >= mIgnoredPause)
                {
                    mController.resumeAR();
                }
                else
                {
                    mIgnoredPause--;
                }

                mResumeARInProgress = false;

                co_await winrt::resume_foreground(Dispatcher());
                InitProgressRing().Visibility(Visibility::Collapsed);

            }
        }

        co_return;
    }


    std::future<void> VuforiaPage::PresentError(const char* error)
    {
        LOG("PresentError: %s", error);

        co_await winrt::resume_foreground(Dispatcher());

        auto title = winrt::to_hstring("Vuforia error");
        auto message = winrt::to_hstring(error);

        MessageDialog popup = MessageDialog(message, title);

        UICommand okButton = UICommand(winrt::to_hstring("OK"));
        okButton.Invoked([this](Windows::UI::Popups::IUICommand const&) { this->Frame().Navigate(xaml_typename<MainPage>()); });
        popup.Commands().Append(okButton);

        popup.ShowAsync();
    }


    std::future<void> VuforiaPage::InitDone()
    {
        LOG("InitDone");
        mVuforiaStarted = mController.startAR();
        if (mVuforiaStarted)
        {
            // Only reset this flag if startAR succeeded so we deinit
            // Vuforia when navigating away from this page
            mVuforiaInitializing = false;
        }

        // Switch to UI thread to update controls
        co_await winrt::resume_foreground(Dispatcher());
        InitProgressRing().Visibility(Visibility::Collapsed);
        BackButton().IsEnabled(true);
    }


    // DirectX rendering
    
    void VuforiaPage::CreateWindowSizeDependentResources()
    {
        mRenderingConfigurationChanged = true;
        mRenderer->CreateWindowSizeDependentResources();
    }


    void VuforiaPage::StartRenderLoop()
    {
        LOG("StartRenderLoop");

        // If the animation render loop is already running then do not start another thread.
        if (mRenderLoopWorker != nullptr && mRenderLoopWorker.Status() == AsyncStatus::Started)
        {
            return;
        }

        // Create a task that will be run on a background thread.
        auto workItemHandler = Windows::System::Threading::WorkItemHandler([this](IAsyncAction const& action)
        {
            // Calculate the updated frame and render once per vertical blanking interval.
            while (action.Status() == AsyncStatus::Started)
            {
                std::scoped_lock<std::mutex> lock(mCriticalSection);

                if (mSwapChainPanelSizeChanged)
                {
                    mDeviceResources->SetLogicalSize(mSwapChainPanelSize);
                    CreateWindowSizeDependentResources();
                    mSwapChainPanelSizeChanged = false;
                }

                Update();
                if (Render())
                {
                    mDeviceResources->Present();
                }
            }
        });

        // Run task on a dedicated high priority background thread.
        mRenderLoopWorker = Windows::System::Threading::ThreadPool::RunAsync(
            workItemHandler,
            Windows::System::Threading::WorkItemPriority::High,
            Windows::System::Threading::WorkItemOptions::TimeSliced
            );
    }


    void VuforiaPage::StopRenderLoop()
    {
        LOG("StopRenderLoop");

        mRenderLoopWorker.Cancel();

        // Wait for current render pass to complete
        std::lock_guard<std::mutex> lock(mCriticalSection);
    }


    void VuforiaPage::Update()
    {
        ProcessInput();

        // Update scene objects
        mTimer.Tick([&]()
        {
            if (mRenderer->isRendererInitialized())
            {
                // Add per frame rendering updates here.
            }
        });
    }


    void VuforiaPage::ProcessInput()
    {
        // Add per frame input handling here.
    }


    bool VuforiaPage::Render()
    {
        // Don't try to render anything before the first Update.
        if (mTimer.GetFrameCount() == 0)
        {
            return false;
        }

        // Don't render before we finish loading renderer resources
        // (textures, meshes, shaders,...).
        if (!mRenderer->isRendererInitialized())
        {
            return false;
        }

        // Don't try to render before Vuforia is started
        if (!mVuforiaStarted)
        {
            return false;
        }

        if (mRenderingConfigurationChanged)
        {
            mRenderingConfigurationChanged = false;
            auto size = mDeviceResources->GetOutputSize();
            mController.configureRendering(int(size.Width), int(size.Height), /* Orientation is locked to landscape */ 2);
        }

        auto context = mDeviceResources->GetD3DDeviceContext();

        // Reset the viewport to target the whole screen.
        auto viewport = mDeviceResources->GetScreenViewport();
        context->RSSetViewports(1, &viewport);

        // Reset render targets to the screen.
        ID3D11RenderTargetView* const targets[1] = { mDeviceResources->GetBackBufferRenderTargetView() };
        context->OMSetRenderTargets(1, targets, mDeviceResources->GetDepthStencilView());

        // Clear the back buffer and depth stencil view.
        context->ClearRenderTargetView(mDeviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
        context->ClearDepthStencilView(mDeviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        Vuforia::DXTextureData textureData;
        Vuforia::DXTextureData* textureDataPtr = nullptr;
        if (!mRenderer->isVideoBackgroundTextureInitialized())
        {
            auto vbTextureSize = mController.getRenderingPrimitives()->getVideoBackgroundTextureSize();
            mRenderer->initVideoBackgroundTexture(vbTextureSize.data[0], vbTextureSize.data[1]);
            textureData.mData.mTexture2D = mRenderer->getVideoBackgroundTexture();
            textureDataPtr = &textureData;
        }

        double unusedViewport[6]; // TODO: Consider using these dimensions
        Vuforia::DXRenderData renderData(mDeviceResources->GetD3DDevice());
        if (mController.prepareToRender(unusedViewport, &renderData, nullptr, textureDataPtr))
        {
            auto renderingPrimitives = mController.getRenderingPrimitives();
            Vuforia::Matrix44F vbProjectionMatrix = Vuforia::Tool::convert2GLMatrix(
                renderingPrimitives->getVideoBackgroundProjectionMatrix(Vuforia::VIEW_SINGULAR));
            const Vuforia::Mesh& vbMesh = renderingPrimitives->getVideoBackgroundMesh(Vuforia::VIEW_SINGULAR);
            mRenderer->renderVideoBackground(vbProjectionMatrix,
                vbMesh.getNumVertices(),
                vbMesh.getPositions(), vbMesh.getUVs(),
                vbMesh.getNumTriangles(), vbMesh.getTriangles());

            mRenderer->prepareForAugmentationRendering();

            Vuforia::Matrix44F worldOriginProjection;
            Vuforia::Matrix44F worldOriginModelView;
            if (mController.getOrigin(worldOriginProjection, worldOriginModelView))
            {
                mRenderer->renderWorldOrigin(worldOriginProjection, worldOriginModelView);
            }

            Vuforia::Matrix44F trackableProjection;
            Vuforia::Matrix44F trackableModelView;
            Vuforia::Matrix44F trackableModelViewScaled;
            Vuforia::Image* modelTargetGuideViewImage = nullptr;
            if (mController.getImageTargetResult(trackableProjection, trackableModelView, trackableModelViewScaled))
            {
                mRenderer->renderImageTarget(trackableProjection, trackableModelView, trackableModelViewScaled);
            }
            else if (mController.getModelTargetResult(trackableProjection, trackableModelView, trackableModelViewScaled))
            {
                mRenderer->renderModelTarget(trackableProjection, trackableModelView, trackableModelViewScaled);
            }
            else if (mController.getModelTargetGuideView(trackableProjection, trackableModelView, &modelTargetGuideViewImage))
            {
                mRenderer->renderModelTargetGuideView(trackableProjection, trackableModelView, modelTargetGuideViewImage);
            }
        }
        mController.finishRender(&renderData);

        return true;
    }

} // namespace winrt::VuforiaSample::implementation
