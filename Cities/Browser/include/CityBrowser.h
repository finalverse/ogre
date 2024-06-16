/*
 -----------------------------------------------------------------------------
 This source file is part of OGRE
 (Object-oriented Graphics Rendering Engine)
 For the latest info, see http://www.ogre3d.org/
 
 Copyright (c) 2000-2014 Torus Knot Software Ltd
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 -----------------------------------------------------------------------------
 */
#ifndef __CityBrowser_H__
#define __CityBrowser_H__

#ifdef HAVE_IMGUI
#include "OgreImGuiOverlay.h"
#endif

#include "OgreOverlayManager.h"
#include "OgreRenderTargetListener.h"
#include "CityContext.h"
#include "CityPlugin.h"
#include "OgreTrays.h"
#include "OgreConfigFile.h"
#include "OgreTechnique.h"
#include "OgreArchiveManager.h"
#include "SdkCity.h"

#define ENABLE_SHADERS_CACHE 1

#if OGRE_PLATFORM == OGRE_PLATFORM_WINRT
#    include <sdkddkver.h>
#    if defined(_WIN32_WINNT) && _WIN32_WINNT == _WIN32_WINNT_WIN8
//      For WinRT 8.0 we only support running from the cache file.
#       undef ENABLE_SHADERS_CACHE
#       define ENABLE_SHADERS_CACHE 1
#    endif
#endif

#ifndef __OGRE_WINRT_PHONE
#define __OGRE_WINRT_PHONE 0
#endif

#ifdef OGRE_STATIC_LIB
#include "DefaultCitiesPlugin.h"
#   ifdef SAMPLES_INCLUDE_PLAYPEN
#   include "PlayPenTestPlugin.h"
#   endif
#endif

#define CAROUSEL_REDRAW_EPS 0.001

namespace OgreBites
{
    /*=============================================================================
      | The OGRE City Browser. Features a menu accessible from all cities,
      | dynamic configuration, resource reloading, node labeling, and more.
      =============================================================================*/
    class CityBrowser : public CityContext, public TrayListener, public Ogre::RenderTargetListener
    {
#ifdef OGRE_STATIC_LIB
        typedef std::map<Ogre::String, CityPlugin*> PluginMap;
        PluginMap mPluginNameMap;                      // A structure to map plugin names to class types
#endif
    public:

    CityBrowser(bool nograb = false, int startCityIndex = -1)
    : CityContext("OGRE City Browser"), mGrabInput(!nograb)
        {
            mIsShuttingDown = false;
            mTrayMgr = 0;
            mLastViewCategory = 0;
            mLastViewTitle = 0;
            mLastCityIndex = -1;
            mStartCityIndex = startCityIndex;
            mCategoryMenu = 0;
            mCityMenu = 0;
            mCitySlider = 0;
            mTitleLabel = 0;
            mDescBox = 0;
            mCarouselPlace = 0.0f;

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
            mGestureView = 0;
#endif
        }

        void loadStartUpCity() override
        {
            if (mStartCityIndex != -1)
            {
                runCityByIndex(mStartCityIndex);
                mStartCityIndex = -1;
            }
        }

        virtual void runCityByIndex(int idx)
        {
            runCity(Ogre::any_cast<City*>(mThumbs[idx]->getUserObjectBindings().getUserAny()));
        }

        /*-----------------------------------------------------------------------------
          | Extends runCity to handle creation and destruction of dummy scene.
          -----------------------------------------------------------------------------*/
        void runCity(City* s) override
        {
            if (mCurrentCity)  // sample quitting
            {
                mCurrentCity->_shutdown();
                mCurrentCity = 0;
                mCityPaused = false;     // don't pause next sample

                // create dummy scene and modify controls
                createDummyScene();
                mTrayMgr->showBackdrop("SdkTrays/Bands");
                mTrayMgr->showAll();
                ((Button*)mTrayMgr->getWidget("StartStop"))->setCaption("Start City");
            }

            if (s)  // sample starting
            {
                // destroy dummy scene and modify controls
                ((Button*)mTrayMgr->getWidget("StartStop"))->setCaption("Stop City");
                mTrayMgr->showBackdrop("SdkTrays/Shade");
                mTrayMgr->hideAll();
                destroyDummyScene();

                try
                {
                    CityContext::runCity(s);
                }
                catch (Ogre::Exception& e)   // if failed to start, show error and fall back to menu
                {
                    destroyDummyScene();

                    s->_shutdown();

                    createDummyScene();
                    mTrayMgr->showBackdrop("SdkTrays/Bands");
                    mTrayMgr->showAll();
                    ((Button*)mTrayMgr->getWidget("StartStop"))->setCaption("Start City");

                    mTrayMgr->showOkDialog("Error!", e.getDescription() + "\nSource: " + e.getSource());
                }
            }
        }

        /// catch any exceptions that might drop out of event handlers implemented by Cities
        bool frameStarted(const Ogre::FrameEvent& evt) override
        {
            try
            {
                return CityContext::frameStarted(evt);
            }
            catch (Ogre::Exception& e)   // show error and fall back to menu
            {
                runCity(0);
                mTrayMgr->showOkDialog("Error!", e.getDescription() + "\nSource: " + e.getSource());
            }

            return true;
        }

