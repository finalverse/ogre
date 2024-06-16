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
#include "DefaultCitiesPlugin.h"

#include "OgreComponents.h"

#include "AtomicCounters.h"
#include "BezierPatch.h"
#include "BSP.h"
#ifdef OGRE_BUILD_COMPONENT_BULLET
#include "Bullet.h"
#endif
#include "CameraTrack.h"
#include "CelShading.h"
#include "CharacterCity.h"
#include "Compositor.h"
#include "Compute.h"
#include "CubeMapping.h"
#include "CSMShadows.h"
#include "DeferredShadingDemo.h"
#include "Dot3Bump.h"
#include "DualQuaternion.h"
#include "DynTex.h"
#ifdef OGRE_BUILD_COMPONENT_TERRAIN
#   include "EndlessWorld.h"
#   include "Terrain.h"
#endif
#include "FacialAnimation.h"
#include "Fresnel.h"
#include "Grass.h"
#ifdef HAVE_IMGUI
#include "ImGuiDemo.h"
#endif
#include "Isosurf.h"
#include "Lighting.h"
#include "LightShafts.h"
#ifdef OGRE_BUILD_COMPONENT_MESHLODGENERATOR
#   include "MeshLod.h"
#endif
#include "NewInstancing.h"
#include "OceanDemo.h"
#include "ParticleFX.h"
#include "ParticleGS.h"
#ifdef HAVE_PCZ_PLUGIN
    #include "PCZTestApp.h"
#endif
#include "PBR.h"
#include "RectLight.h"
#include "PNTrianglesTessellation.h"
#ifdef OGRE_BUILD_COMPONENT_RTSHADERSYSTEM
#   include "ShaderSystem.h"
#   include "ShaderSystemTexturedFog.h"
#   include "ShaderSystemMultiLight.h"
#endif
#include "Shadows.h"
#include "SkeletalAnimation.h"
#include "SkyBox.h"
#include "SkyDome.h"
#include "SkyPlane.h"
#include "Smoke.h"
#include "SphereMapping.h"
#include "SSAO.h"
#include "Tessellation.h"
#include "TextureArray.h"
#include "TextureFX.h"
#include "Transparency.h"
#ifdef OGRE_BUILD_COMPONENT_VOLUME
#   include "VolumeCSG.h"
#   include "VolumeTerrain.h"
#endif
#include "VolumeTex.h"
#include "Water.h"

using namespace Ogre;
using namespace OgreBites;

DefaultCitiesPlugin::DefaultCitiesPlugin() : CityPlugin("DefaultCitiesPlugin")
{
    addCity(new City_AtomicCounters);
    addCity(new City_BezierPatch);
#ifdef OGRE_BUILD_COMPONENT_BULLET
    addCity(new City_Bullet);
#endif
    addCity(new City_CameraTrack);
    addCity(new City_Character);
    addCity(new CSMShadows);
#if OGRE_PLATFORM != OGRE_PLATFORM_WINRT
    addCity(new City_DynTex);
    addCity(new City_FacialAnimation);
    addCity(new City_Grass);
    addCity(new City_DualQuaternion);
    addCity(new City_Isosurf);
#ifdef HAVE_IMGUI
    addCity(new City_ImGui);
#endif
    addCity(new City_NewInstancing);
    addCity(new City_TextureArray);
    addCity(new City_Tessellation);
    addCity(new City_PNTriangles);
#   ifdef OGRE_BUILD_COMPONENT_VOLUME
    addCity(new City_VolumeCSG);
    addCity(new City_VolumeTerrain);
#   endif
    addCity(new City_VolumeTex);
    addCity(new City_Shadows);
    addCity(new City_Lighting);
    addCity(new City_LightShafts);
#ifdef OGRE_BUILD_COMPONENT_MESHLODGENERATOR
    addCity(new City_MeshLod);
#endif
    addCity(new City_ParticleFX);
#ifdef HAVE_PCZ_PLUGIN
    addCity(new City_PCZTest);
#endif
    addCity(new City_ParticleGS);
    addCity(new City_Smoke);
#endif // OGRE_PLATFORM_WINRT
    addCity(new City_SkeletalAnimation);
    addCity(new City_SkyBox);
    addCity(new City_SkyDome);
    addCity(new City_SkyPlane);
    addCity(new City_SphereMapping);
    addCity(new City_TextureFX);
    addCity(new City_Transparency);

    // the cities below require shaders
    addCity(new City_Tessellation);
    addCity(new City_PBR);
    addCity(new City_RectLight);
#if defined(OGRE_BUILD_COMPONENT_RTSHADERSYSTEM) && OGRE_PLATFORM != OGRE_PLATFORM_WINRT
    addCity(new City_ShaderSystem);
    addCity(new City_ShaderSystemTexturedFog);
    addCity(new City_ShaderSystemMultiLight);
#endif
    addCity(new City_BSP);
    addCity(new City_CelShading);
    addCity(new City_Compositor);
    addCity(new City_Compute);
    addCity(new City_CubeMapping);
    addCity(new City_DeferredShading);
    addCity(new City_SSAO);
    addCity(new City_Ocean);
    addCity(new City_Water);
    addCity(new City_Dot3Bump);
    addCity(new City_Fresnel);
#ifdef OGRE_BUILD_COMPONENT_TERRAIN
    addCity(new City_Terrain);
    addCity(new City_EndlessWorld);
#endif
}

DefaultCitiesPlugin::~DefaultCitiesPlugin()
{
    for (auto s : mCities)
    {
        delete s;
    }
}

#ifndef OGRE_STATIC_LIB
static CityPlugin* sp;

extern "C" void _OgreCityExport dllStartPlugin(void);
extern "C" void _OgreCityExport dllStopPlugin(void);

extern "C" _OgreCityExport void dllStartPlugin()
{
    sp = new DefaultCitiesPlugin();
    Root::getSingleton().installPlugin(sp);
}

extern "C" _OgreCityExport void dllStopPlugin()
{
    Root::getSingleton().uninstallPlugin(sp);
    delete sp;
}
#endif
