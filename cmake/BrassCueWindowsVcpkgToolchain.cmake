# Visual Studio Open Folder can replace VCPKG_ROOT with its bundled checkout.
function(brasscue_external_vcpkg_root candidate output)
  string(STRIP "${candidate}" candidate)
  string(REPLACE "\\" "/" candidate "${candidate}")
  cmake_path(NORMAL_PATH candidate)
  string(TOLOWER "${candidate}" lowercase_candidate)
  if(NOT IS_ABSOLUTE "${candidate}"
      OR lowercase_candidate MATCHES "/microsoft visual studio/[^/]+/[^/]+/vc/vcpkg/?$"
      OR NOT EXISTS "${candidate}/.vcpkg-root"
      OR NOT EXISTS "${candidate}/vcpkg.exe"
      OR NOT EXISTS "${candidate}/scripts/buildsystems/vcpkg.cmake")
    set(${output} "" PARENT_SCOPE)
  else()
    set(${output} "${candidate}" PARENT_SCOPE)
  endif()
endfunction()

if(BRASSCUE_VCPKG_ROOT)
  brasscue_external_vcpkg_root("${BRASSCUE_VCPKG_ROOT}" brasscue_vcpkg_root)
  if(NOT brasscue_vcpkg_root)
    message(FATAL_ERROR "BRASSCUE_VCPKG_ROOT='${BRASSCUE_VCPKG_ROOT}' must point to an external vcpkg checkout with vcpkg.exe and scripts/buildsystems/vcpkg.cmake.")
  endif()
else()
  brasscue_external_vcpkg_root("$ENV{VCPKG_ROOT}" brasscue_vcpkg_root)
  foreach(profile_dir IN ITEMS "$ENV{LOCALAPPDATA}" "$ENV{APPDATA}")
    if(brasscue_vcpkg_root)
      break()
    endif()
    if(profile_dir AND EXISTS "${profile_dir}/vcpkg/vcpkg.path.txt")
      file(READ "${profile_dir}/vcpkg/vcpkg.path.txt" registered_root)
      brasscue_external_vcpkg_root("${registered_root}" brasscue_vcpkg_root)
    endif()
  endforeach()
  if(NOT brasscue_vcpkg_root)
    message(FATAL_ERROR
      "No external classic vcpkg checkout found in VCPKG_ROOT or the user integration descriptor. "
      "Set BRASSCUE_VCPKG_ROOT in CMakeUserPresets.json to the checkout that already has wxWidgets, "
      "then delete the BrassCue CMake cache and reconfigure. See docs/GUI.md.")
  endif()
endif()

if(NOT BRASSCUE_RESOLVED_VCPKG_ROOT STREQUAL brasscue_vcpkg_root)
  message(STATUS "BrassCue external classic vcpkg root: ${brasscue_vcpkg_root}")
endif()
set(BRASSCUE_RESOLVED_VCPKG_ROOT "${brasscue_vcpkg_root}" CACHE INTERNAL "Selected external classic vcpkg root")
set(ENV{VCPKG_ROOT} "${brasscue_vcpkg_root}")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES BRASSCUE_VCPKG_ROOT)
include("${brasscue_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
