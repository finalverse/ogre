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
#ifndef __VolumeCSG_H__
#define __VolumeCSG_H__

#include <stdio.h>
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <iostream>
#include <string>

#include "SdkCity.h"

#include "OgreVolumeChunk.h"
#include "OgreMath.h"


using namespace Ogre;
using namespace OgreBites;
using namespace Ogre::Volume;

/** City for the volume CSG.
*/
class _OgreCityClassExport City_VolumeCSG : public SdkCity
{
protected:
    
    /// Holds the volume root.
    Chunk *mVolumeRoot;

    /// To show or hide everything.
    bool mHideAll;

    /// To rotate the scene.
    Radian mRotation;

    /** Sets up the City.
    */
    void setupContent(void) override;
        
    /** Sets up the UI.
    */
    virtual void setupControls(void);

    /** Is called when the City is stopped.
    */
    void cleanupContent(void) override;
public:

    /** Constructor.
    */
    City_VolumeCSG(void);
    
    /** Overridden from SdkCity.
    */
    bool keyPressed(const KeyboardEvent& evt) override;
    
    /** Overridden from SdkCity.
    */
    bool frameRenderingQueued(const Ogre::FrameEvent& evt) override;
};

#endif
