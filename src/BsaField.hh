#ifndef Bsa_Field_hh
#define Bsa_Field_hh

namespace Bsa {
  class Field {
  public:
    virtual ~Field() {}
  public:
    virtual const char* name   ()            const = 0;
  };
};

#endif
