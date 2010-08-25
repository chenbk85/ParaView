/*=========================================================================

  Program:   ParaView
  Module:    vtkSMProxy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMProxy.h"

#include "vtkCommand.h"
#include "vtkDebugLeaks.h"
#include "vtkGarbageCollector.h"
#include "vtkInstantiator.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModuleConnectionManager.h"
#include "vtkProcessModule2.h"
#include "vtkPVOptions.h"
#include "vtkPVXMLElement.h"
#include "vtkSMDocumentation.h"
#include "vtkSMInputProperty.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxyLocator.h"
#include "vtkSMProxyManager.h"

#include "vtkSMProxyInternals.h"

#include <vtkstd/algorithm>
#include <vtkstd/set>
#include <vtkstd/string>
#include <vtksys/ios/sstream>

//---------------------------------------------------------------------------
// Observer for modified event of the property
class vtkSMProxyObserver : public vtkCommand
{
public:
  static vtkSMProxyObserver *New() 
    { return new vtkSMProxyObserver; }

  virtual void Execute(vtkObject* vtkNotUsed(obj),
    unsigned long vtkNotUsed(event), void* vtkNotUsed(data))
    {
    if (this->Proxy)
      {
      if (this->PropertyName.c_str())
        {
        // This is observing a property.
        this->Proxy->SetPropertyModifiedFlag(this->PropertyName.c_str(), 1);
        }
      }
    }

  void SetPropertyName(const char* name)
    {
    this->PropertyName = (name? name : "");
    }

  // Note that Proxy is not reference counted. Since the Proxy has a reference 
  // to the Property and the Property has a reference to the Observer, making
  // Proxy reference counted would cause a loop.
  void SetProxy(vtkSMProxy* proxy)
    {
      this->Proxy = proxy;
    }

protected:
  vtkstd::string PropertyName;
  vtkSMProxy* Proxy;
};

vtkStandardNewMacro(vtkSMProxy);

vtkCxxSetObjectMacro(vtkSMProxy, XMLElement, vtkPVXMLElement);
vtkCxxSetObjectMacro(vtkSMProxy, Hints, vtkPVXMLElement);
vtkCxxSetObjectMacro(vtkSMProxy, Deprecated, vtkPVXMLElement);

//---------------------------------------------------------------------------
vtkSMProxy::vtkSMProxy()
{
  this->Internals = new vtkSMProxyInternals;
  // By default, all objects are created on data server.
  this->Location = vtkProcessModule2::DATA_SERVER;
  this->VTKClassName = 0;
  this->XMLGroup = 0;
  this->XMLName = 0;
  this->XMLLabel = 0;
  this->ObjectsCreated = 0;

  this->XMLElement = 0;
  this->DoNotUpdateImmediately = 0;
  this->DoNotModifyProperty = 0;
  this->InUpdateVTKObjects = 0;
  this->PropertiesModified = 0;

  this->Documentation = vtkSMDocumentation::New();
  this->InMarkModified = 0;

  this->NeedsUpdate = true;

  this->Hints = 0;
  this->Deprecated = 0;
}

//---------------------------------------------------------------------------
vtkSMProxy::~vtkSMProxy()
{
  this->RemoveAllObservers();

  // ensure that the properties are destroyed before we delete this->Internals.
  this->Internals->Properties.clear();

  delete this->Internals;
  this->SetVTKClassName(0);
  this->SetXMLGroup(0);
  this->SetXMLName(0);
  this->SetXMLLabel(0);
  this->SetXMLElement(0);
  this->Documentation->Delete();
  this->SetHints(0);
  this->SetDeprecated(0);
}


//---------------------------------------------------------------------------
vtkObjectBase* vtkSMProxy::GetClientSideObject()
{
  // FIXME:
  abort();
  return NULL;
//  return vtkProcessModule::GetProcessModule()->GetObjectFromID(
//    this->GetID(), true); // the second argument means that no error
                          // will be printed if the ID is invalid.
}

//---------------------------------------------------------------------------
const char* vtkSMProxy::GetPropertyName(vtkSMProperty* prop)
{
  const char* result = 0;
  vtkSMPropertyIterator* piter = this->NewPropertyIterator();
  for(piter->Begin(); !piter->IsAtEnd(); piter->Next())
    {
    if (prop == piter->GetProperty())
      {
      result = piter->GetKey();
      break;
      }
    }
  piter->Delete();
  return result;
}

//---------------------------------------------------------------------------
vtkSMProperty* vtkSMProxy::GetProperty(const char* name)
{
  if (!name)
    {
    return 0;
    }
  vtkSMProxyInternals::PropertyInfoMap::iterator it =
    this->Internals->Properties.find(name);
  if (it != this->Internals->Properties.end())
    {
    return it->second.Property.GetPointer();
    }

  return 0;
}

//---------------------------------------------------------------------------
void vtkSMProxy::RemoveAllObservers()
{
  vtkSMProxyInternals::PropertyInfoMap::iterator it;
  for (it  = this->Internals->Properties.begin();
       it != this->Internals->Properties.end();
       ++it)
    {
    vtkSMProperty* prop = it->second.Property.GetPointer();
    if (it->second.ObserverTag > 0)
      {
      prop->RemoveObserver(it->second.ObserverTag);
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMProxy::RemoveProperty(const char* name)
{
  if (!name)
    {
    return;
    }

  vtkSMProxyInternals::PropertyInfoMap::iterator it =
    this->Internals->Properties.find(name);
  if (it != this->Internals->Properties.end())
    {
    it->second.Property->SetParent(0);
    this->Internals->Properties.erase(it);
    }

  vtkstd::vector<vtkStdString>::iterator iter =
    vtkstd::find(this->Internals->PropertyNamesInOrder.begin(),
                 this->Internals->PropertyNamesInOrder.end(),
                 name);
  if(iter != this->Internals->PropertyNamesInOrder.end())
    {
    this->Internals->PropertyNamesInOrder.erase(iter);
    }
}

//---------------------------------------------------------------------------
void vtkSMProxy::AddProperty(const char* name, vtkSMProperty* prop)
{
  if (!prop)
    {
    return;
    }
  if (!name)
    {
    vtkErrorMacro("Can not add a property without a name.");
    return;
    }

  // Check if the property already exists. If it does, we will
  // replace it (and remove the observer from it)
  vtkSMProxyInternals::PropertyInfoMap::iterator it =
    this->Internals->Properties.find(name);

  if (it != this->Internals->Properties.end())
    {
    vtkWarningMacro("Property " << name  << " already exists. Replacing");
    vtkSMProperty* oldProp = it->second.Property.GetPointer();
    if (it->second.ObserverTag > 0)
      {
      oldProp->RemoveObserver(it->second.ObserverTag);
      }
    oldProp->SetParent(0);
    }

  unsigned int tag=0;

  vtkSMProxyObserver* obs = vtkSMProxyObserver::New();
  obs->SetProxy(this);
  obs->SetPropertyName(name);
  // We have to store the tag in order to be able to remove
  // the observer later.
  tag = prop->AddObserver(vtkCommand::ModifiedEvent, obs);
  obs->Delete();

  prop->SetParent(this);

  vtkSMProxyInternals::PropertyInfo newEntry;
  newEntry.Property = prop;
  newEntry.ObserverTag = tag;
  this->Internals->Properties[name] = newEntry;

  // BUG: Hmm, if this replaces an existing property, are we ending up with that
  // name being pushed in twice in the PropertyNamesInOrder list?

  // Add the property name to the vector of property names.
  // This vector keeps track of the order in which properties
  // were added.
  this->Internals->PropertyNamesInOrder.push_back(name);
}

//---------------------------------------------------------------------------
bool vtkSMProxy::UpdateProperty(const char* name, int force)
{
  vtkSMProxyInternals::PropertyInfoMap::iterator it =
    this->Internals->Properties.find(name);
  if (it == this->Internals->Properties.end())
    {
    return false;
    }

  if (!it->second.ModifiedFlag && !force)
    {
    return false;
    }

  if (it->second.Property->GetInformationOnly())
    {
    // cannot update information only properties.
    return false;
    }

  this->CreateVTKObjects();

  // In case this property is a self property and causes
  // another UpdateVTKObjects(), make sure that it does
  // not cause recursion. If this is not set, UpdateVTKObjects()
  // that is caused by UpdateProperty() can end up calling trying
  // to push the same property.
  it->second.ModifiedFlag = 0;

  vtkSMMessage message;
  it->second.Property->WriteTo(&message);
  this->PushState(&message);

  // Fire event to let everyone know that a property has been updated.
  this->InvokeEvent(vtkCommand::UpdatePropertyEvent,
    const_cast<char*>(name));
  this->MarkModified(this);
  return true;
}

//---------------------------------------------------------------------------
void vtkSMProxy::SetPropertyModifiedFlag(const char* name, int flag)
{
  if (this->DoNotModifyProperty)
    {
    return;
    }

  vtkSMProxyInternals::PropertyInfoMap::iterator it =
    this->Internals->Properties.find(name);
  if (it == this->Internals->Properties.end())
    {
    return;
    }

  this->InvokeEvent(vtkCommand::PropertyModifiedEvent, (void*)name);
  
  vtkSMProperty* prop = it->second.Property.GetPointer();
  if (prop->GetInformationOnly())
    {
    // Information only property is modified...nothing much to do.
    return;
    }

  it->second.ModifiedFlag = flag;

  if (flag && !this->DoNotUpdateImmediately && prop->GetImmediateUpdate())
    {
    this->UpdateProperty(it->first.c_str());
    }
  else
    {
    this->PropertiesModified = 1;
    }
}

//-----------------------------------------------------------------------------
void vtkSMProxy::MarkAllPropertiesAsModified()
{
  vtkSMProxyInternals::PropertyInfoMap::iterator it;
  for (it = this->Internals->Properties.begin();
       it != this->Internals->Properties.end(); it++)
    {
    // Not the most efficient way to set the flag, but probably the safest.
    this->SetPropertyModifiedFlag(it->first.c_str(), 1);
    }
}

//---------------------------------------------------------------------------
void vtkSMProxy::UpdatePropertyInformation(vtkSMProperty* prop)
{
  this->UpdatePropertyInformationInternal(prop);
}

//---------------------------------------------------------------------------
void vtkSMProxy::UpdatePropertyInformation()
{
  this->UpdatePropertyInformationInternal(NULL);
}

//---------------------------------------------------------------------------
void vtkSMProxy::UpdatePropertyInformationInternal(
  vtkSMProperty* single_property/*=NULL*/)
{
  this->CreateVTKObjects();

  if (!this->ObjectsCreated)
    {
    return;
    }

  bool some_thing_to_fetch = false;
  vtkSMMessage message;
  Variant* var = message.AddExtension(PullRequest::arguments);
  var->set_type(Variant::STRING);

  vtkSMProxyInternals::PropertyInfoMap::iterator it;
  if (single_property != NULL)
    {
    if (single_property->GetInformationOnly())
      {
      var->add_txt(it->first.c_str());
      some_thing_to_fetch = true;
      }
    }
  else
    {
    // Update all information properties.
    for (it  = this->Internals->Properties.begin();
      it != this->Internals->Properties.end(); ++it)
      {
      vtkSMProperty* prop = it->second.Property.GetPointer();
      if (prop->GetInformationOnly())
        {
        var->add_txt(it->first.c_str());
        some_thing_to_fetch = true;
        }
      }
    }

  if (!some_thing_to_fetch)
    {
    return;
    }

  // Hmm, this changes message itself. Funky.
  this->PullState(&message);

  for (int i=0; i < message.ExtensionSize(ProxyState::property); ++i)
    {
    const ProxyState_Property *prop_message = &message.GetExtension(ProxyState::property, i);
    const char* pname = prop_message->name().c_str();
    it = this->Internals->Properties.find(pname);
    if (it != this->Internals->Properties.end() &&
      it->second.Property->GetInformationOnly())
      {
      it->second.Property->ReadFrom(&message, i);
      }
    }

  // Make sure all dependent domains are updated. UpdateInformation()
  // might have produced new information that invalidates the domains.
  for (it  = this->Internals->Properties.begin();
    it != this->Internals->Properties.end();
    ++it)
    {
    vtkSMProperty* prop = it->second.Property.GetPointer();
    if (prop->GetInformationOnly())
      {
      prop->UpdateDependentDomains();
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMProxy::UpdateVTKObjects()
{
  this->CreateVTKObjects();
  if (!this->ObjectsCreated || this->InUpdateVTKObjects ||
    !this->ArePropertiesModified())
    {
    return;
    }

  this->InUpdateVTKObjects = 1;

  // iterate over all properties and push modified ones.

  vtkSMMessage message;
  vtkSMProxyInternals::PropertyInfoMap::iterator iter;
  for (iter = this->Internals->Properties.begin();
    iter != this->Internals->Properties.end(); ++iter)
    {
    vtkSMProperty* property = iter->second.Property;
    if (property && !property->GetInformationOnly() &&
      iter->second.ModifiedFlag)
      {
      property->WriteTo(&message);

      // Fire event to let everyone know that a property has been updated.
      // This is currently used by vtkSMLink. Need to see if we can avoid this
      // as firing these events ain't inexpensive.
      this->InvokeEvent(vtkCommand::UpdatePropertyEvent,
        const_cast<char*>(iter->first.c_str()));
      }
    }
  this->InUpdateVTKObjects = 0;
  this->PropertiesModified = false;

  // Send the message
  this->PushState(&message);

  this->MarkModified(this);
  this->InvokeEvent(vtkCommand::UpdateEvent, 0);
}

//---------------------------------------------------------------------------
void vtkSMProxy::CreateVTKObjects()
{
  if (this->ObjectsCreated)
    {
    return;
    }
  this->ObjectsCreated = 1;
  this->WarnIfDeprecated();

  vtkSMMessage message;
  message.SetExtension(DefinitionHeader::client_class, this->GetClassName());
  message.SetExtension(DefinitionHeader::server_class, "vtkPMProxy"
    /* this->GetKernelClassName() */);
  message.SetExtension(ProxyState::xml_group, this->GetXMLGroup());
  message.SetExtension(ProxyState::xml_name, this->GetXMLName());
  this->PushState(&message);

#ifdef FIXME
  // ensure that this is happening correctly in PMProxy
  if (this->VTKClassName && this->VTKClassName[0] != '\0')
    {
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    vtkClientServerStream stream;
    this->VTKObjectID =
      pm->NewStreamObject(this->VTKClassName, stream);
    stream << vtkClientServerStream::Invoke 
           << pm->GetProcessModuleID()
           << "RegisterProgressEvent" 
           << this->VTKObjectID
           << static_cast<int>(this->VTKObjectID.ID)
           << vtkClientServerStream::End;
    pm->SendStream(this->ConnectionID, this->Servers, stream);
    }

  vtkSMProxyInternals::ProxyMap::iterator it2 =
    this->Internals->SubProxies.begin();
  for( ; it2 != this->Internals->SubProxies.end(); it2++)
    {
    it2->second.GetPointer()->CreateVTKObjects();
    }
#endif
}

//---------------------------------------------------------------------------
bool vtkSMProxy::WarnIfDeprecated()
{
  if (this->Deprecated)
    {
    vtkWarningMacro("Proxy (" << this->XMLGroup << ", " << this->XMLName 
      << ")  has been deprecated in ParaView " <<
      this->Deprecated->GetAttribute("deprecated_in") << 
      " and will be removed by ParaView " <<
      this->Deprecated->GetAttribute("to_remove_in") << ". " <<
      (this->Deprecated->GetCharacterData()? 
       this->Deprecated->GetCharacterData() : ""));
    return true;
    }
  return false;
}

//---------------------------------------------------------------------------
void vtkSMProxy::AddConsumer(vtkSMProperty* property, vtkSMProxy* proxy)
{
  int found=0;
  vtkstd::vector<vtkSMProxyInternals::ConnectionInfo>::iterator i = 
    this->Internals->Consumers.begin();
  for(; i != this->Internals->Consumers.end(); i++)
    {
    if ( i->Property == property && i->Proxy == proxy )
      {
      found = 1;
      break;
      }
    }

  if (!found)
    {
    vtkSMProxyInternals::ConnectionInfo info(property, proxy);
    this->Internals->Consumers.push_back(info);
    }
  
}

//---------------------------------------------------------------------------
void vtkSMProxy::RemoveConsumer(vtkSMProperty* property, vtkSMProxy*)
{
  vtkstd::vector<vtkSMProxyInternals::ConnectionInfo>::iterator i = 
    this->Internals->Consumers.begin();
  for(; i != this->Internals->Consumers.end(); i++)
    {
    if ( i->Property == property )
      {
      this->Internals->Consumers.erase(i);
      break;
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMProxy::RemoveAllConsumers()
{
  this->Internals->Consumers.erase(this->Internals->Consumers.begin(),
                                   this->Internals->Consumers.end());
}

//---------------------------------------------------------------------------
unsigned int vtkSMProxy::GetNumberOfConsumers()
{
  return static_cast<unsigned int>(this->Internals->Consumers.size());
}

//---------------------------------------------------------------------------
vtkSMProxy* vtkSMProxy::GetConsumerProxy(unsigned int idx)
{
  return this->Internals->Consumers[idx].Proxy;
}

//---------------------------------------------------------------------------
vtkSMProperty* vtkSMProxy::GetConsumerProperty(unsigned int idx)
{
  return this->Internals->Consumers[idx].Property;
}

//---------------------------------------------------------------------------
void vtkSMProxy::AddProducer(vtkSMProperty* property, vtkSMProxy* proxy)
{
  int found=0;
  vtkstd::vector<vtkSMProxyInternals::ConnectionInfo>::iterator i = 
    this->Internals->Producers.begin();
  for(; i != this->Internals->Producers.end(); i++)
    {
    if ( i->Property == property && i->Proxy == proxy )
      {
      found = 1;
      break;
      }
    }

  if (!found)
    {
    vtkSMProxyInternals::ConnectionInfo info(property, proxy);
    this->Internals->Producers.push_back(info);
    }
  
}

//---------------------------------------------------------------------------
void vtkSMProxy::RemoveProducer(vtkSMProperty* property, vtkSMProxy* proxy)
{
  vtkstd::vector<vtkSMProxyInternals::ConnectionInfo>::iterator i = 
    this->Internals->Producers.begin();
  for(; i != this->Internals->Producers.end(); i++)
    {
    if ( i->Property == property && i->Proxy == proxy )
      {
      this->Internals->Producers.erase(i);
      break;
      }
    }
}

//---------------------------------------------------------------------------
unsigned int vtkSMProxy::GetNumberOfProducers()
{
  return static_cast<unsigned int>(this->Internals->Producers.size());
}

//---------------------------------------------------------------------------
vtkSMProxy* vtkSMProxy::GetProducerProxy(unsigned int idx)
{
  return this->Internals->Producers[idx].Proxy;
}

//---------------------------------------------------------------------------
vtkSMProperty* vtkSMProxy::GetProducerProperty(unsigned int idx)
{
  return this->Internals->Producers[idx].Property;
}

//----------------------------------------------------------------------------
void vtkSMProxy::PostUpdateData()
{
  unsigned int numProducers = this->GetNumberOfProducers();
  for (unsigned int i=0; i<numProducers; i++)
    {
    if (this->GetProducerProxy(i)->NeedsUpdate)
      {
      this->GetProducerProxy(i)->PostUpdateData();
      }
    }
  if (this->NeedsUpdate)
    {
    this->InvokeEvent(vtkCommand::UpdateDataEvent, 0);
    this->NeedsUpdate = false;
    }
}

//----------------------------------------------------------------------------
void vtkSMProxy::MarkModified(vtkSMProxy* modifiedProxy)
{
  /*
   * UpdatePropertyInformation() is now explicitly called in
   * UpdatePipelineInformation(). The calling on UpdatePropertyInformation()
   * was not really buying us much as far as keeping dependent domains updated
   * was concerned, for unless UpdatePipelineInformation was called on the
   * reader/filter, updating infor properties was not going to yeild any
   * changed values. Removing this also allows for linking for info properties
   * and properties using property links.
   * A side effect of this may be that the 3DWidgets information properties wont get
   * updated on setting "action" properties such as PlaceWidget.
  if (this->ObjectsCreated)
    {
    // If not created yet, don't worry syncing the info properties.
    this->UpdatePropertyInformation();
    }
  */
  if (!this->InMarkModified)
    {
    this->InMarkModified = 1;
    this->InvokeEvent(vtkCommand::ModifiedEvent, (void*)modifiedProxy);
    this->MarkDirty(modifiedProxy);
    this->InMarkModified = 0;
    }
}

//----------------------------------------------------------------------------
void vtkSMProxy::MarkDirty(vtkSMProxy* modifiedProxy)
{
  if (this->NeedsUpdate)
    {
    return;
    }
    
  this->MarkConsumersAsDirty(modifiedProxy);
  this->NeedsUpdate = true;
}

//----------------------------------------------------------------------------
void vtkSMProxy::MarkConsumersAsDirty(vtkSMProxy* modifiedProxy)
{
  unsigned int numConsumers = this->GetNumberOfConsumers();
  for (unsigned int i=0; i<numConsumers; i++)
    {
    vtkSMProxy* cons = this->GetConsumerProxy(i);
    if (cons)
      {
      cons->MarkDirty(modifiedProxy);
      }
    }
}

//----------------------------------------------------------------------------
vtkSMProperty* vtkSMProxy::NewProperty(const char* name)
{
  vtkSMProperty* property = this->GetProperty(name);
  if (property)
    {
    property->Register(this);
    return property;
    }

  vtkPVXMLElement* element = this->XMLElement;
  if (!element)
    {
    return 0;
    }

  vtkPVXMLElement* propElement = 0;
  for(unsigned int i=0; i < element->GetNumberOfNestedElements(); ++i)
    {
    propElement = element->GetNestedElement(i);
    if (strcmp(propElement->GetName(), "SubProxy") != 0)
      {
      const char* pname = propElement->GetAttribute("name");
      if (pname && strcmp(name, pname) == 0)
        {
        break;
        }
      }
    propElement = 0;
    }
  if (!propElement)
    {
    return 0;
    }
  return this->NewProperty(name, propElement);
}

//----------------------------------------------------------------------------
vtkSMProperty* vtkSMProxy::NewProperty(const char* name, 
                                       vtkPVXMLElement* propElement)
{
  vtkSMProperty* property = this->GetProperty(name);
  if (property)
    {
    return property;
    }
  
  if (!propElement)
    {
    return 0;
    }

  vtkObject* object = 0;
  vtksys_ios::ostringstream cname;
  cname << "vtkSM" << propElement->GetName() << ends;
  object = vtkInstantiator::CreateInstance(cname.str().c_str());

  property = vtkSMProperty::SafeDownCast(object);
  if (property)
    {
    int old_val = this->DoNotUpdateImmediately;
    int old_val2 = this->DoNotModifyProperty;
    this->DoNotUpdateImmediately = 1;

    // Internal properties should not be created as modified.
    // Otherwise, properties like ForceUpdate get pushed and
    // cause problems.
    int is_internal;
    if (property->GetIsInternal())
      {
      this->DoNotModifyProperty = 1;
      }
    if (propElement->GetScalarAttribute("is_internal", &is_internal))
      {
      if (is_internal)
        {
        this->DoNotModifyProperty = 1;
        }
      }
    this->AddProperty(name, property);
    if (!property->ReadXMLAttributes(this, propElement))
      {
      vtkErrorMacro("Could not parse property: " << propElement->GetName());
      this->DoNotUpdateImmediately = old_val;
      return 0;
      }
    this->DoNotUpdateImmediately = old_val;
    this->DoNotModifyProperty = old_val2;

    // Properties should be created as modified unless they
    // are internal.
//     if (!property->GetIsInternal())
//       {
//       this->Internals->Properties[name].ModifiedFlag = 1;
//       }
    property->Delete();
    }
  else
    {
    vtkErrorMacro("Could not instantiate property: " << propElement->GetName());
    }

  return property;
}

//---------------------------------------------------------------------------
int vtkSMProxy::ReadXMLAttributes(
  vtkSMProxyManager* pm, vtkPVXMLElement* element)
{
  this->SetXMLElement(element);

  // Read the common attributes.
  const char* className = element->GetAttribute("class");
  if(className)
    {
    this->SetVTKClassName(className);
    }

  const char* xmlname = element->GetAttribute("name");
  if(xmlname)
    {
    this->SetXMLName(xmlname);
    this->SetXMLLabel(xmlname);
    }

  const char* xmllabel = element->GetAttribute("label");
  if (xmllabel)
    {
    this->SetXMLLabel(xmllabel);
    }

  const char* processes = element->GetAttribute("processes");
  if (processes)
    {
    vtkTypeUInt32 uiprocesses = 0;
    vtkstd::string strprocesses = processes;
    if (strprocesses.find("client") != vtkstd::string::npos)
      {
      uiprocesses |= vtkProcessModule2::CLIENT;
      }
    if (strprocesses.find("renderserver") != vtkstd::string::npos)
      {
      uiprocesses |= vtkProcessModule2::RENDER_SERVER;
      }
    if (strprocesses.find("dataserver") != vtkstd::string::npos)
      {
      uiprocesses |= vtkProcessModule2::DATA_SERVER;
      }
    this->SetLocation(uiprocesses);
    }

  // Locate documentation.
  for (unsigned int cc=0; cc < element->GetNumberOfNestedElements(); ++cc)
    {
    vtkPVXMLElement* subElem = element->GetNestedElement(cc);
    if (strcmp(subElem->GetName(), "Documentation") == 0)
      {
      this->Documentation->SetDocumentationElement(subElem);
      }
    else if (strcmp(subElem->GetName(), "Hints") == 0)
      {
      this->SetHints(subElem);
      }
    else if (strcmp(subElem->GetName(), "Deprecated") == 0)
      {
      this->SetDeprecated(subElem);
      }
    }

  // Create all properties
  if (!this->CreateSubProxiesAndProperties(pm, element))
    {
    return 0;
    }

  this->SetXMLElement(0);
  return 1;
}

//---------------------------------------------------------------------------
int vtkSMProxy::CreateSubProxiesAndProperties(vtkSMProxyManager* pm, 
  vtkPVXMLElement *element)
{
  if (!element || !pm)
    {
    return 0;
    }

  for(unsigned int i=0; i < element->GetNumberOfNestedElements(); ++i)
    {
    vtkPVXMLElement* propElement = element->GetNestedElement(i);
    if (strcmp(propElement->GetName(), "SubProxy") != 0)
      {
      const char* name = propElement->GetAttribute("name");
      vtkstd::string tagName = propElement->GetName();
      if (name && tagName.find("Property") == (tagName.size()-8))
        {
        this->NewProperty(name, propElement);
        }
      }
    }
  return 1;
}

//---------------------------------------------------------------------------
vtkSMPropertyIterator* vtkSMProxy::NewPropertyIterator()
{
  vtkSMPropertyIterator* iter = vtkSMPropertyIterator::New();
  iter->SetProxy(this);
  return iter;
}

//---------------------------------------------------------------------------
void vtkSMProxy::Copy(vtkSMProxy* src)
{
  this->Copy(src, 0, 
    vtkSMProxy::COPY_PROXY_PROPERTY_VALUES_BY_REFERENCE);
}

//---------------------------------------------------------------------------
void vtkSMProxy::Copy(vtkSMProxy* src, const char* exceptionClass)
{
  this->Copy(src, exceptionClass,
    vtkSMProxy::COPY_PROXY_PROPERTY_VALUES_BY_REFERENCE);
}

//---------------------------------------------------------------------------
void vtkSMProxy::Copy(vtkSMProxy* src, const char* exceptionClass,
  int proxyPropertyCopyFlag)
{
  if (!src)
    {
    return;
    }

  vtkSMPropertyIterator* iter = this->NewPropertyIterator();
  for(iter->Begin(); !iter->IsAtEnd(); iter->Next())
    {
    const char* key = iter->GetKey();
    vtkSMProperty* dest = iter->GetProperty();
    if (key && dest)
      {
      vtkSMProperty* source = src->GetProperty(key);
      if (source)
        {
        if (!exceptionClass || !dest->IsA(exceptionClass))
          {
          vtkSMProxyProperty* pp = vtkSMProxyProperty::SafeDownCast(dest);
          if (!pp || proxyPropertyCopyFlag == 
            vtkSMProxy::COPY_PROXY_PROPERTY_VALUES_BY_REFERENCE)
            {
            dest->Copy(source);
            }
          else
            {
            pp->DeepCopy(source, exceptionClass, 
              vtkSMProxy::COPY_PROXY_PROPERTY_VALUES_BY_CLONING);
            }
          }
        }
      }
    }

  iter->Delete();
}

//---------------------------------------------------------------------------
void vtkSMProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
 
  os << indent << "Name: " 
     << (this->Name ? this->Name : "(null)")
     << endl;
  os << indent << "VTKClassName: " 
     << (this->VTKClassName ? this->VTKClassName : "(null)")
     << endl;
  os << indent << "XMLName: "
     << (this->XMLName ? this->XMLName : "(null)")
     << endl;
  os << indent << "XMLGroup: " 
    << (this->XMLGroup ? this->XMLGroup : "(null)")
    << endl;
  os << indent << "XMLLabel: " 
    << (this->XMLLabel? this->XMLLabel : "(null)")
    << endl;
  os << indent << "Documentation: " << this->Documentation << endl;
  os << indent << "ObjectsCreated: " << this->ObjectsCreated << endl;
  os << indent << "Hints: " ;
  if (this->Hints)
    {
    this->Hints->PrintSelf(os, indent);
    }
  else
    {
    os << "(null)" << endl;
    }

  vtkSMPropertyIterator* iter = this->NewPropertyIterator();
  if (iter)
    {
    for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
      {
      const char* key = iter->GetKey();
      vtkSMProperty* property = iter->GetProperty();
      if (key)
        {
        os << indent << "Property (" << key << "): ";
        if (property)
          {
          os << endl;
          property->PrintSelf(os, indent.GetNextIndent());
          }
        else
          {
          os << "(none)" << endl; 
          }
        }
      }
    iter->Delete();
    }
}
