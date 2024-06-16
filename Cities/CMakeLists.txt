# Configure Cities build

# Add android JNI binding city
if(OGRE_BUILD_ANDROID_JNI_CITY)
  add_subdirectory(AndroidJNI)
endif()

if (OGRE_BUILD_CITIES)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Common/include)

  if (OGRE_BUILD_COMPONENT_RTSHADERSYSTEM)
    add_definitions(-DINCLUDE_RTSHADER_SYSTEM)
  	set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreRTShaderSystem)
  endif ()

  if (OGRE_BUILD_COMPONENT_BULLET)
    set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreBullet)
  endif ()

  if (OGRE_BUILD_COMPONENT_OVERLAY)
    set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreOverlay)
  endif ()
  
  if (OGRE_BUILD_COMPONENT_TERRAIN)
  	set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreTerrain)
  endif ()

  if (OGRE_BUILD_COMPONENT_VOLUME)
  	set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreVolume)
  endif ()
  
  if (OGRE_BUILD_COMPONENT_MESHLODGENERATOR AND OGRE_CONFIG_ENABLE_MESHLOD)
  	set(CITY_DEPENDENCIES ${CITY_DEPENDENCIES} OgreMeshLodGenerator)
  endif ()
  
  if(EMSCRIPTEN)
    add_subdirectory(Emscripten)
    return()
  endif()

  ## Default Cities Plugin
  file (GLOB CITIES_HEADERS ${PROJECT_SOURCE_DIR}/Cities/*/include)
  include_directories(${CITIES_HEADERS})

  file(GLOB CITIES_SOURCES ${PROJECT_SOURCE_DIR}/Cities/*/src/*.cpp)

  set(BLACKLIST Browser TerrainTessellation)
  if(NOT OGRE_BUILD_PLUGIN_PCZ)
    list(APPEND BLACKLIST PCZTestApp)
  endif()
  if(NOT OGRE_BUILD_COMPONENT_TERRAIN)
    list(APPEND BLACKLIST EndlessWorld)
  endif()

  foreach(FILTER_CITY ${BLACKLIST})
    file(GLOB BLACKLIST_CPP ${PROJECT_SOURCE_DIR}/Cities/${FILTER_CITY}/src/*.cpp)
    list(REMOVE_ITEM CITIES_SOURCES ${BLACKLIST_CPP})
  endforeach()

  add_definitions(${OGRE_VISIBILITY_FLAGS})
  add_library(DefaultCities ${OGRE_LIB_TYPE}
              ${CMAKE_CURRENT_SOURCE_DIR}/Common/src/DefaultCitiesPlugin.cpp
              ${CITIES_SOURCES} ${CITIES_HEADERS})
  target_link_libraries(DefaultCities OgreMain ${CITY_DEPENDENCIES} OgreBites)

  if(OGRE_BUILD_PLUGIN_PCZ)
    target_link_libraries(DefaultCities Plugin_PCZSceneManager)
    target_compile_definitions(DefaultCities PRIVATE -DHAVE_PCZ_PLUGIN)
  endif()

  if(OGRE_BUILD_COMPONENT_OVERLAY_IMGUI)
    target_compile_definitions(DefaultCities PRIVATE -DHAVE_IMGUI)
  endif()

  ogre_config_city_lib(DefaultCities)

  if (APPLE AND NOT APPLE_IOS)
      # Set the INSTALL_PATH so that Cities can be installed in the application package
      set_target_properties(DefaultCities
        PROPERTIES BUILD_WITH_INSTALL_RPATH 1
        INSTALL_NAME_DIR "@executable_path/../Plugins"
      )
  endif()

  ## Add browser last
  add_subdirectory(Browser)
endif ()
		
if (MSVC AND OGRE_BUILD_CITIES)
  find_package(Wix)
  if (Wix_FOUND)
    # Create WiX setup for demo build
    configure_file(${OGRE_TEMPLATES_DIR}/demos.wxs.in ${CMAKE_CURRENT_BINARY_DIR}/demos.wxs @ONLY)
    configure_file(${OGRE_TEMPLATES_DIR}/demomedia.wxi.in ${CMAKE_CURRENT_BINARY_DIR}/demomedia.wxi @ONLY)
    configure_file(${OGRE_TEMPLATES_DIR}/DemoLicense.rtf ${CMAKE_CURRENT_BINARY_DIR}/DemoLicense.rtf COPYONLY)
	# Configure files, set media dir temporarily
	set(OGRE_MEDIA_DIR_TMP ${OGRE_MEDIA_DIR_REL})
	set(OGRE_MEDIA_DIR_REL "Media")
    configure_file(${OGRE_TEMPLATES_DIR}/resources.cfg.in ${CMAKE_CURRENT_BINARY_DIR}/resources.cfg @ONLY)
	# restore
	set(OGRE_MEDIA_DIR_REL ${OGRE_MEDIA_DIR_TMP})
    add_custom_target(demo_installer 
    COMMAND ${Wix_BINARY_DIR}/candle demos.wxs 
	  COMMAND ${Wix_BINARY_DIR}/light -ext WixUIExtension -cultures:en-us -out OgreDemos_v${OGRE_VERSION_DASH_SEPARATED}.msi demos.wixobj
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Building demo installer" VERBATIM
    )
	# Make sure we build samples first
	add_dependencies(demo_installer CityBrowser)
  endif()
endif()