        /*-----------------------------------------------------------------------------
          | Extends frameRenderingQueued to update tray manager and carousel.
          -----------------------------------------------------------------------------*/
        bool frameRenderingQueued(const Ogre::FrameEvent& evt) override
        {
            // don't do all these calculations when sample's running or when in configuration screen or when no cities loaded
            if (!mLoadedCities.empty() && mTitleLabel->getTrayLocation() != TL_NONE && (!mCurrentCity || mCityPaused))
            {
                // makes the carousel spin smoothly toward its right position
                float carouselOffset = mCityMenu->getSelectionIndex() - mCarouselPlace;
                if (std::abs(carouselOffset) <= CAROUSEL_REDRAW_EPS) mCarouselPlace = mCityMenu->getSelectionIndex();
                else mCarouselPlace += carouselOffset * Ogre::Math::Clamp<float>(evt.timeSinceLastFrame * 15.0, -1.0, 1.0);

                // update the thumbnail positions based on carousel state
                for (int i = 0; i < (int)mThumbs.size(); i++)
                {
                    if(carouselOffset == 0) break;

                    Ogre::Real thumbOffset = mCarouselPlace - i;
                    Ogre::Real phase = (thumbOffset / 2.0) - 2.8;

                    if (thumbOffset < -5 || thumbOffset > 4)    // prevent thumbnails from wrapping around in a circle
                    {
                        mThumbs[i]->hide();
                        continue;
                    }
                    else mThumbs[i]->show();

                    Ogre::Real left = Ogre::Math::Cos(phase) * 200.0;
                    Ogre::Real top = Ogre::Math::Sin(phase) * 200.0;
                    Ogre::Real scale = 1.0 / Ogre::Math::Pow((Ogre::Math::Abs(thumbOffset) + 1.0), 0.75);

                    auto frame =
                        dynamic_cast<Ogre::BorderPanelOverlayElement*>(mThumbs[i]->getChildren().begin()->second);

                    mThumbs[i]->setDimensions(128.0 * scale, 96.0 * scale);
                    frame->setDimensions(mThumbs[i]->getWidth() + 16.0, mThumbs[i]->getHeight() + 16.0);
                    mThumbs[i]->setPosition((int)(left - 80.0 - (mThumbs[i]->getWidth() / 2.0)),
                                            (int)(top - 5.0 - (mThumbs[i]->getHeight() / 2.0)));
                    frame->setMaterial(nullptr); // dont draw inner region
                    if (i == mCityMenu->getSelectionIndex()) frame->setBorderMaterialName("SdkTrays/Frame/Over");
                    else frame->setBorderMaterialName("SdkTrays/Frame");
                }
            }

            mTrayMgr->frameRendered(evt);

            return CityContext::frameRenderingQueued(evt);
        }

        /*-----------------------------------------------------------------------------
          | Handles confirmation dialog responses.
          -----------------------------------------------------------------------------*/
        void yesNoDialogClosed(const Ogre::DisplayString& question, bool yesHit) override
        {
            if (question.substr(0, 14) == "This will stop" && yesHit)   // confirm unloading of cities
            {
                runCity(0);
                buttonHit((Button*)mTrayMgr->getWidget("UnloadReload"));
            }
        }

        /*-----------------------------------------------------------------------------
          | Handles button widget events.
          -----------------------------------------------------------------------------*/
        void buttonHit(Button* b) override
        {
            if (b->getName() == "StartStop")   // start or stop sample
            {
                if (b->getCaption() == "Start City")
                {
                    if (mLoadedCities.empty()) {
                        mTrayMgr->showOkDialog("Error!", "No sample selected!");
                    } else {
                        // use the sample pointer we stored inside the thumbnail
                        Ogre::Renderable* r = mThumbs[mCityMenu->getSelectionIndex()];
                        runCity(Ogre::any_cast<City*>(r->getUserObjectBindings().getUserAny()));
                    }
                } else
                    runCity(0);
            }
            else if (b->getName() == "UnloadReload")   // unload or reload sample plugins and update controls
            {
                if (b->getCaption() == "Unload Cities")
                {
                    if (mCurrentCity) mTrayMgr->showYesNoDialog("Warning!", "This will stop the current sample. Unload anyway?");
                    else
                    {
                        // save off current view and try to restore it on the next reload
                        mLastViewTitle = mCityMenu->getSelectionIndex();
                        mLastViewCategory = mCategoryMenu->getSelectionIndex();

                        unloadCities();
                        populateCityMenus();
                        b->setCaption("Reload Cities");
                    }
                }
                else
                {
                    loadCities();
                    populateCityMenus();
                    if (!mLoadedCities.empty()) b->setCaption("Unload Cities");

                    try  // attempt to restore the last view before unloading cities
                    {
                        mCategoryMenu->selectItem(mLastViewCategory);
                        mCityMenu->selectItem(mLastViewTitle);
                    }
                    catch (Ogre::Exception&) {}
                }
            }
#ifdef HAVE_IMGUI
            else if (b->getName() == "Configure")   // enter configuration screen
            {
                mOwnsImGuiOverlay = !Ogre::OverlayManager::getSingleton().getByName("ImGuiOverlay");
                auto imguiOverlay = initialiseImGui();
                imguiOverlay->addFont("SdkTrays/Caption", "Essential");
                imguiOverlay->setZOrder(300);
                imguiOverlay->show();

                if(mOwnsImGuiOverlay)
                {
                    ImGui::GetStyle().FrameRounding = 8;
                    ImVec4* colors = ImGui::GetStyle().Colors;
                    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.48f, 0.2f, 0.54f);
                    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.98f, 0.2f, 0.40f);
                    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.98f, 0.2f, 0.67f);
                    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.48f, 0.2f, 1.00f);
                    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.98f, 0.2f, 1.00f);
                    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.88f, 0.2f, 1.00f);
                    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.98f, 0.2f, 1.00f);
                    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.98f, 0.2f, 0.40f);
                    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.98f, 0.2f, 1.00f);
                    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.98f, 0.2f, 1.00f);
                    colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.98f, 0.2f, 0.31f);
                    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.98f, 0.2f, 0.80f);
                    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.98f, 0.2f, 1.00f);
                    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.98f, 0.2f, 0.20f);
                    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.98f, 0.2f, 0.67f);
                    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.98f, 0.2f, 0.95f);
                }

                mWindow->addListener(this);
                mInputListenerChain = TouchAgnosticInputListenerChain(mWindow, {this, mTrayMgr, getImGuiInputListener()});

                mTrayMgr->removeWidgetFromTray("StartStop");
                mTrayMgr->removeWidgetFromTray("Configure");
