#ifndef __City_ParticleGS_H__
#define __City_ParticleGS_H__


#include "ProceduralManualObject.h"
#include "OgreRenderToVertexBuffer.h"
#include "RandomTools.h"
#include "CityPlugin.h"
#include "SdkCity.h"

// #define LOG_GENERATED_BUFFER
namespace OgreBites {
using namespace Ogre;

class _OgreCityClassExport City_ParticleGS : public SdkCity
{
 public:
    City_ParticleGS();

 protected:

    void createProceduralParticleSystem();
    void testCapabilities(const RenderSystemCapabilities* caps) override;
    void setupContent(void) override;
    void cleanupContent() override;
#ifdef LOG_GENERATED_BUFFER
    bool frameEnded(const FrameEvent& evt);
#endif
    ProceduralManualObject* mParticleSystem;
};
}
#endif
