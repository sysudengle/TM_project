#ifndef __MOLECULE_H_
#define __MOLECULE_H_

#include "../../src/tm_scope.h"
#include "../../src/tm.h"

typedef struct Molecule{
  tm_double x, y, z;
  tm_double f_x, f_y, f_z;
} Molecule;

#endif //__MOLECULE_H_
