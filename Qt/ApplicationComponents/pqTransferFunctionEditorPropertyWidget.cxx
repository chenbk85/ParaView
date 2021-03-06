/*=========================================================================

   Program: ParaView
   Module: pqTransferFunctionEditorPropertyWidget.cxx

   Copyright (c) 2005-2012 Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "pqTransferFunctionEditorPropertyWidget.h"

#include "vtkSMProxy.h"
#include "vtkSMProperty.h"
#include "pqTransferFunctionChartViewWidget.h"
#include "vtkPiecewiseFunction.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMProxyListDomain.h"
#include "pqPVApplicationCore.h"
#include "vtkChartXY.h"
#include "vtkAxis.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

pqTransferFunctionEditorPropertyWidget::pqTransferFunctionEditorPropertyWidget(vtkSMProxy *smProxy,
                                                                               vtkSMProperty *proxyProperty,
                                                                               QWidget *pWidget)
  : pqPropertyWidget(smProxy, pWidget),
    Property(proxyProperty)
{
  QVBoxLayout *l = new QVBoxLayout;
  l->setMargin(0);

  QPushButton *button = new QPushButton("Edit");
  connect(button, SIGNAL(clicked()), this, SLOT(buttonClicked()));
  l->addWidget(button);

  this->setLayout(l);

  this->setReason() << "pqTransferFunctionEditorPropertyWidget for a property with "
                    << "the panel_widget=\"transfer_function_editor\" attribute";
}

pqTransferFunctionEditorPropertyWidget::~pqTransferFunctionEditorPropertyWidget()
{
}

void pqTransferFunctionEditorPropertyWidget::buttonClicked()
{
  vtkSMProxyProperty *proxyProperty = vtkSMProxyProperty::SafeDownCast(this->Property);
  if(!proxyProperty)
    {
    qDebug() << "error, property is not a proxy property";
    return;
    }
  if(proxyProperty->GetNumberOfProxies() < 1)
    {
    qDebug() << "error, no proxies for property";
    return;
    }

  vtkSMProxy *pxy = proxyProperty->GetProxy(0);
  if(!pxy)
    {
    qDebug() << "error, no proxy property";
    return;
    }
  vtkObjectBase *object = pxy->GetClientSideObject();

  vtkPiecewiseFunction *transferFunction =
    vtkPiecewiseFunction::SafeDownCast(object);

  pqTransferFunctionEditorPropertyWidgetDialog dialog(transferFunction, this);
  dialog.resize(600, 400);
  dialog.exec();

  emit this->modified();
}

// === pqTransferFunctionEditorPropertyWidgetDialog ======================== //
pqTransferFunctionEditorPropertyWidgetDialog::
  pqTransferFunctionEditorPropertyWidgetDialog(vtkPiecewiseFunction *transferFunction, QWidget *parent_)
  : QDialog(parent_)
{
  QVBoxLayout *l = new QVBoxLayout;

  this->TransferFunction = transferFunction;

  pqTransferFunctionChartViewWidget *widget =
    new pqTransferFunctionChartViewWidget(this);
  widget->setTitle("Edit Transfer Function");
  widget->chart()->GetAxis(vtkAxis::BOTTOM)->SetTitle("Input");
  widget->chart()->GetAxis(vtkAxis::LEFT)->SetTitle("Output");
  double range[2];
  this->TransferFunction->GetRange(range);
  double validBounds[4] = { VTK_DOUBLE_MIN, VTK_DOUBLE_MAX, range[0], range[1] };
  widget->setValidBounds(validBounds);
  widget->addPiecewiseFunction(this->TransferFunction.GetPointer());

  l->addWidget(widget);

  QHBoxLayout *buttonLayout = new QHBoxLayout;
  QPushButton *closeButton = new QPushButton("Close", this);
  closeButton->setObjectName("CloseButton");
  this->connect(closeButton, SIGNAL(clicked()), this, SLOT(accept()));
  buttonLayout->addWidget(closeButton);
  l->addLayout(buttonLayout);
  this->setLayout(l);
}
