# FindDirectX.cmake
if (WIN32)
    # First, try to find the old DirectX SDK
    find_path(DIRECTX_INCLUDE_DIR dxdiag.h
        "$ENV{DXSDK_DIR}/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (March 2009)/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (August 2008)/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (June 2008)/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (March 2008)/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (November 2007)/Include"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (August 2007)/Include"
        "C:/DX90SDK/Include"
        "$ENV{PROGRAMFILES}/DX90SDK/Include"
    )

    if (DIRECTX_INCLUDE_DIR)
        include_directories(${DIRECTX_INCLUDE_DIR})
    else()
        # If the old DirectX SDK is not found, use the Windows SDK
        set(DIRECTX_INCLUDE_DIR "$ENV{WindowsSdkDir}/Include")
        include_directories(${DIRECTX_INCLUDE_DIR})
    endif()

    find_path(DIRECTX_LIBRARY_DIR dxguid.lib
        "$ENV{DXSDK_DIR}/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (March 2009)/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (August 2008)/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (June 2008)/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (March 2008)/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (November 2007)/Lib/x86"
        "$ENV{PROGRAMFILES}/Microsoft DirectX SDK (August 2007)/Lib/x86"
        "C:/DX90SDK/Lib"
        "$ENV{PROGRAMFILES}/DX90SDK/Lib"
    )

    if (NOT DIRECTX_LIBRARY_DIR)
        # If the old DirectX SDK is not found, use the Windows SDK
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            # 64-bit build
            set(DIRECTX_LIBRARY_DIR "$ENV{WindowsSdkDir}/Lib/x64")
        else()
            # 32-bit build
            set(DIRECTX_LIBRARY_DIR "$ENV{WindowsSdkDir}/Lib/x86")
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DirectX DEFAULT_MSG DIRECTX_LIBRARY_DIR DIRECTX_INCLUDE_DIR)
