project(hode)

cmake_minimum_required(VERSION 3.0)

if(VITA)
  find_program(PKG_CONFIG_EXECUTABLE NAMES arm-vita-eabi-pkg-config HINTS "${VITASDK}/bin")
  if (NOT PKG_CONFIG_EXECUTABLE)
    message(WARNING "Could not find arm-vita-eabi-pkg-config")
  endif()
endif(VITA)

include(FindPkgConfig)

pkg_check_modules(SDL2 sdl2 REQUIRED)

include_directories(
  ${SDL2_INCLUDE_DIRS}
)

file(GLOB SRC *.cpp)
list(FILTER SRC EXCLUDE REGEX ".*android.cpp|system_psp.cpp|system_wii.cpp")
add_executable(${CMAKE_PROJECT_NAME}
  ${SRC}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
  ${SDL2_LIBRARIES}
)

if(NINTENDO_SWITCH)
  add_definitions(-D__SWITCH__)
  add_custom_target(${CMAKE_PROJECT_NAME}.nro
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMAND nacptool --create "Heart of Darkness" "cyx, usineur" "0.2.9f" ${CMAKE_PROJECT_NAME}.nacp
    COMMAND elf2nro ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.nro --icon=${CMAKE_SOURCE_DIR}/res/icon.jpg --nacp=${CMAKE_PROJECT_NAME}.nacp
  )
  add_custom_target(nxlink
    COMMAND nxlink -a $(SWITCHIP) ${CMAKE_PROJECT_NAME}.nro -s -p ${CMAKE_PROJECT_NAME}/${CMAKE_PROJECT_NAME}.nro
    DEPENDS ${CMAKE_PROJECT_NAME}.nro
  )
endif(NINTENDO_SWITCH)

if(VITA)
  include("${VITASDK}/share/vita.cmake" REQUIRED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fpermissive")
  vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE)
  vita_create_vpk(${CMAKE_PROJECT_NAME}.vpk "HODEVITA1" eboot.bin
    NAME ${CMAKE_PROJECT_NAME}
    FILE res/icon0.png sce_sys/icon0.png
    FILE res/bg.png sce_sys/livearea/contents/bg.png
    FILE res/startup.png sce_sys/livearea/contents/startup.png
    FILE res/template.xml sce_sys/livearea/contents/template.xml
)
endif(VITA)
