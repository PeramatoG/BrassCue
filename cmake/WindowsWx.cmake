# The classic toolchain attaches application-local deployment when each executable
# is declared. Checking here also catches existing build trees configured without it.
if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[xX]64$")
  message(FATAL_ERROR "The Windows GUI requires an x64 MSVC toolchain. Select the x64 preset or use an x64 Developer PowerShell.")
endif()
if(NOT VCPKG_TOOLCHAIN OR VCPKG_MANIFEST_MODE OR NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows"
    OR NOT VCPKG_APPLOCAL_DEPS)
  message(FATAL_ERROR
    "The Windows GUI requires classic vcpkg x64-windows with VCPKG_APPLOCAL_DEPS=ON. "
    "Install wxwidgets:x64-windows --classic, set VCPKG_ROOT, and configure a fresh build with "
    "--preset windows-x64-debug (see docs/GUI.md for Visual Studio generator commands).")
endif()

set(brasscue_vcpkg_triplet "${VCPKG_INSTALLED_DIR}/x64-windows")
if(NOT IS_DIRECTORY "${brasscue_vcpkg_triplet}")
  message(FATAL_ERROR
    "The classic vcpkg toolchain '${CMAKE_TOOLCHAIN_FILE}' expects packages in "
    "'${brasscue_vcpkg_triplet}'. Select the checkout that already has wxWidgets using "
    "BRASSCUE_VCPKG_ROOT in CMakeUserPresets.json, or set VCPKG_INSTALLED_DIR if its packages "
    "are stored separately, then configure a fresh build. See docs/GUI.md.")
endif()
get_property(brasscue_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(brasscue_multi_config)
  set(brasscue_configs ${CMAKE_CONFIGURATION_TYPES})
else()
  set(brasscue_configs ${CMAKE_BUILD_TYPE})
endif()
if(NOT brasscue_configs)
  message(FATAL_ERROR "Select CMAKE_BUILD_TYPE=Debug or Release for the single-config Windows GUI build.")
endif()

# Check the package's actual Debug/Release imports against the same bin trees
# searched by applocal. A cached wxWidgets_DIR can otherwise link a different copy.
function(brasscue_validate_wx)
  foreach(component IN LISTS ARGN)
    if(NOT TARGET wx::${component})
      message(FATAL_ERROR "wx::${component} is missing from the selected wxWidgets CONFIG package. Reinstall the required wxWidgets features and configure a fresh build tree.")
    endif()
    get_target_property(wx_type wx::${component} TYPE)
    if(NOT wx_type STREQUAL "SHARED_LIBRARY")
      message(FATAL_ERROR "wx::${component} must be a shared import from classic vcpkg x64-windows. Check wxWidgets_DIR and configure a fresh build tree.")
    endif()
    foreach(config IN LISTS brasscue_configs)
      string(TOUPPER "${config}" config_upper)
      if(config_upper STREQUAL "DEBUG")
        set(import_config DEBUG)
        set(package_dir "${brasscue_vcpkg_triplet}/debug")
      elseif(config_upper MATCHES "^(RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
        set(import_config RELEASE)
        set(package_dir "${brasscue_vcpkg_triplet}")
      else()
        message(FATAL_ERROR "Unsupported Windows GUI configuration '${config}'. Use Debug, Release, RelWithDebInfo or MinSizeRel.")
      endif()

      get_target_property(dll wx::${component} IMPORTED_LOCATION_${import_config})
      get_target_property(implib wx::${component} IMPORTED_IMPLIB_${import_config})
      foreach(kind IN ITEMS dll implib)
        if(kind STREQUAL "dll")
          set(expected_dir "${package_dir}/bin")
          set(extension ".dll")
        else()
          set(expected_dir "${package_dir}/lib")
          set(extension ".lib")
        endif()
        get_filename_component(import_dir "${${kind}}" DIRECTORY)
        get_filename_component(import_ext "${${kind}}" LAST_EXT)
        file(REAL_PATH "${import_dir}" import_dir)
        file(REAL_PATH "${expected_dir}" expected_dir)
        string(TOLOWER "${import_dir}" import_dir)
        string(TOLOWER "${expected_dir}" expected_dir)
        if(NOT EXISTS "${${kind}}" OR NOT import_dir STREQUAL expected_dir OR NOT import_ext STREQUAL extension)
          message(FATAL_ERROR
            "wx::${component} has an invalid ${import_config} ${kind}: '${${kind}}'. "
            "Expected a file in '${package_dir}'. Check wxWidgets_DIR and reinstall "
            "wxwidgets:x64-windows --classic (with [webview] when enabled), then configure a fresh build tree.")
        endif()
      endforeach()

      # vcpkg uses the release DLL tree for all configurations other than Debug.
      get_target_property(real_target wx::${component} ALIASED_TARGET)
      if(NOT real_target)
        set(real_target wx::${component})
      endif()
      set_property(TARGET ${real_target} PROPERTY MAP_IMPORTED_CONFIG_${config_upper} "${import_config}")
    endforeach()
  endforeach()
endfunction()

function(brasscue_check_wx_applocal target)
  set(dlls)
  foreach(component IN LISTS brasscue_wx_components)
    list(APPEND dlls "$<TARGET_FILE_NAME:wx::${component}>")
  endforeach()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-Ddirectory=$<TARGET_FILE_DIR:${target}>" "-Ddlls=${dlls}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CheckWxApplocal.cmake"
    VERBATIM)
endfunction()
