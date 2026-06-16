#ifndef HELPERS_IMPORTED
#define HELPERS_IMPORTED

#include <vector>
#include <cmath>
#include <algorithm>
#include <utility>

double nDimensionalDistance(const std::vector<double>& points1, const std::vector<double>& points2) {
    double sum = 0.0;
    for (size_t i = 0; i < points1.size(); ++i) {
        double diff = points1[i] - points2[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

std::vector<double> sumAllProductsOfTuplesUpTo(const std::vector<double>& arr, int max_degree) {
    int n = arr.size();
    int effective_max = std::min(max_degree, n);
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
    if (degree <= 0) return 1.0;
    std::vector<double> res = sumAllProductsOfTuplesUpTo(sample, degree);
    if (res.empty() || (int)res.size() < degree) return 0.0;
    return res[degree - 1];
}

std::vector<double> choleskyDecomposition(const std::vector<double>& similarityMatrix, const std::vector<double>& costs, int num_samples) {
    std::vector<double> weights(num_samples, 0.0);
    if (num_samples <= 0) {
        return weights;
    }
    std::vector<double> choleskyDP(num_samples * num_samples, 0.0);
    std::vector<double> diagonal(num_samples, 0.0);
    const double eps = 1e-9;
    for (int col = 0; col < num_samples; ++col) {
        for (int row = col; row < num_samples; ++row) {
            double sum = 0.0;
            if (row == col) {
                for (int i = 0; i < row; ++i) {
                    sum += choleskyDP[row * num_samples + i] * choleskyDP[row * num_samples + i];
                }
                choleskyDP[col * num_samples + col] = std::sqrt(std::max(eps, similarityMatrix[col * num_samples + col] - sum));
                diagonal[col] = choleskyDP[col * num_samples + col];
            } else {
                for (int i = 0; i < col; ++i) {
                    sum += choleskyDP[row * num_samples + i] * choleskyDP[col * num_samples + i];
                }
                choleskyDP[row * num_samples + col] = (similarityMatrix[row * num_samples + col] - sum) / diagonal[col];
            }
        }
    }
    std::vector<double> z(num_samples, 0.0);
    for (int i = 0; i < num_samples; ++i) {
        double sum = 0.0;
        for (int j = 0; j < i; ++j) {
            sum += choleskyDP[i * num_samples + j] * z[j];
        }
        z[i] = (costs[i] - sum) / diagonal[i];
    }
    for (int i = num_samples - 1; i >= 0; --i) {
        double sum = 0.0;
        for (int j = i + 1; j < num_samples; ++j) {
            sum += choleskyDP[j * num_samples + i] * weights[j];
        }
        weights[i] = (z[i] - sum) / diagonal[i];
    }
    return weights;
}

std::vector<double> solvePolynomialLeastSquares(
    const std::vector<double>& selectedSamples,
    const std::vector<double>& selectedCosts,
    int trainingDim,
    int degree
) {
    int k_points = static_cast<int>(selectedCosts.size());
    int num_coefficients = degree + 1;
    std::vector<double> leastSquaresMatrix(num_coefficients * num_coefficients, 0.0);
    std::vector<double> leastSquaresResults(num_coefficients, 0.0);
    for (int row = 0; row < num_coefficients; ++row) {
        for (int col = 0; col <= row; ++col) {
            double sum = 0.0;
            for (int i = 0; i < k_points; ++i) {
                std::vector<double> sample(selectedSamples.begin() + i * trainingDim, selectedSamples.begin() + (i + 1) * trainingDim);
                sum += getTupleProduct(sample, col) * getTupleProduct(sample, row);
            }
            leastSquaresMatrix[row * num_coefficients + col] = sum;
            leastSquaresMatrix[col * num_coefficients + row] = sum;
        }
        double resSum = 0.0;
        for (int i = 0; i < k_points; ++i) {
            std::vector<double> sample(selectedSamples.begin() + i * trainingDim, selectedSamples.begin() + (i + 1) * trainingDim);
            resSum += selectedCosts[i] * getTupleProduct(sample, row);
        }
        leastSquaresResults[row] = resSum;
    }
    return choleskyDecomposition(leastSquaresMatrix, leastSquaresResults, num_coefficients);
}
#endif