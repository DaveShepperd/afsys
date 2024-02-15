/* See LICENSE.txt for license details */

#ifndef _ANY_PROTO_H_
#define _ANY_PROTO_H_

#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == CYRIX_MGX)
#include "cy_proto.h"
#endif
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == PHOENIX)
#include "phx_proto.h"
#endif
#if HOST_BOARD_CLASS && ((HOST_BOARD & HOST_BOARD_CLASS) == I86_PC)
#include "i86_proto.h"
#endif

#endif      /* _ANY_PROTO_H_ */

