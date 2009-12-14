# configure C/C++ runtime type (static, shared, none, cppstatic)
IF(NOT DEFINED CFG_RUNTIME_TYPE)
  SET(CFG_RUNTIME_TYPE "static")
ENDIF(NOT DEFINED CFG_RUNTIME_TYPE)
# configure multithreaded application (0, 1)
IF(NOT DEFINED CFG_THREADS)
  SET(CFG_THREADS 0)
ENDIF(NOT DEFINED CFG_THREADS)
# configure optimization type (speed, size)
IF(NOT DEFINED CFG_OPT_TYPE)
  SET(CFG_OPT_TYPE "speed")
ENDIF(NOT DEFINED CFG_OPT_TYPE)
# configure application entry point name (default = -1, none = 0, any function name)
IF(NOT DEFINED CFG_ENTRY_NAME)
  SET(CFG_ENTRY_NAME -1)
ENDIF(NOT DEFINED CFG_ENTRY_NAME)

# C compiler flags
SET(CMAKE_C_FLAGS "-nologo -Zi -W3 -Gy -DWIN32_LEAN_AND_MEAN -D_WINDOWS -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1")
IF(${CFG_THREADS})
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTHREADS")
ENDIF(${CFG_THREADS})
IF(CMAKE_CL_64)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN64")
ELSE(CMAKE_CL_64)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32")
ENDIF(CMAKE_CL_64)

SET(CMAKE_C_FLAGS_DEBUG "-DDEBUG -D_DEBUG -Od")
SET(CMAKE_C_FLAGS_RELEASE "-DNDEBUG")

IF(${CFG_RUNTIME_TYPE} STREQUAL "none")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -GS-")
ELSE(${CFG_RUNTIME_TYPE} STREQUAL "none")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -GS")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -RTC1")
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -GL")
ENDIF(${CFG_RUNTIME_TYPE} STREQUAL "none")

IF(${CFG_RUNTIME_TYPE} STREQUAL "static")
  IF(${CFG_THREADS} OR DEFINED MSVC80)
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MTd")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
  ELSE(${CFG_THREADS} OR DEFINED MSVC80)
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MLd")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ML")
  ENDIF(${CFG_THREADS} OR DEFINED MSVC80)
ELSEIF(${CFG_RUNTIME_TYPE} STREQUAL "shared")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MDd")
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MD")
ELSEIF(${CFG_RUNTIME_TYPE} STREQUAL "cppstatic")
  SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MDd -D_STATIC_CPPLIB")
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MD -D_STATIC_CPPLIB")
ENDIF(${CFG_RUNTIME_TYPE} STREQUAL "static")

IF(${CFG_OPT_TYPE} STREQUAL "speed")
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
ENDIF(${CFG_OPT_TYPE} STREQUAL "speed")
IF(${CFG_OPT_TYPE} STREQUAL "size")
  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O1")
ENDIF(${CFG_OPT_TYPE} STREQUAL "size")

# C++ compiler flags
SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -EHsc -GR")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

# Resource compiler flags
IF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  SET(CMAKE_RC_FLAGS -DDEBUG)
ENDIF(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

# Linker flags
SET(CMAKE_EXE_LINKER_FLAGS "-nologo -debug -incremental:no -stack:10000000 -map /manifest:no -dynamicbase -nxcompat -largeaddressaware")

IF(CMAKE_CL_64)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -machine:x64")
ELSE(CMAKE_CL_64)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -machine:x86")
  IF(NOT ${CFG_RUNTIME_TYPE} STREQUAL "none")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -safeseh")
  ENDIF(NOT ${CFG_RUNTIME_TYPE} STREQUAL "none")
ENDIF(CMAKE_CL_64)

IF(${CFG_RUNTIME_TYPE} STREQUAL "none")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlib chkstk.obj")
ENDIF(${CFG_RUNTIME_TYPE} STREQUAL "none")

IF(${CFG_ENTRY_NAME} EQUAL 0)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -noentry")
ELSEIF(${CFG_ENTRY_NAME} EQUAL -1)
  IF(${CFG_RUNTIME_TYPE} STREQUAL "none")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -entry:main")
  ENDIF(${CFG_RUNTIME_TYPE} STREQUAL "none")
ELSE(${CFG_ENTRY_NAME} EQUAL 0)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -entry:${CFG_ENTRY_NAME}")
ENDIF(${CFG_ENTRY_NAME} EQUAL 0)

SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fixed:no")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "-opt:ref -opt:icf")

IF(NOT ${CFG_RUNTIME_TYPE} STREQUAL "none")
  SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -LTCG")
ENDIF(NOT ${CFG_RUNTIME_TYPE} STREQUAL "none")

SET(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE ${CMAKE_EXE_LINKER_FLAGS_RELEASE})

MACRO(add_precomp_header precomp_header precomp_source sources)
  GET_FILENAME_COMPONENT(precomp_name ${precomp_header} NAME_WE)
  SET(pch_name "${CMAKE_CURRENT_BINARY_DIR}/${precomp_name}.pch")

  SET_SOURCE_FILES_PROPERTIES(${precomp_source} PROPERTIES COMPILE_FLAGS "-Yc\"${precomp_header}\" -Fp\"${pch_name}\"" OBJECT_OUTPUTS "${pch_name}")
  SET_SOURCE_FILES_PROPERTIES(${${sources}} PROPERTIES COMPILE_FLAGS "-Yu\"${precomp_header}\" -FI\"${precomp_header}\" -Fp\"${pch_name}\"" OBJECT_DEPENDS "${pch_name}")

  LIST(APPEND ${sources} ${precomp_source})
ENDMACRO(add_precomp_header)
