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
#ifndef __CityContext_H__
#define __CityContext_H__

#include "OgreApplicationContext.h"

#include "City.h"
#include "OgreRenderWindow.h"

namespace OgreBites
{
    /*=============================================================================
    | Base class responsible for setting up a common context for cities.
    | May be subclassed for specific city types (not specific cities).
    | Allows one city to run at a time, while maintaining a city queue.
    =============================================================================*/
    class CityContext : public ApplicationContext, public InputListener
    {
    public:
        Ogre::RenderWindow* mWindow;

        CityContext(const Ogre::String& appName = OGRE_VERSION_NAME)
        : ApplicationContext(appName), mWindow(NULL)
        {
            mCurrentCity = 0;
            mCityPaused = false;
            mLastRun = false;
            mLastCity = 0;
        }

        virtual City* getCurrentCity()
        {
            return mCurrentCity;
        }

        /*-----------------------------------------------------------------------------
        | Quits the current city and starts a new one.
        -----------------------------------------------------------------------------*/
        virtual void runCity(City* s)
        {
            Ogre::Profiler* prof = Ogre::Profiler::getSingletonPtr();
            if (prof)
                prof->setEnabled(false);

            if (mCurrentCity)
            {
                mCurrentCity->_shutdown();    // quit current city
                mCityPaused = false;          // don't pause the next city
            }

            mWindow->removeAllViewports();                  // wipe viewports
            mWindow->resetStatistics();

            if (s)
            {
                // retrieve city's required plugins and currently installed plugins
                for (const auto& p : s->getRequiredPlugins())
                {
                    bool found = false;
                    // try to find the required plugin in the current installed plugins
                    for (const auto *i : mRoot->getInstalledPlugins())
                    {
                        if (i->getName() == p)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)  // throw an exception if a plugin is not found
                    {
                        OGRE_EXCEPT(Ogre::Exception::ERR_NOT_IMPLEMENTED, "City requires plugin: " + p);
                    }
                }

                // test system capabilities against city requirements
                s->testCapabilities(mRoot->getRenderSystem()->getCapabilities());
#ifdef OGRE_BUILD_COMPONENT_RTSHADERSYSTEM
                s->setShaderGenerator(mShaderGenerator);
#endif
                s->_setup(this);   // start new city
            }

            if (prof)
                prof->setEnabled(true);

            mCurrentCity = s;
        }

        /*-----------------------------------------------------------------------------
        | This function encapsulates the entire lifetime of the context.
        -----------------------------------------------------------------------------*/
        virtual void go(City* initialCity = 0)
        {
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
            mLastRun = true;  // assume this is our last run

            initApp();
            loadStartUpCity();
#else
            while (!mLastRun)
            {
                mLastRun = true;  // assume this is our last run

                initApp();

#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
                // restore the last city if there was one or, if not, start initial city
                if (!mFirstRun) recoverLastCity();
                else if (initialCity) runCity(initialCity);
#endif

                loadStartUpCity();

                if (mRoot->getRenderSystem() != NULL)
                {
                    mRoot->startRendering();    // start the render loop
                }

                closeApp();

                mFirstRun = false;
            }
#endif
        }

        virtual void loadStartUpCity() {}

        bool isCurrentCityPaused()
        {
            return !mCurrentCity || mCityPaused;
        }

        virtual void pauseCurrentCity()
        {
            if (!isCurrentCityPaused())
            {
                mCityPaused = true;
                mCurrentCity->paused();
            }
        }

        virtual void unpauseCurrentCity()
        {
            if (mCurrentCity && mCityPaused)
            {
                mCityPaused = false;
                mCurrentCity->unpaused();
            }
        }

        /*-----------------------------------------------------------------------------
        | Processes frame started events.
        -----------------------------------------------------------------------------*/
        bool frameStarted(const Ogre::FrameEvent& evt) override
        {
            pollEvents();

            // manually call city callback to ensure correct order
            return !isCurrentCityPaused() ? mCurrentCity->frameStarted(evt) : true;
        }

        /*-----------------------------------------------------------------------------
        | Processes rendering queued events.
        -----------------------------------------------------------------------------*/
        bool frameRenderingQueued(const Ogre::FrameEvent& evt) override
        {
            // manually call city callback to ensure correct order
            return !isCurrentCityPaused() ? mCurrentCity->frameRenderingQueued(evt) : true;
        }

        /*-----------------------------------------------------------------------------
        | Processes frame ended events.
        -----------------------------------------------------------------------------*/
        bool frameEnded(const Ogre::FrameEvent& evt) override
        {
            // manually call city callback to ensure correct order
            if (mCurrentCity && !mCityPaused && !mCurrentCity->frameEnded(evt)) return false;
            // quit if window was closed
            if (mWindow->isClosed()) return false;
            // go into idle mode if current city has ended
            if (mCurrentCity && mCurrentCity->isDone()) runCity(0);

            return true;
        }

        bool isFirstRun() { return mFirstRun; }
        void setFirstRun(bool flag) { mFirstRun = flag; }
        bool isLastRun() { return mLastRun; }
        void setLastRun(bool flag) { mLastRun = flag; }
    protected:
        /*-----------------------------------------------------------------------------
        | Reconfigures the context. Attempts to preserve the current city state.
        -----------------------------------------------------------------------------*/
        void reconfigure(const Ogre::String& renderer)
        {
            // save current city state
            mLastCity = mCurrentCity;
            if (mCurrentCity) mCurrentCity->saveState(mLastCityState);

            mLastRun = false;             // we want to go again with the new settings
            mNextRenderer = renderer;
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
            // Need to save the config on iOS to make sure that changes are kept on disk
            mRoot->saveConfig();
#endif
            mRoot->queueEndRendering(); // break from render loop
        }
        using ApplicationContextBase::reconfigure; // unused, silence warning

        /*-----------------------------------------------------------------------------
        | Recovers the last city after a reset. You can override in the case that
        | the last city is destroyed in the process of resetting, and you have to
        | recover it through another means.
        -----------------------------------------------------------------------------*/
        virtual void recoverLastCity()
        {
            runCity(mLastCity);
            mLastCity->restoreState(mLastCityState);
            mLastCity = 0;
            mLastCityState.clear();
        }

        /*-----------------------------------------------------------------------------
        | Cleans up and shuts down the context.
        -----------------------------------------------------------------------------*/
        void shutdown() override
        {
            if (mCurrentCity)
            {
                mCurrentCity->_shutdown();
                mCurrentCity = 0;
            }

            ApplicationContext::shutdown();
        }

        City* mCurrentCity;             // The active city (0 if none is active)
        bool mCityPaused;               // whether current city is paused
        bool mLastRun;                  // whether or not this is the final run
        City* mLastCity;                // last city run before reconfiguration
        Ogre::NameValuePairList mLastCityState;     // state of last city
    };
}

#endif
