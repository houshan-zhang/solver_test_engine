#ifndef __SCIP_PRESOL_MULTIAGG_H__
#define __SCIP_PRESOL_MULTIAGG_H__

#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the multi-aggregation presolver and includes it in SCIP */
SCIP_EXPORT
SCIP_RETCODE SCIPincludePresolMultiagg(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
