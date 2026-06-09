#include "modules/graph/Cas.h"

#include <algorithm>
#include <cmath>

namespace superwin {

std::vector<double> SolveRoots(const Expr& f, double lo, double hi, int samples) {
    std::vector<double> roots;
    if (!f.valid() || !(hi > lo) || samples < 2) return roots;

    auto pushRoot = [&](double r) {
        if (!std::isfinite(r)) return;
        for (double e : roots)
            if (std::fabs(e - r) < 1e-6 * (1.0 + std::fabs(r))) return;
        roots.push_back(r);
    };

    const double step = (hi - lo) / samples;
    double xPrev = lo, fPrev = f.eval(lo);
    if (std::isfinite(fPrev) && std::fabs(fPrev) < 1e-12) pushRoot(xPrev);

    for (int i = 1; i <= samples; ++i) {
        const double x = lo + i * step;
        const double fx = f.eval(x);
        if (std::isfinite(fPrev) && std::isfinite(fx)) {
            if ((fPrev < 0 && fx > 0) || (fPrev > 0 && fx < 0)) {
                // Bracketed sign change: bisect to the crossing.
                double a = xPrev, b = x, fa = fPrev;
                for (int it = 0; it < 80; ++it) {
                    const double m = 0.5 * (a + b);
                    const double fm = f.eval(m);
                    if (!std::isfinite(fm)) break;
                    if ((fa < 0) == (fm < 0)) { a = m; fa = fm; } else { b = m; }
                }
                const double r = 0.5 * (a + b);
                const double fr = f.eval(r);
                // A real root drives f to ~0; a pole keeps |f| large -> discard it.
                if (std::isfinite(fr) && std::fabs(fr) < 1e-6) pushRoot(r);
            } else if (std::fabs(fx) < 1e-12) {
                pushRoot(x);
            }
        }
        xPrev = x;
        fPrev = fx;
    }

    std::sort(roots.begin(), roots.end());
    return roots;
}

}  // namespace superwin