#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
                mTrayMgr->removeWidgetFromTray("UnloadReload");
                mTrayMgr->removeWidgetFromTray("Quit");
                mTrayMgr->moveWidgetToTray("Apply", TL_RIGHT);
#endif
                mTrayMgr->moveWidgetToTray("Back", TL_RIGHT);

                for (unsigned int i = 0; i < mThumbs.size(); i++)
                {
                    mThumbs[i]->hide();
                }

                while (mTrayMgr->getTrayContainer(TL_CENTER)->isVisible())
                {
                    mTrayMgr->removeWidgetFromTray(TL_CENTER, 0);
                }

                while (mTrayMgr->getTrayContainer(TL_LEFT)->isVisible())
                {
                    mTrayMgr->removeWidgetFromTray(TL_LEFT, 0);
                }

                windowResized(mWindow);
            }
            else if (b->getName() == "Back")   // leave configuration screen
            {
                if(mOwnsImGuiOverlay)
                    Ogre::OverlayManager::getSingleton().destroy("ImGuiOverlay"); // bring down overly to avoid interfering with cities
                else
                    Ogre::ImGuiOverlay::NewFrame(); // clear dialog

                mInputListenerChain = TouchAgnosticInputListenerChain(mWindow, {this, mTrayMgr});
                mWindow->removeListener(this);

                while (!mTrayMgr->getWidgets(TL_NONE).empty())
                {
                    mTrayMgr->moveWidgetToTray(TL_NONE, 0, TL_LEFT);
                }

                mTrayMgr->removeWidgetFromTray("Apply");
                mTrayMgr->removeWidgetFromTray("Back");

                mTrayMgr->moveWidgetToTray("StartStop", TL_RIGHT);
#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
                mTrayMgr->moveWidgetToTray("UnloadReload", TL_RIGHT);
#endif
                mTrayMgr->moveWidgetToTray("Configure", TL_RIGHT);
#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
                mTrayMgr->moveWidgetToTray("Quit", TL_RIGHT);
#endif

                mCarouselPlace += CAROUSEL_REDRAW_EPS;  // force redraw
                windowResized(mWindow);
            }
