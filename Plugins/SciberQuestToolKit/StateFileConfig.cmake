#    ____    _ __           ____               __    ____
#   / __/___(_) /  ___ ____/ __ \__ _____ ___ / /_  /  _/__  ____
#  _\ \/ __/ / _ \/ -_) __/ /_/ / // / -_|_-</ __/ _/ // _ \/ __/
# /___/\__/_/_.__/\__/_/  \___\_\_,_/\__/___/\__/ /___/_//_/\__(_)
#
# Copyright 2012 SciberQuest Inc.
#
# +---------------------------------------------------------------------------+
# |                                                                           |
# |                       SciVis Tool Kit Tests                               |
# |                                                                           |
# +---------------------------------------------------------------------------+

set(SQTK_DATA_DIR
  "${PROJECT_SOURCE_DIR}/../data/"
  CACHE FILEPATH
  "Path to SciberQuestToolKit test data.")

if (EXISTS ${SQTK_DATA_DIR})

  message(STATUS "Configuring state files.")

  set(SQTK_TESTS
    test-periodic-bc.pvsm
    test-sq-field-tracer.pvsm
    test-sq-poincare-mapper.pvsm
    test-sq-topology-mapper.pvsm
    )

  foreach (F ${SQTK_TESTS})
    configure_file(
      "${PROJECT_SOURCE_DIR}/states/${F}.in"
      "${PROJECT_BINARY_DIR}/states/${F}"
      @ONLY)
    message(STATUS "    Configured ${F}.")
  endforeach (F)

endif (EXISTS ${SQTK_DATA_DIR})
