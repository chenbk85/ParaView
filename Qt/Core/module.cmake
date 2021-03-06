set (__depends)
if (PARAVIEW_ENABLE_PYTHON)
  set (__depends vtkPVPythonSupport)
endif (PARAVIEW_ENABLE_PYTHON)

vtk_module(pqCore
  GROUPS
    ParaViewQt
  DEPENDS
    pqWidgets
    vtkGUISupportQt
    vtkPVServerManagerApplication
    vtkPVServerManagerDefault
    ${__depends}
  EXCLUDE_FROM_WRAPPING
  TEST_LABELS
    PARAVIEW
)
