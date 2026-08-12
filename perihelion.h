#ifndef PERIHELION
#define PERIHELION
#include <vector>
#include <random>
#include <cmath>
#include <helpers.h>

double quotient_rule(double a, double b, double da, double db) {
    if (b == 0) return 0.0; // avoid division by zero
    return (da * b - a * db) / (b * b);
}

int nChooseK(int n, int k) {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    k = std::min(k, n - k); // take advantage of symmetry
    int c = 1;
    for (int i = 0; i < k; ++i) {
        c = c * (n - i) / (i + 1);
    }
    return c;
}



// Perihelion is an optimizer that estimates diagonal derivatives using Least Squares.
// It then applies a Newton step based on these estimates.
// computes perihleion for one param only
double perihelion_diagonal_one(double current_guess, const std::vector<double>& history, const std::vector<double>& costs, int history_size, int degree=2) {
    degree = std::max(degree, 2); 
    std::vector<double> coefficients = solvePolynomialLeastSquares(history, costs, history_size, degree+1);
    std::vector<std::vector<double>> derivatives(degree+1, std::vector<double>(degree+1, 0.0));
    for (int i = 0; i <= degree; ++i) {
        for (int j = 0; j <= degree; ++j) {
            if (j < i) {
                derivatives[i][j] = 0.0;
                continue;
            }
            derivatives[i][j] = (coefficients[j] * std::pow(current_guess, j - i) * std::tgamma(j + 1) / std::tgamma(j - i + 1));
        }
        
    }
    std::vector<double> sums(degree+1, 0.0);
    for (int i = 0; i <= degree; ++i) {
        for (int j = 0; j <= degree; ++j) {
            sums[i] += derivatives[i][j];
        }
    }

    // arbitrary householder: x_new - d* (1/f)^(d-1)*x_old/(1/f)^d*x_old
    // leibniz rule defines g(x)*f(x) = 1 (g(x) = 1/f(x))
    std::vector<double> g_x(degree, 0.0);
    g_x[0] = 1.0 / sums[1];
    //leibniz: g(d) = -1/f*sum_{d-1 k=0} (d choose k) * g(k)*f(d-k)
    for (int i = 1; i < degree; ++i) {
        for (int j = 0; j < i; ++j) {
            g_x[i] -= g_x[0] * nChooseK(i, j) * g_x[j] * sums[i+1-j];
        }
    }
    // max for damping factor
    return current_guess + ((degree-1) * g_x[degree-2] / (std::max(abs(g_x[degree-1]), 1e-4) * std::signbit(g_x[degree-1]))) * current_guess;
    
}

std::vector<double> perihelion_diagonal_all(const std::vector<double>& current_guess, const std::vector<std::vector<double>>& history, const std::vector<std::vector<double>>& costs, int history_size, int degree=2) {
    std::vector<double> new_guess(current_guess.size(), 0.0);
    for (size_t i = 0; i < current_guess.size(); ++i) {
        std::vector<double> param_history(history_size, 0.0);
        std::vector<double> param_costs(history_size, 0.0);
        for (int j = 0; j < history_size; ++j) {
            param_history[j] = history[j][i];
            param_costs[j] = costs[j][i];
        }
        new_guess[i] = perihelion_diagonal_one(current_guess[i], param_history, param_costs, history_size, degree);
    }
    return new_guess;
}

// TODO: make cross term Perihelion
// Cross terms may  more noisy and make more inaccuracy
// I also dont wanna deal with the math