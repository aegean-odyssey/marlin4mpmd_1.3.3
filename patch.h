/* PATCH.H
   include this file up-front so we can use the make facility to
   configure the 05Alimit or 10Alimit "flavors" of the firmware
*/

#include "Configuration_STM.h"
#include "macros.h"
#include "boards.h"
#include "Version.h"
#include "Configuration.h"

#ifdef MAKE_10ALIMIT
#define PIDTEMPBED
#undef  BED_LIMIT_SWITCHING
#endif

#ifdef MAKE_05ALIMIT
#undef  PIDTEMPBED
#define BED_LIMIT_SWITCHING
#endif
