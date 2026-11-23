foreach(dll IN LISTS dlls)
  if(NOT EXISTS "${directory}/${dll}")
    message(FATAL_ERROR
      "vcpkg application-local deployment did not place '${dll}' beside the executable in "
      "'${directory}'. Check the preceding applocal output and the classic vcpkg configuration "
      "in docs/GUI.md, then rebuild with --clean-first.")
  endif()
endforeach()
