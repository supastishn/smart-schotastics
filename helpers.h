#ifndef HELPERS_IMPORTED
#define HELPERS_IMPORTED
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
constexpr double kPi = 3.14159265358979323846;
struct ParameterBound {
    double lower;
    double upper;
    bool log_scale;
};
struct ZScoreStats {
    double mean;
    double std_dev;
};
struct CholeskyFactor {
    std::vector<double> lower;
    std::vector<double> diagonal;
    int size;
};
inline double normalPDF(double x) {
    return (1.0 / std::sqrt(2.0 * kPi)) * std::exp(-0.5 * x * x);
}
inline double normalCDF(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}
inline double squaredEuclideanDistance(const std::vector<double>& points1, const std::vector<double>& points2) {
    double sum = 0.0;
    const size_t n = std::min(points1.size(), points2.size());
    for (size_t i = 0; i < n; ++i) {
        const double diff = points1[i] - points2[i];
        sum += diff * diff;
    }
    return sum;
}
inline double nDimensionalDistance(const std::vector<double>& points1, const std::vector<double>& points2) {
    return std::sqrt(squaredEuclideanDistance(points1, points2));
}
inline void clampUnitPoint(std::vector<double>& point) {
    for (double& val : point) {
        val = std::max(0.0, std::min(1.0, val));
    }
}
inline std::vector<double> normalizeInput(const std::vector<double>& rawPoint, const std::vector<ParameterBound>& bounds) {
    if (bounds.empty()) {
        return rawPoint;
    }
    std::vector<double> normalized(bounds.size(), 0.0);
    for (size_t i = 0; i < bounds.size(); ++i) {
        const double rawVal = (i < rawPoint.size()) ? rawPoint[i] : bounds[i].lower;
        double lo = bounds[i].lower;
        double hi = bounds[i].upper;
        double val = rawVal;
        if (bounds[i].log_scale) {
            const double safeLower = std::max(1e-12, lo);
            const double safeUpper = std::max(safeLower * (1.0 + 1e-12), hi);
            const double safeVal = std::max(safeLower, std::min(safeUpper, rawVal));
            lo = std::log(safeLower);
            hi = std::log(safeUpper);
            val = std::log(safeVal);
        }
        const double span = hi - lo;
        const double u = (span > 1e-12) ? (val - lo) / span : 0.5;
        normalized[i] = std::max(0.0, std::min(1.0, u));
    }
    return normalized;
}
inline std::vector<double> denormalizeInput(const std::vector<double>& unitPoint, const std::vector<ParameterBound>& bounds) {
    if (bounds.empty()) {
        return unitPoint;
    }
    std::vector<double> raw(bounds.size(), 0.0);
    for (size_t i = 0; i < bounds.size(); ++i) {
        const double u = std::max(0.0, std::min(1.0, (i < unitPoint.size()) ? unitPoint[i] : 0.5));
        if (bounds[i].log_scale) {
            const double safeLower = std::max(1e-12, bounds[i].lower);
            const double safeUpper = std::max(safeLower * (1.0 + 1e-12), bounds[i].upper);
            const double lo = std::log(safeLower);
            const double hi = std::log(safeUpper);
            raw[i] = std::exp(lo + u * (hi - lo));
        } else {
            raw[i] = bounds[i].lower + u * (bounds[i].upper - bounds[i].lower);
        }
    }
    return raw;
}
inline ZScoreStats computeZScoreStats(const std::vector<double>& values) {
    if (values.empty()) {
        return {0.0, 1.0};
    }
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    const double mean = sum / static_cast<double>(values.size());
    double sqSum = 0.0;
    for (double v : values) {
        const double diff = v - mean;
        sqSum += diff * diff;
    }
    double stdDev = std::sqrt(sqSum / static_cast<double>(values.size()));
    if (stdDev < 1e-12) {
        stdDev = 1.0;
    }
    return {mean, stdDev};
}
inline double normalizeSingleCostZScore(double cost, const ZScoreStats& stats) {
    return (cost - stats.mean) / stats.std_dev;
}
inline std::vector<double> normalizeCostsZScore(const std::vector<double>& costs, const ZScoreStats& stats) {
    std::vector<double> normalized(costs.size(), 0.0);
    for (size_t i = 0; i < costs.size(); ++i) {
        normalized[i] = normalizeSingleCostZScore(costs[i], stats);
    }
    return normalized;
}
inline std::vector<double> sumAllProductsOfTuplesUpTo(const std::vector<double>& arr, int max_degree) {
    const int n = static_cast<int>(arr.size());
    const int effective_max = std::min(max_degree, n);
    std::vector<std::vector<double>> dp(effective_max + 1, std::vector<double>(n + 1, 0.0));
    for (int i = 0; i <= n; ++i) {
        dp[0][i] = 1.0;
    }
    for (int k = 1; k <= effective_max; ++k) {
        for (int i = n - 1; i >= 0; --i) {
            dp[k][i] = dp[k][i + 1] + arr[i] * dp[k - 1][i + 1];
        }
    }
    std::vector<double> result(max_degree, 0.0);
    for (int k = 1; k <= effective_max; ++k) {
        result[k - 1] = dp[k][0];
    }
    return result;
}
inline double getTupleProduct(const std::vector<double>& sample, int degree) {
    if (degree <= 0) {
        return 1.0;
    }
    std::vector<double> res = sumAllProductsOfTuplesUpTo(sample, degree);
    if (res.empty() || static_cast<int>(res.size()) < degree) {
        return 0.0;
    }
    return res[degree - 1];
}
inline CholeskyFactor computeCholeskyFactor(const std::vector<double>& matrix, int n, double jitter = 1e-8) {
    CholeskyFactor factor;
    factor.size = n;
    if (n <= 0) {
        return factor;
    }
    factor.lower.assign(n * n, 0.0);
    factor.diagonal.assign(n, 0.0);
    const double eps = 1e-9;
    for (int col = 0; col < n; ++col) {
        for (int row = col; row < n; ++row) {
            double sum = 0.0;
            for (int i = 0; i < col; ++i) {
                sum += factor.lower[row * n + i] * factor.lower[col * n + i];
            }
            if (row == col) {
                const double diagVal = matrix[col * n + col] + jitter - sum;
                const double root = std::sqrt(std::max(eps, diagVal));
                factor.lower[col * n + col] = root;
                factor.diagonal[col] = root;
            } else {
                factor.lower[row * n + col] = (matrix[row * n + col] - sum) / factor.diagonal[col];
            }
        }
    }
    return factor;
}
inline std::vector<double> forwardSubstituteCholesky(const CholeskyFactor& factor, const std::vector<double>& rhs) {
    const int n = factor.size;
    std::vector<double> z(n, 0.0);
    for (int i = 0; i < n; ++i) {
        double sum = 0.0;
        for (int j = 0; j < i; ++j) {
            sum += factor.lower[i * n + j] * z[j];
        }
        z[i] = (rhs[i] - sum) / factor.diagonal[i];
    }
    return z;
}
inline std::vector<double> backwardSubstituteCholesky(const CholeskyFactor& factor, const std::vector<double>& z) {
    const int n = factor.size;
    std::vector<double> weights(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double sum = 0.0;
        for (int j = i + 1; j < n; ++j) {
            sum += factor.lower[j * n + i] * weights[j];
        }
        weights[i] = (z[i] - sum) / factor.diagonal[i];
    }
    return weights;
}
inline std::vector<double> solveWithCholesky(const CholeskyFactor& factor, const std::vector<double>& rhs) {
    if (factor.size <= 0) {
        return {};
    }
    const std::vector<double> z = forwardSubstituteCholesky(factor, rhs);
    return backwardSubstituteCholesky(factor, z);
}
inline std::vector<double> choleskyDecomposition(
    const std::vector<double>& similarityMatrix,
    const std::vector<double>& costs,
    int num_samples,
    double jitter = 1e-8
) {
    if (num_samples <= 0) {
        return {};
    }
    const CholeskyFactor factor = computeCholeskyFactor(similarityMatrix, num_samples, jitter);
    return solveWithCholesky(factor, costs);
}
inline void generateExponents(int index, int current_sum, int max_degree, std::vector<int>& current, std::vector<std::vector<int>>& result) {
    if (index == static_cast<int>(current.size())) {
        result.push_back(current);
        return;
    }
    for (int p = 0; current_sum + p <= max_degree; ++p) {
        current[index] = p;
        generateExponents(index + 1, current_sum + p, max_degree, current, result);
    }
}
inline double evaluateMonomial(const std::vector<double>& sample, const std::vector<int>& exponents) {
    double val = 1.0;
    const size_t dims = std::min(sample.size(), exponents.size());
    for (size_t i = 0; i < dims; ++i) {
        for (int e = 0; e < exponents[i]; ++e) {
            val *= sample[i];
        }
    }
    return val;
}
struct PolynomialFitResult {
    std::vector<double> coefficients;
    CholeskyFactor designFactor;
    std::vector<std::vector<int>> exponentsList;
};
inline PolynomialFitResult fitPolynomialLeastSquares(
    const std::vector<double>& selectedSamples,
    const std::vector<double>& selectedCosts,
    int trainingDim,
    int degree,
    double ridge = 1e-6
) {
    PolynomialFitResult result;
    const int k_points = static_cast<int>(selectedCosts.size());
    std::vector<int> current(std::max(0, trainingDim), 0);
    if (trainingDim > 0) {
        generateExponents(0, 0, degree, current, result.exponentsList);
    }
    const int num_coefficients = static_cast<int>(result.exponentsList.size());
    if (k_points <= 0 || trainingDim <= 0 || num_coefficients <= 0) {
        result.coefficients.assign(std::max(0, num_coefficients), 0.0);
        result.designFactor.size = 0;
        return result;
    }
    std::vector<double> evaluations(k_points * num_coefficients, 0.0);
    for (int i = 0; i < k_points; ++i) {
        std::vector<double> sample(
            selectedSamples.begin() + static_cast<std::ptrdiff_t>(i) * trainingDim,
            selectedSamples.begin() + static_cast<std::ptrdiff_t>(i + 1) * trainingDim
        );
        for (int j = 0; j < num_coefficients; ++j) {
            evaluations[i * num_coefficients + j] = evaluateMonomial(sample, result.exponentsList[j]);
        }
    }
    std::vector<double> leastSquaresMatrix(num_coefficients * num_coefficients, 0.0);
    std::vector<double> leastSquaresResults(num_coefficients, 0.0);
    for (int row = 0; row < num_coefficients; ++row) {
        for (int col = 0; col <= row; ++col) {
            double sum = 0.0;
            for (int i = 0; i < k_points; ++i) {
                sum += evaluations[i * num_coefficients + col] * evaluations[i * num_coefficients + row];
            }
            leastSquaresMatrix[row * num_coefficients + col] = sum;
            leastSquaresMatrix[col * num_coefficients + row] = sum;
        }
        double resSum = 0.0;
        for (int i = 0; i < k_points; ++i) {
            resSum += selectedCosts[i] * evaluations[i * num_coefficients + row];
        }
        leastSquaresResults[row] = resSum;
    }
    result.designFactor = computeCholeskyFactor(leastSquaresMatrix, num_coefficients, ridge);
    result.coefficients = solveWithCholesky(result.designFactor, leastSquaresResults);
    return result;
}
inline std::vector<double> solvePolynomialLeastSquares(
    const std::vector<double>& selectedSamples,
    const std::vector<double>& selectedCosts,
    int trainingDim,
    int degree,
    double ridge = 1e-6
) {
    return fitPolynomialLeastSquares(selectedSamples, selectedCosts, trainingDim, degree, ridge).coefficients;
}
#endif