#endif
            else if (b->getName() == "Apply")   // apply any changes made in the configuration screen
            {
                reconfigure(mNextRenderer);
            }
            else
            {
                mRoot->queueEndRendering();   // exit browser
            }
        }

        void preViewportUpdate(const Ogre::RenderTargetViewportEvent& evt) override
        {
#ifdef HAVE_IMGUI
            Ogre::ImGuiOverlay::NewFrame();

            auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
            auto center = ImGui::GetMainViewport()->GetCenter();
            if(mWindow->getWidth() <= 1280)
                ImGui::SetNextWindowPos(ImVec2(0, center.y), ImGuiCond_Always, ImVec2(0.f, 0.5f));
            else
                ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Configuration", NULL, flags);
            Ogre::DrawRenderingSettings(mNextRenderer);
            ImGui::End();
#endif
        }

        /*-----------------------------------------------------------------------------
          | Handles menu item selection changes.
          -----------------------------------------------------------------------------*/
        void itemSelected(SelectMenu* menu) override
        {
            if (menu == mCategoryMenu)      // category changed, so update the sample menu, carousel, and slider
            {
                for (unsigned int i = 0; i < mThumbs.size(); i++)    // destroy all thumbnails in carousel
                {
                    Ogre::MaterialManager::getSingleton().remove(mThumbs[i]->getName(), "Essential");
                    Widget::nukeOverlayElement(mThumbs[i]);
                }
                mThumbs.clear();

                Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();
                Ogre::String selectedCategory;

                if (menu->getSelectionIndex() != -1) selectedCategory = menu->getSelectedItem();
                else
                {
                    mTitleLabel->setCaption("");
                    mDescBox->setText("");
                }

                bool all = selectedCategory == "All";
                Ogre::StringVector sampleTitles;
                Ogre::MaterialPtr templateMat = Ogre::MaterialManager::getSingleton().getByName("SdkTrays/CityThumbnail");

                // populate the sample menu and carousel with filtered cities
                for (CitySet::iterator i = mLoadedCities.begin(); i != mLoadedCities.end(); i++)
                {
                    Ogre::NameValuePairList& info = (*i)->getInfo();

                    if (all || info["Category"] == selectedCategory)
                    {
                        Ogre::String name = "SdkTrays/CityThumb" + Ogre::StringConverter::toString(sampleTitles.size() + 1);

                        // clone a new material for sample thumbnail
                        Ogre::MaterialPtr newMat = templateMat->clone(name);

                        Ogre::TextureUnitState* tus = newMat->getTechnique(0)->getPass(0)->getTextureUnitState(0);
                        tus->setTextureName(info["Thumbnail"]);

                        // create sample thumbnail overlay
                        auto bp = dynamic_cast<Ogre::PanelOverlayElement*>(
                            om.createOverlayElementFromTemplate("SdkTrays/Picture", "", name));
                        bp->setHorizontalAlignment(Ogre::GHA_RIGHT);
                        bp->setVerticalAlignment(Ogre::GVA_CENTER);
                        bp->setMaterialName(name);
                        bp->getUserObjectBindings().setUserAny(*i);
                        mTrayMgr->getTraysLayer()->add2D(bp);

                        // add sample thumbnail and title
                        mThumbs.push_back(bp);
                        sampleTitles.push_back((*i)->getInfo()["Title"]);
                    }
                }

                mCarouselPlace = CAROUSEL_REDRAW_EPS;  // reset carousel

                mCityMenu->setItems(sampleTitles);
                if (mCityMenu->getNumItems() != 0) itemSelected(mCityMenu);

                mCitySlider->setRange(1, static_cast<Ogre::Real>(sampleTitles.size()), static_cast<Ogre::Real>(sampleTitles.size()));
            }
            else if (menu == mCityMenu)    // sample changed, so update slider, label and description
            {
                if (mCitySlider->getValue() != menu->getSelectionIndex() + 1)
                    mCitySlider->setValue(menu->getSelectionIndex() + 1);

                Ogre::Renderable* r = mThumbs[mCityMenu->getSelectionIndex()];
                City* s = Ogre::any_cast<City*>(r->getUserObjectBindings().getUserAny());
                mTitleLabel->setCaption(menu->getSelectedItem());
                mDescBox->setText("Category: " + s->getInfo()["Category"] + "\nDescription: " + s->getInfo()["Description"]);

                if (mCurrentCity != s) ((Button*)mTrayMgr->getWidget("StartStop"))->setCaption("Start City");
                else ((Button*)mTrayMgr->getWidget("StartStop"))->setCaption("Stop City");
            }
        }

        /*-----------------------------------------------------------------------------
          | Handles sample slider changes.
          -----------------------------------------------------------------------------*/
        void sliderMoved(Slider* slider) override
        {
            // format the caption to be fraction style
            Ogre::String denom = "/" + Ogre::StringConverter::toString(mCityMenu->getNumItems());
            slider->setValueCaption(slider->getValueCaption() + denom);

            // tell the sample menu to change if it hasn't already
            if (mCityMenu->getSelectionIndex() != -1 && mCityMenu->getSelectionIndex() != slider->getValue() - 1)
                mCityMenu->selectItem(slider->getValue() - 1);
        }

        /*-----------------------------------------------------------------------------
          | Handles keypresses.
          -----------------------------------------------------------------------------*/
        bool keyPressed(const KeyboardEvent& evt) override
        {
            if (mTrayMgr->isDialogVisible()) return true;  // ignore keypresses when dialog is showing

            Keycode key = evt.keysym.sym;

            if (key == SDLK_ESCAPE)
            {
#if __OGRE_WINRT_PHONE
                // If there is a quit button, assume that we intended to press it via 'ESC'.
                if (mTrayMgr->areTraysVisible())
                {
                    Widget *pWidget = mTrayMgr->getWidget("Quit");
                    if (pWidget)
                    {
                        buttonHit((Button*)pWidget);  // on phone, quit entirely.
                        return false;  // now act as if we didn't handle the button to get AppModel to exit.
                    }
                }
#endif // __OGRE_WINRT_PHONE
                if (mTitleLabel->getTrayLocation() != TL_NONE)
                {
                    // if we're in the main screen and a sample's running, toggle sample pause state
                    if (mCurrentCity)
                    {
                        if (mCityPaused)
                        {
                            mTrayMgr->hideAll();
                            unpauseCurrentCity();
                        }
                        else
                        {
                            pauseCurrentCity();
                            mTrayMgr->showAll();
                        }
                    }
                }
                else buttonHit((Button*)mTrayMgr->getWidget("Back"));  // if we're in config, just go back
            }
            else if ((key == SDLK_UP || key == SDLK_DOWN) && mTitleLabel->getTrayLocation() != TL_NONE)
            {
                // if we're in the main screen, use the up and down arrow keys to cycle through cities
                int newIndex = mCityMenu->getSelectionIndex() + (key == SDLK_UP ? -1 : 1);
                mCityMenu->selectItem(Ogre::Math::Clamp<int>(newIndex, 0, mCityMenu->getNumItems() - 1));
            }
            else if (key == SDLK_RETURN)   // start or stop sample
            {
                if (!mLoadedCities.empty() && (mCityPaused || mCurrentCity == 0))
                {
                    Ogre::Renderable* r = mThumbs[mCityMenu->getSelectionIndex()];
                    City* newCity = Ogre::any_cast<City*>(r->getUserObjectBindings().getUserAny());
                    runCity(newCity == mCurrentCity ? 0 : newCity);
                }
            }
            else if(key == SDLK_F9)   // toggle full screen
            {
                // Make sure we use the window size as originally requested, NOT the
                // current window size (which may have altered to fit desktop)
                auto desc = mRoot->getRenderSystem()->getRenderWindowDescription();
                mWindow->setFullscreen(!mWindow->isFullScreen(), desc.width, desc.height);
            }
            else if(key == SDLK_F11 || key == SDLK_F12) // Decrease and increase FSAA level on the fly
            {
                // current FSAA                0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
                unsigned decreasedFSAA[17] = { 0, 0, 1, 2, 2, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8 };
                unsigned increasedFSAA[17] = { 2, 2, 4, 4, 8, 8, 8, 8,16,16,16,16,16,16,16,16, 0, };
                unsigned FSAA = std::min(mWindow->getFSAA(), 16U);
                unsigned newFSAA = (key == SDLK_F12) ? increasedFSAA[FSAA] : decreasedFSAA[FSAA];
                if(newFSAA != 0)
                    mWindow->setFSAA(newFSAA, mWindow->getFSAAHint());
            }

            return CityContext::keyPressed(evt);
        }

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        void motionBegan( void )
        {
        }

        void motionEnded( void )
        {
            if (mTrayMgr->isDialogVisible()) return;  // ignore keypresses when dialog is showing

            if (mTitleLabel->getTrayLocation() != TL_NONE)
            {
                // if we're in the main screen and a sample's running, toggle sample pause state
                if (mCurrentCity)
                {
                    if (mCityPaused)
                    {
                        mTrayMgr->hideAll();
                        unpauseCurrentCity();
                    }
                    else
                    {
                        pauseCurrentCity();
                        mTrayMgr->showAll();
                    }
                }
            }
            else buttonHit((Button*)mTrayMgr->getWidget("Back"));  // if we're in config, just go back

        }

        void motionCancelled( void )
        {
        }
