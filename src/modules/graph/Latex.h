// Convert a (MathLive) LaTeX string into the plain infix grammar that ParseExpr
// understands, so the WebView math editor can drive the existing CAS.
//
// Handles: {grouping}, ^ superscripts, \frac, \sqrt (and \sqrt[n]), the trig /
// log / exp functions, \cdot \times \div, \pi, \left \right, \operatorname, and
// \sum / \prod / \int with their limits. Implicit multiplication (e.g. "2x") is
// left for the parser to resolve.
#pragma once

#include <string>

namespace superwin {

std::string LatexToInfix(const std::string& latex);

}  // namespace superwin
