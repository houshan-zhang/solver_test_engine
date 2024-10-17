#ifndef __SCIP_PRESOL_EXTENDED_SPARSIFY_H__
#define __SCIP_PRESOL_EXTENDED_SPARSIFY_H__

#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the extended sparsify presolver and includes it in SCIP */
SCIP_EXPORT
SCIP_RETCODE SCIPincludePresolExtendedSparsify(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