#endif

        /*-----------------------------------------------------------------------------
          | Extends pointerPressed to inject mouse press into tray manager, and to check
          | for thumbnail clicks, just because we can.
          -----------------------------------------------------------------------------*/
        bool mousePressed(const MouseButtonEvent& evt) override
        {
            if (mTitleLabel->getTrayLocation() != TL_NONE)
            {
                for (unsigned int i = 0; i < mThumbs.size(); i++)
                {
                    if (mThumbs[i]->isVisible() && Widget::isCursorOver(mThumbs[i],
                        Ogre::Vector2(mTrayMgr->getCursorContainer()->getLeft(), mTrayMgr->getCursorContainer()->getTop()), 0))
                    {
                        mCityMenu->selectItem(i);
                        return true;
                    }
                }
            }

            if(!isCurrentCityPaused()) return true; // stop propagation
            return false;
        }

        bool buttonPressed(const ButtonEvent& evt) override
        {
            KeyboardEvent e;
            e.keysym.sym = 0;
            switch (evt.button)
            {
            case 0:
                e.keysym.sym = SDLK_RETURN;
                break;
            case 1:
                e.keysym.sym = SDLK_ESCAPE;
                break;
            case 11:
                e.keysym.sym = SDLK_UP;
                break;
            case 12:
                e.keysym.sym = SDLK_DOWN;
                break;
            }
            return keyPressed(e);
        }

        /*-----------------------------------------------------------------------------
          | Extends pointerReleased to inject mouse release into tray manager.
          -----------------------------------------------------------------------------*/
        bool mouseReleased(const MouseButtonEvent& evt) override
        {
            if(!isCurrentCityPaused()) return true; // stop propagation
            return false;
        }

        /*-----------------------------------------------------------------------------
          | Extends pointerMoved to inject mouse position into tray manager, and checks
          | for mouse wheel movements to slide the carousel, because we can.
          -----------------------------------------------------------------------------*/
        bool mouseMoved(const MouseMotionEvent& evt) override
        {
            if(!isCurrentCityPaused()) return true; // stop propagation
            return false;
        }

        //TODO: Handle iOS and Android.
        /** Mouse wheel scrolls the sample list.
         */
        bool mouseWheelRolled(const MouseWheelEvent& evt) override
        {
            if(mTrayMgr->mouseWheelRolled(evt))
                return true;

            if(!isCurrentCityPaused()) return true; // stop propagation

            if (mTitleLabel->getTrayLocation() != TL_NONE && mCityMenu->getNumItems() != 0)
            {
                int newIndex = mCityMenu->getSelectionIndex() - evt.y / Ogre::Math::Abs(evt.y);
                mCityMenu->selectItem(Ogre::Math::Clamp<int>(newIndex, 0, mCityMenu->getNumItems() - 1));
            }

            return CityContext::mouseWheelRolled(evt);
        }

        /*-----------------------------------------------------------------------------
          | Extends windowResized to best fit menus on screen. We basically move the
          | menu tray to the left for higher resolutions and move it to the center
          | for lower resolutions.
          -----------------------------------------------------------------------------*/
        void windowResized(Ogre::RenderWindow* rw) override
        {
            if (!mTrayMgr) return;

            Ogre::OverlayContainer* center = mTrayMgr->getTrayContainer(TL_CENTER);
            Ogre::OverlayContainer* left = mTrayMgr->getTrayContainer(TL_LEFT);

            if (center->isVisible() && rw->getWidth() < 1280 - center->getWidth())
            {
                while (center->isVisible())
                {
                    mTrayMgr->moveWidgetToTray(mTrayMgr->getWidgets(TL_CENTER)[0], TL_LEFT);
                }
            }
            else if (left->isVisible() && rw->getWidth() >= 1280 - left->getWidth())
            {
                while (left->isVisible())
                {
                    mTrayMgr->moveWidgetToTray(mTrayMgr->getWidgets(TL_LEFT)[0], TL_CENTER);
                }
            }

            CityContext::windowResized(rw);
        }

        /*-----------------------------------------------------------------------------
          | Extends setup to create dummy scene and tray interface.
          -----------------------------------------------------------------------------*/
        void setup() override
        {
            ApplicationContext::setup();
            mWindow = getRenderWindow();

            mInputListenerChain = TouchAgnosticInputListenerChain(mWindow, {this, mTrayMgr});
            addInputListener(&mInputListenerChain);
            if(mGrabInput) setWindowGrab();
            else mTrayMgr->hideCursor();
#ifdef OGRE_STATIC_LIB
            mPluginNameMap["DefaultCities"] = new DefaultCitiesPlugin();
#   ifdef SAMPLES_INCLUDE_PLAYPEN
            mPluginNameMap["PlaypenTests"] = new  PlaypenTestPlugin();
#   endif
#endif

            City* startupCity = loadCities();

            // create template material for sample thumbnails
            Ogre::MaterialPtr thumbMat = Ogre::MaterialManager::getSingleton().create("SdkTrays/CityThumbnail", "Essential");
            thumbMat->setLightingEnabled(false);
            thumbMat->setDepthCheckEnabled(false);
            thumbMat->getTechnique(0)->getPass(0)->createTextureUnitState();

            setupWidgets();
            windowResized(mWindow);   // adjust menus for resolution

            // if this is our first time running, and there's a startup sample, run it
            if (startupCity && mFirstRun){
                runCity(startupCity);
            }
        }

    protected:
        /*-----------------------------------------------------------------------------
          | Overrides the default window title.
          -----------------------------------------------------------------------------*/
        NativeWindowPair createWindow(const Ogre::String& name, uint32_t w, uint32_t h, Ogre::NameValuePairList miscParams) override
        {
#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID
            // Make sure Trays are not tiny -  we cannot easily scale the UI, therefore just reduce resolution
            float contentScaling = AConfiguration_getDensity(mAConfig)/float(ACONFIGURATION_DENSITY_HIGH);
            if(contentScaling > 1.0)
            {
                miscParams["contentScalingFactor"] = std::to_string(contentScaling);
                miscParams["FSAA"] = "2";
            }
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
            mGestureView = [[CityBrowserGestureView alloc] init];
            mGestureView.mBrowser = this;

            [[[UIApplication sharedApplication] keyWindow] addSubview:mGestureView];
#endif

            return ApplicationContext::createWindow(name, w, h, miscParams);
        }

        /*-----------------------------------------------------------------------------
          | Initialises only the browser's resources and those most commonly used
          | by cities. This way, additional special content can be initialised by
          | the cities that use them, so startup time is unaffected.
          -----------------------------------------------------------------------------*/
        void loadResources() override
        {
#if OGRE_PLATFORM != OGRE_PLATFORM_APPLE
            Ogre::OverlayManager::getSingleton().setPixelRatio(getDisplayDPI()/96);
#endif

            Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup("Essential");
            mTrayMgr = new TrayManager("BrowserControls", getRenderWindow(), this);
            mTrayMgr->showBackdrop("SdkTrays/Bands");
            mTrayMgr->getTrayContainer(TL_NONE)->hide();

#if ENABLE_SHADERS_CACHE == 1
            enableShaderCache();
#endif
            // Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);

            createDummyScene();

            mTrayMgr->showLoadingBar(1, 0);
            Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
            mTrayMgr->hideLoadingBar();
        }

        /*-----------------------------------------------------------------------------
          | Loads sample plugins from a configuration file.
          -----------------------------------------------------------------------------*/
        virtual City* loadCities()
        {
            City* startupCity = 0;
            Ogre::StringVector unloadedCityPlugins;

            Ogre::String startupCityTitle;
            Ogre::StringVector sampleList;
#ifdef OGRE_STATIC_LIB
            for(auto it = mPluginNameMap.begin(); it != mPluginNameMap.end(); ++it)
            {
                sampleList.push_back(it->first);
            }
#else
            Ogre::ConfigFile cfg;
#   if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID
            Ogre::Archive* apk = Ogre::ArchiveManager::getSingleton().load("", "APKFileSystem", true);
            cfg.load(apk->open(mFSLayer->getConfigFilePath("cities.cfg")));
#   else
            cfg.load(mFSLayer->getConfigFilePath("cities.cfg"));
#   endif

            Ogre::String sampleDir = cfg.getSetting("CityFolder");        // Mac OS X just uses Resources/ directory
            sampleList = cfg.getMultiSetting("CityPlugin");
            startupCityTitle = cfg.getSetting("StartupCity");

            sampleDir = Ogre::FileSystemLayer::resolveBundlePath(sampleDir);

#   if OGRE_PLATFORM != OGRE_PLATFORM_APPLE && OGRE_PLATFORM != OGRE_PLATFORM_APPLE_IOS
            if (sampleDir.empty()) sampleDir = ".";   // user didn't specify plugins folder, try current one
#   endif

            // add slash or backslash based on platform
            char lastChar = sampleDir[sampleDir.length() - 1];
            if (lastChar != '/' && lastChar != '\\')
            {
#   if OGRE_PLATFORM == OGRE_PLATFORM_WIN32 || (OGRE_PLATFORM == OGRE_PLATFORM_WINRT)
                sampleDir += "\\";
#   else
                sampleDir += "/";
#   endif
            }
#endif

            CitySet newCities;

            // loop through all sample plugins...
            for (Ogre::StringVector::iterator i = sampleList.begin(); i != sampleList.end(); i++)
            {
#ifndef OGRE_STATIC_LIB
                try   // try to load the plugin
                {
                    mRoot->loadPlugin(sampleDir + *i);
                }
                catch (Ogre::Exception& e)   // plugin couldn't be loaded
                {
                    Ogre::LogManager::getSingleton().logError(e.what());
                    unloadedCityPlugins.push_back(sampleDir + *i);
                    continue;
                }

                Ogre::Plugin* p = mRoot->getInstalledPlugins().back();   // acquire plugin instance
                CityPlugin* sp = dynamic_cast<CityPlugin*>(p);

                if (!sp)  // this is not a CityPlugin, so unload it
                {
                    unloadedCityPlugins.push_back(sampleDir + *i);
                    mRoot->unloadPlugin(sampleDir + *i);
                    continue;
                }

                mLoadedCityPlugins.push_back(sampleDir + *i);   // add to records
#else
                CityPlugin* sp = mPluginNameMap[*i];
#endif

                // go through every sample in the plugin...
                newCities = sp->getCities();
                for (CitySet::iterator j = newCities.begin(); j != newCities.end(); j++)
                {
                    Ogre::NameValuePairList& info = (*j)->getInfo();   // acquire custom sample info

                    mLoadedCities.insert(*j);                    // add sample only after ensuring title for sorting
                    mCityCategories.insert(info["Category"]);   // add sample category

                    if (info["Title"] == startupCityTitle) startupCity = *j;   // we found the startup sample
                }
            }

            if (!mLoadedCities.empty()) mCityCategories.insert("All");   // insert a category for all cities

            if (!unloadedCityPlugins.empty())   // show error message summarising missing or invalid plugins
            {
                Ogre::String message = "These requested sample plugins were either missing, corrupt or invalid:";

                for (unsigned int i = 0; i < unloadedCityPlugins.size(); i++)
                {
                    message += "\n- " + unloadedCityPlugins[i];
                }

                mTrayMgr->showOkDialog("Error!", message);
            }

            return startupCity;
        }

        /*-----------------------------------------------------------------------------
          | Unloads any loaded sample plugins.
          -----------------------------------------------------------------------------*/
        virtual void unloadCities()
        {
#ifndef OGRE_STATIC_LIB
            for (unsigned int i = 0; i < mLoadedCityPlugins.size(); i++)
            {
                mRoot->unloadPlugin(mLoadedCityPlugins[i]);
            }
#endif

            mLoadedCities.clear();
            mLoadedCityPlugins.clear();
            mCityCategories.clear();
        }

        /*-----------------------------------------------------------------------------
          | Sets up main page for browsing cities.
          -----------------------------------------------------------------------------*/
        virtual void setupWidgets()
        {
            mTrayMgr->destroyAllWidgets();

            // create main navigation tray
            mTrayMgr->showLogo(TL_RIGHT);
            mTrayMgr->createSeparator(TL_RIGHT, "LogoSep");
            mTrayMgr->createButton(TL_RIGHT, "StartStop", "Start City", 120);

#if (OGRE_PLATFORM != OGRE_PLATFORM_WINRT) && (OGRE_PLATFORM != OGRE_PLATFORM_ANDROID)
            mTrayMgr->createButton(TL_RIGHT, "UnloadReload", mLoadedCities.empty() ? "Reload Cities" : "Unload Cities");
#endif
#if (OGRE_PLATFORM != OGRE_PLATFORM_WINRT) && defined(HAVE_IMGUI)
            mTrayMgr->createButton(TL_RIGHT, "Configure", "Configure");
#endif
#if (OGRE_PLATFORM != OGRE_PLATFORM_ANDROID)
            mTrayMgr->createButton(TL_RIGHT, "Quit", "Quit");
#endif

            // create sample viewing controls
            float infoWidth = 250;
#if (OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS) || (OGRE_PLATFORM == OGRE_PLATFORM_ANDROID)
            infoWidth *= 0.9;
#endif
            mTitleLabel = mTrayMgr->createLabel(TL_LEFT, "CityTitle", "");
            mDescBox = mTrayMgr->createTextBox(TL_LEFT, "CityInfo", "City Info", infoWidth, 208);
            mCategoryMenu = mTrayMgr->createThickSelectMenu(TL_LEFT, "CategoryMenu", "Select Category", infoWidth, 10);
            mCityMenu = mTrayMgr->createThickSelectMenu(TL_LEFT, "CityMenu", "Select City", infoWidth, 10);
            mCitySlider = mTrayMgr->createThickSlider(TL_LEFT, "CitySlider", "Slide Cities", infoWidth, 80, 0, 0, 0);

            /* Sliders do not notify their listeners on creation, so we manually call the callback here
               to format the slider value correctly. */
            sliderMoved(mCitySlider);

            // create configuration screen button tray
            mTrayMgr->createButton(TL_NONE, "Apply", "Apply Changes");
            mTrayMgr->createButton(TL_NONE, "Back", "Go Back");

            populateCityMenus();
        }

        /*-----------------------------------------------------------------------------
          | Populates home menus with loaded cities.
          -----------------------------------------------------------------------------*/
        virtual void populateCityMenus()
        {
            Ogre::StringVector categories;
            for (std::set<Ogre::String>::iterator i = mCityCategories.begin(); i != mCityCategories.end(); i++)
                categories.push_back(*i);

            mCategoryMenu->setItems(categories);
            if (mCategoryMenu->getNumItems() != 0)
                mCategoryMenu->selectItem(0);
            else
                itemSelected(mCategoryMenu);   // if there are no items, we can't select one, so manually invoke callback

            mCarouselPlace = CAROUSEL_REDRAW_EPS; // force redraw
        }

        /*-----------------------------------------------------------------------------
          | Overrides to recover by last sample's index instead.
          -----------------------------------------------------------------------------*/
        void recoverLastCity() override
        {
            // restore the view while we're at it too
            mCategoryMenu->selectItem(mLastViewCategory);
            mCityMenu->selectItem(mLastViewTitle);

            if (mLastCityIndex != -1)
            {
                int index = -1;
                for (CitySet::iterator i = mLoadedCities.begin(); i != mLoadedCities.end(); i++)
                {
                    index++;
                    if (index == mLastCityIndex)
                    {
                        runCity(*i);
                        (*i)->restoreState(mLastCityState);
                        mLastCity = 0;
                        mLastCityIndex = -1;
                        mLastCityState.clear();
                    }
                }

                pauseCurrentCity();
                mTrayMgr->showAll();
            }

            buttonHit((Button*)mTrayMgr->getWidget("Configure"));
        }

        /*-----------------------------------------------------------------------------
          | Extends reconfigure to save the view and the index of last sample run.
          -----------------------------------------------------------------------------*/
        void reconfigure(const Ogre::String& renderer)
        {
            mLastViewCategory = mCategoryMenu->getSelectionIndex();
            mLastViewTitle = mCityMenu->getSelectionIndex();

            mLastCityIndex = -1;
            unsigned int index = -1;
            for (CitySet::iterator i = mLoadedCities.begin(); i != mLoadedCities.end(); i++)
            {
                index++;
                if (*i == mCurrentCity)
                {
                    mLastCityIndex = index;
                    break;
                }
            }

            CityContext::reconfigure(renderer);
        }
    public:
        /*-----------------------------------------------------------------------------
          | Extends shutdown to destroy dummy scene and tray interface.
          -----------------------------------------------------------------------------*/
        void shutdown() override
        {
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
            [mGestureView release];
#endif
            if (mTrayMgr)
            {
                delete mTrayMgr;
                mTrayMgr = 0;
            }

            if (!mCurrentCity && mRoot->getRenderSystem() != NULL) destroyDummyScene();

            CityContext::shutdown();

            mCategoryMenu = 0;
            mCityMenu = 0;
            mCitySlider = 0;
            mTitleLabel = 0;
            mDescBox = 0;
            mHiddenOverlays.clear();
            mThumbs.clear();
            mCarouselPlace = 0;
            mWindow = 0;

            unloadCities();
        }
    protected:
        /*-----------------------------------------------------------------------------
          | Extend to temporarily hide a sample's overlays while in the pause menu.
          -----------------------------------------------------------------------------*/
        void pauseCurrentCity() override
        {
            CityContext::pauseCurrentCity();

            Ogre::OverlayManager::OverlayMapIterator it = Ogre::OverlayManager::getSingleton().getOverlayIterator();
            mHiddenOverlays.clear();

            while (it.hasMoreElements())
            {
                Ogre::Overlay* o = it.getNext();
                if (o->isVisible())                  // later, we don't want to unhide the initially hidden overlays
                {
                    mHiddenOverlays.push_back(o);    // save off hidden overlays so we can unhide them later
                    o->hide();
                }
            }
        }

        /*-----------------------------------------------------------------------------
        | Extend to unhide all of sample's temporarily hidden overlays.
          -----------------------------------------------------------------------------*/
        void unpauseCurrentCity() override
        {
            CityContext::unpauseCurrentCity();

            for (std::vector<Ogre::Overlay*>::iterator i = mHiddenOverlays.begin(); i != mHiddenOverlays.end(); i++)
            {
                (*i)->show();
            }

            mHiddenOverlays.clear();
        }

        TrayManager* mTrayMgr;                      // SDK tray interface
        Ogre::StringVector mLoadedCityPlugins;       // loaded sample plugins
        std::set<Ogre::String> mCityCategories;      // sample categories
        CitySet mLoadedCities;                      // loaded cities
        SelectMenu* mCategoryMenu;                     // sample category select menu
        SelectMenu* mCityMenu;                       // sample select menu
        Slider* mCitySlider;                         // sample slider bar
        Label* mTitleLabel;                            // sample title label
        TextBox* mDescBox;                             // sample description box
        std::vector<Ogre::Overlay*> mHiddenOverlays;   // sample overlays hidden for pausing
        std::vector<Ogre::OverlayContainer*> mThumbs;  // sample thumbnails
        Ogre::Real mCarouselPlace;                     // current state of carousel
        int mLastViewTitle;                            // last sample title viewed
        int mLastViewCategory;                         // last sample category viewed
        int mLastCityIndex;                          // index of last sample running
        int mStartCityIndex;                         // directly starts the sample with the given index
        TouchAgnosticInputListenerChain mInputListenerChain;
    public:
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        CityBrowserGestureView *mGestureView;
#endif
        bool mIsShuttingDown;
        bool mGrabInput;
        bool mOwnsImGuiOverlay;
    };
}

#endif
