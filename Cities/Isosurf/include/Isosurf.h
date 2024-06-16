#ifndef __City_Isosurf_H__
#define __City_Isosurf_H__

#include "SdkCity.h"
#include "CityPlugin.h"
#include "ProceduralTools.h"

namespace OgreBites {
using namespace Ogre;

class _OgreCityClassExport City_Isosurf : public SdkCity
{
    Entity* tetrahedra;
    MeshPtr mTetrahedraMesh;

 public:
    City_Isosurf();
    void testCapabilities(const RenderSystemCapabilities* caps) override;
    void setupContent(void) override;
    void cleanupContent() override;
    bool frameRenderingQueued(const FrameEvent& evt) override;
};
}

#endif
