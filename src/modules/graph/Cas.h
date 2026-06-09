// Numeric helpers layered on top of the symbolic Expr engine, used by the
// Calculator's Class IV CAS panel. Pure logic in superwin_core (unit-tested).
#pragma once

#include <string>
#include <vector>

#include "modules/graph/Expr.h"

namespace superwin {

// Numerically solve f(x) = 0 for real roots on [lo, hi]. Scans for sign changes
// and brackets each with bisection, discarding apparent crossings at poles
// (where |f| stays large). Returns roots ascending, de-duplicated. `samples` is
// the scan resolution; more samples find closely spaced roots at higher cost.
std::vector<double> SolveRoots(const Expr& f, double lo = -100.0, double hi = 100.0,
                               int samples = 4000);

}  // namespace superwin
