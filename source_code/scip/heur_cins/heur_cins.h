/**@file   heur_cins.h
 * @ingroup PRIMALHEURISTICS
 * @brief  LNS heuristic that combines the incumbent with the LP optimum
 *
 * CINS is a large neighborhood search heuristic, i.e., it requires a LP solution or a feasible solution. It solves a
 * sub-SCIP that is created by fixing variables
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_HEUR_CINS_H__
#define __SCIP_HEUR_CINS_H__

#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates CINS primal heuristic and includes it in SCIP
 *
 *  @ingroup PrimalHeuristicIncludes
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeHeurCins(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
