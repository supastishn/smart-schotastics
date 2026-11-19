#ifndef LBFGS_IMPORTED
#define LBFGS_IMPORTED
#include <algorithm>
#include <cmath>
#include <vector>
#include "helpers.h"
struct LbfgsConfig {
    int max_iterations = 30;
    int history_size = 10;
    double grad_tolerance = 1e-6;
    double fd_step = 1e-5;
    double armijo_c1 = 1e-4;
};
inline double dotProduct(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0.0;
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}
template <typename ObjectiveFn>
inline std::vector<double> finiteDifferenceGradientBox(
    const std::vector<double>& x,
    ObjectiveFn&& objective_fn,
    double fd_step
) {
    const size_t dim = x.size();
    std::vector<double> grad(dim, 0.0);
    std::vector<double> probe = x;
    for (size_t i = 0; i < dim; ++i) {
        const double x_plus = std::min(1.0, x[i] + fd_step);
        const double x_minus = std::max(0.0, x[i] - fd_step);
        const double span = x_plus - x_minus;
        if (span <= 1e-12) {
            grad[i] = 0.0;
            continue;
        }
        probe[i] = x_plus;
        const double f_plus = objective_fn(probe);
        probe[i] = x_minus;
        const double f_minus = objective_fn(probe);
        probe[i] = x[i];
        grad[i] = (f_plus - f_minus) / span;
    }
    return grad;
}
inline std::vector<double> projectGradientForBox(const std::vector<double>& x, const std::vector<double>& grad) {
    const size_t dim = x.size();
    std::vector<double> pg = grad;
    for (size_t i = 0; i < dim; ++i) {
        if ((x[i] <= 0.0 && grad[i] > 0.0) || (x[i] >= 1.0 && grad[i] < 0.0)) {
            pg[i] = 0.0;
        }
    }
    return pg;
}
template <typename ObjectiveFn>
inline std::vector<double> lbfgsMinimizeBox(
    const std::vector<double>& start_point,
    ObjectiveFn&& objective_fn,
    const LbfgsConfig& config = LbfgsConfig{}
) {
    std::vector<double> x = start_point;
    clampUnitPoint(x);
    const size_t dim = x.size();
    if (dim == 0) {
        return x;
    }
    double f_curr = objective_fn(x);
    std::vector<double> grad = finiteDifferenceGradientBox(x, objective_fn, config.fd_step);
    std::vector<std::vector<double>> s_history;
    std::vector<std::vector<double>> y_history;
    std::vector<double> rho_history;
    for (int iter = 0; iter < config.max_iterations; ++iter) {
        const std::vector<double> pg = projectGradientForBox(x, grad);
        const double pg_norm = std::sqrt(dotProduct(pg, pg));
        if (pg_norm < config.grad_tolerance) {
            break;
        }
        const int m = static_cast<int>(s_history.size());
        std::vector<double> q = pg;
        std::vector<double> alpha(m, 0.0);
        for (int i = m - 1; i >= 0; --i) {
            alpha[i] = rho_history[i] * dotProduct(s_history[i], q);
            for (size_t d = 0; d < dim; ++d) {
                q[d] -= alpha[i] * y_history[i][d];
            }
        }
        double gamma = 1.0;
        if (m > 0) {
            const double yy = dotProduct(y_history.back(), y_history.back());
            const double sy = dotProduct(s_history.back(), y_history.back());
            if (yy > 1e-12 && sy > 1e-12) {
                gamma = std::max(1e-6, std::min(1e6, sy / yy));
            }
        }
        std::vector<double> r(dim, 0.0);
        for (size_t d = 0; d < dim; ++d) {
            r[d] = gamma * q[d];
        }
        for (int i = 0; i < m; ++i) {
            const double beta = rho_history[i] * dotProduct(y_history[i], r);
            for (size_t d = 0; d < dim; ++d) {
                r[d] += s_history[i][d] * (alpha[i] - beta);
            }
        }
        std::vector<double> dir(dim, 0.0);
        for (size_t d = 0; d < dim; ++d) {
            dir[d] = -r[d];
            if ((x[d] <= 0.0 && dir[d] < 0.0) || (x[d] >= 1.0 && dir[d] > 0.0)) {
                dir[d] = 0.0;
            }
        }
        if (dotProduct(dir, pg) >= 0.0) {
            s_history.clear();
            y_history.clear();
            rho_history.clear();
            for (size_t d = 0; d < dim; ++d) {
                dir[d] = -pg[d];
            }
        }
        double step_size = 1.0;
        bool accepted = false;
        std::vector<double> x_next(dim, 0.0);
        double f_next = f_curr;
        for (int ls = 0; ls < 20; ++ls) {
            for (size_t d = 0; d < dim; ++d) {
                x_next[d] = x[d] + step_size * dir[d];
            }
            clampUnitPoint(x_next);
            std::vector<double> delta(dim, 0.0);
            for (size_t d = 0; d < dim; ++d) {
                delta[d] = x_next[d] - x[d];
            }
            const double slope = dotProduct(grad, delta);
            f_next = objective_fn(x_next);
            if (f_next <= f_curr + config.armijo_c1 * slope) {
                accepted = true;
                break;
            }
            step_size *= 0.5;
        }
        if (!accepted) {
            break;
        }
        const std::vector<double> grad_next = finiteDifferenceGradientBox(x_next, objective_fn, config.fd_step);
        std::vector<double> s(dim, 0.0);
        std::vector<double> y(dim, 0.0);
        for (size_t d = 0; d < dim; ++d) {
            s[d] = x_next[d] - x[d];
            y[d] = grad_next[d] - grad[d];
        }
        const double sy = dotProduct(s, y);
        const double s_norm = std::sqrt(dotProduct(s, s));
        const double y_norm = std::sqrt(dotProduct(y, y));
        if (sy > 1e-10 * s_norm * y_norm) {
            if (static_cast<int>(s_history.size()) >= config.history_size) {
                s_history.erase(s_history.begin());
                y_history.erase(y_history.begin());
                rho_history.erase(rho_history.begin());
            }
            s_history.push_back(s);
            y_history.push_back(y);
            rho_history.push_back(1.0 / sy);
        }
        x = x_next;
        f_curr = f_next;
        grad = grad_next;
    }
    return x;
}
#endif