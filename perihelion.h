#ifndef PERIHELION_IMPORTED
#define PERIHELION_IMPORTED
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "helpers.h"
enum class PerihelionMethod {
    NEWTON_RAPHSON,
    HALLEY
};
inline double perihelion_diagonal_one(
    double current_guess,
    const std::vector<double>& history,
    const std::vector<double>& costs,
    PerihelionMethod method = PerihelionMethod::HALLEY,
    double max_step = 0.25
) {
    const size_t n = std::min(history.size(), costs.size());
    if (n < 2) {
        return std::max(0.0, std::min(1.0, current_guess));
    }
    const size_t desired_local = (method == PerihelionMethod::HALLEY) ? 10 : 8;
    const size_t k_local = std::min(n, desired_local);
    std::vector<std::pair<double, size_t>> distIndices(n);
    for (size_t i = 0; i < n; ++i) {
        distIndices[i] = {std::fabs(history[i] - current_guess), i};
    }
    std::partial_sort(distIndices.begin(), distIndices.begin() + k_local, distIndices.end());
    std::vector<double> shiftedHistory(k_local, 0.0);
    std::vector<double> localCosts(k_local, 0.0);
    for (size_t i = 0; i < k_local; ++i) {
        const size_t idx = distIndices[i].second;
        shiftedHistory[i] = history[idx] - current_guess;
        localCosts[i] = costs[idx];
    }
    const int degree = (method == PerihelionMethod::HALLEY && k_local >= 4) ? 3 : 2;
    const std::vector<double> coeffs = solvePolynomialLeastSquares(shiftedHistory, localCosts, 1, degree, 1e-6);
    if (coeffs.size() < 3) {
        return std::max(0.0, std::min(1.0, current_guess));
    }
    const double f1 = coeffs[1];
    const double f2 = 2.0 * coeffs[2];
    const double f3 = (coeffs.size() > 3) ? (6.0 * coeffs[3]) : 0.0;
    if (std::fabs(f1) < 1e-12) {
        return std::max(0.0, std::min(1.0, current_guess));
    }
    double step = 0.0;
    if (f2 > 1e-8) {
        const double newton_step = -f1 / f2;
        if (method == PerihelionMethod::HALLEY) {
            const double denom = 2.0 * f2 * f2 - f1 * f3;
            if (denom > 1e-8) {
                const double halley_step = -(2.0 * f1 * f2) / denom;
                step = (halley_step * f1 < 0.0) ? halley_step : newton_step;
            } else {
                step = newton_step;
            }
        } else {
            step = newton_step;
        }
    } else {
        step = -0.1 * f1;
    }
    step = std::max(-max_step, std::min(max_step, step));
    return std::max(0.0, std::min(1.0, current_guess + step));
}
inline std::vector<double> perihelion_diagonal_all(
    const std::vector<double>& current_guess,
    const std::vector<std::vector<double>>& point_history,
    const std::vector<double>& costs,
    PerihelionMethod method = PerihelionMethod::HALLEY,
    double max_step = 0.25
) {
    std::vector<double> new_guess = current_guess;
    clampUnitPoint(new_guess);
    const size_t n = std::min(point_history.size(), costs.size());
    const size_t dim = current_guess.size();
    if (n < 2 || dim == 0) {
        return new_guess;
    }
    const size_t k_nd = std::min(n, std::max<size_t>(12, dim * 4));
    std::vector<std::pair<double, size_t>> distIndices(n);
    for (size_t j = 0; j < n; ++j) {
        distIndices[j] = {nDimensionalDistance(point_history[j], new_guess), j};
    }
    std::partial_sort(distIndices.begin(), distIndices.begin() + k_nd, distIndices.end());
    std::vector<double> param_history(k_nd, 0.0);
    std::vector<double> param_costs(k_nd, 0.0);
    for (size_t i = 0; i < dim; ++i) {
        for (size_t j = 0; j < k_nd; ++j) {
            const size_t idx = distIndices[j].second;
            param_history[j] = (i < point_history[idx].size()) ? point_history[idx][i] : new_guess[i];
            param_costs[j] = costs[idx];
        }
        new_guess[i] = perihelion_diagonal_one(new_guess[i], param_history, param_costs, method, max_step);
    }
    return new_guess;
}
#endif