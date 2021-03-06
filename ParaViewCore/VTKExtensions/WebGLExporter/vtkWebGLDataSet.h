/*=========================================================================

  Program:   ParaView Web
  Module:    vtkWebGLDataSet.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkWebGLDataSet
// .SECTION Description
// vtkWebGLDataSet represent vertices, lines, polygons, and triangles.

#ifndef __vtkWebGLDataSet_h
#define __vtkWebGLDataSet_h

#include "vtkObject.h"
#include "vtkWebGLObject.h"
#include "vtkPVVTKExtensionsWebGLExporterModule.h" // needed for export macro

#include <string>

class VTKPVVTKEXTENSIONSWEBGLEXPORTER_EXPORT vtkWebGLDataSet : public vtkObject
{
public:
  static vtkWebGLDataSet* New();
  vtkTypeMacro(vtkWebGLDataSet, vtkObject)
  void PrintSelf(ostream &os, vtkIndent indent);

  void SetVertices(float* v, int size);
  void SetIndexes(short* i, int size);
  void SetNormals(float* n);
  void SetColors(unsigned char* c);
  void SetPoints(float* p, int size);
  void SetTCoords(float *t);
  void SetMatrix(float* m);
  void SetType(WebGLObjectTypes t);

  unsigned char* GetBinaryData();
  int GetBinarySize();
  void GenerateBinaryData();
  bool HasChanged();

  //BTX
  std::string GetMD5();
  //ETX

protected:
  vtkWebGLDataSet();
  ~vtkWebGLDataSet();

  int NumberOfVertices;
  int NumberOfPoints;
  int NumberOfIndexes;
  WebGLObjectTypes webGLType;

  float* Matrix;
  float* vertices;
  float* normals;
  short* indexes;
  float* points;
  float* tcoords;
  unsigned char* colors;
  unsigned char* binary;   // Data in binary
  int binarySize;          // Size of the data in binary
  bool hasChanged;
  std::string MD5;

private:
  vtkWebGLDataSet(const vtkWebGLDataSet&); // Not implemented
  void operator=(const vtkWebGLDataSet&);   // Not implemented
};

#endif
