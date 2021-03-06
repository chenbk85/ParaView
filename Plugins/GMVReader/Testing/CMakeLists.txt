INCLUDE(ParaViewTestingMacros)

IF(VISIT_BUILD_READER_GMV)
  SET(GMV_READER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/GMVReaderBasicRead_WithVisitBridge.xml
  )
  SET(GMVReaderBasicRead_WithVisitBridge_BREAK TRUE)
  SET(GMVReaderBasicRead_WithVisitBridge_USE_NEW_PANELS TRUE)

  SET(GMV_READER_SERVER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/serverGMVReaderBasicRead_WithVisitBridge.xml
  )
  SET(serverGMVReaderBasicRead_WithVisitBridge_BREAK TRUE)
  SET(serverGMVReaderBasicRead_WithVisitBridge_USE_NEW_PANELS TRUE)

  SET(GMV_READER_STATIC_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/GMVReaderBasicRead_WithVisitBridge-static.xml
  )
  SET(GMVReaderBasicRead_WithVisitBridge-static_BREAK TRUE)
  SET(GMVReaderBasicRead_WithVisitBridge-static_USE_NEW_PANELS TRUE)

  SET(GMV_READER_STATIC_SERVER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/serverGMVReaderBasicRead_WithVisitBridge-static.xml
  )
  SET(serverGMVReaderBasicRead_WithVisitBridge-static_BREAK TRUE)
  SET(serverGMVReaderBasicRead_WithVisitBridge-static_USE_NEW_PANELS TRUE)

ELSE(VISIT_BUILD_READER_GMV)
  SET(GMV_READER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/GMVReaderBasicRead.xml
  )
  SET(GMVReaderBasicRead_BREAK TRUE)
  SET(GMVReaderBasicRead_USE_NEW_PANELS TRUE)

  SET(GMV_READER_SERVER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/serverGMVReaderBasicRead.xml
  )
  SET(serverGMVReaderBasicRead_BREAK TRUE)
  SET(serverGMVReaderBasicRead_USE_NEW_PANELS TRUE)

  SET(GMV_READER_STATIC_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/GMVReaderBasicRead-static.xml
  )
  SET(GMVReaderBasicRead-static_BREAK TRUE)
  SET(GMVReaderBasicRead-static_USE_NEW_PANELS TRUE)

  SET(GMV_READER_STATIC_SERVER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/serverGMVReaderBasicRead-static.xml
  )
  SET(serverGMVReaderBasicRead-static_BREAK TRUE)
  SET(serverGMVReaderBasicRead-static_USE_NEW_PANELS TRUE)
ENDIF(VISIT_BUILD_READER_GMV)

IF (PARAVIEW_DATA_ROOT AND PARAVIEW_BUILD_QT_GUI)
  IF (BUILD_SHARED_LIBS)
    ADD_CLIENT_TESTS("pv"
      BASELINE_DIR ${PARAVIEW_DATA_ROOT}/Baseline
      TEST_SCRIPTS ${GMV_READER_TESTS}
    )

    ADD_CLIENT_SERVER_TESTS("pvcs"
      BASELINE_DIR ${PARAVIEW_DATA_ROOT}/Baseline
      TEST_SCRIPTS ${GMV_READER_SERVER_TESTS}
    )

  ELSE(BUILD_SHARED_LIBS)
    # Static build no need to load plugins
    ADD_CLIENT_TESTS("pv"
      BASELINE_DIR ${PARAVIEW_DATA_ROOT}/Baseline
      TEST_SCRIPTS ${GMV_READER_STATIC_TESTS}
    )

    ADD_CLIENT_SERVER_TESTS("pvcs"
      BASELINE_DIR ${PARAVIEW_DATA_ROOT}/Baseline
      TEST_SCRIPTS ${GMV_READER_STATIC_SERVER_TESTS}
    )

  ENDIF(BUILD_SHARED_LIBS)
ENDIF (PARAVIEW_DATA_ROOT AND PARAVIEW_BUILD_QT_GUI)
