if(SWIFT_BUILD_DYNAMIC_STDLIB AND SWIFT_INCLUDE_TESTS)
  add_subdirectory(swift-reflection-test)
endif()
if(SWIFT_BUILD_SDK_OVERLAY)
  include(CheckIncludeFiles)
  check_include_files("xpc/xpc.h" HAVE_XPC_H)

  if(SWIFT_BUILD_SOURCEKIT AND CMAKE_SYSTEM_NAME STREQUAL Darwin AND HAVE_XPC_H)
    add_subdirectory(swift-lang)
  endif()
endif()
