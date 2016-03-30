set(SHAPO_NAME shapo)

if (NOT ENABLE_SHAPO)
  return()
endif()

tpl_dowload_path( URL ${SHAPO_URL} ${TPL_DOWNLOAD_PATH} )

ExternalProject_Add( ${SHAPO_NAME}
 URL ${URL}/${SHAPO_TGZ}
 URL_MD5 ${SHAPO_MD5}
 UPDATE_COMMAND ""
 CMAKE_ARGS
   -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
   -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
   -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
   -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
   -DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
   -DSHAPO_INSTALL_DIR:PATH=${CMAKE_INSTALL_PREFIX}
   -DBUILD_FORTRAN_SUPPORT:BOOL=OFF
   -DBUILD_SHARED_LIBS:BOOL=OFF
   -DBUILD_SHAPO_PARALLEL:BOOL=OFF
 INSTALL_COMMAND ""
)
