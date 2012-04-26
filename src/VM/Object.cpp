#include "Object.h"

namespace magpie
{
  using std::ostream;

  temp<BoolObject> Object::create(bool value)
  {
    return Memory::makeTemp(new BoolObject(value));
  }
  
  temp<ClassObject> Object::createClass(gc<String> name)
  {
    return Memory::makeTemp(new ClassObject(name));
  }
  
  temp<NumberObject> Object::create(double value)
  {
    return Memory::makeTemp(new NumberObject(value));
  }
  
  temp<StringObject> Object::create(gc<String> value)
  {
    return Memory::makeTemp(new StringObject(value));
  }
}
