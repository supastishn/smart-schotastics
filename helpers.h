#ifndef HELPERS_IMPORTED
#define HELPERS_IMPORTED

#include <vector>

double nDimensionalDistance(const std::vector<double>& points1, const std::vector<double>& points2) {
    double sum = 0.0;
    for (size_t i = 0; i < points1.size(); ++i) {
	double diff = points1[i] - points2[i];
	sum += diff * diff;
    }
    return sqrt(sum);
}
//finally, a usecase for my CP knowledge
// calcs sum of all products of pairs of size 1 to tuple_size
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

std::vector<double> choleskyDecomposition(const std::vector<double>& similarityMatrix, const std::vector<double>& costs, int num_samples) {
    std::vector<double> chelovskyDP(num_samples * num_samples, 0.0);
    std::vector<double> diagonal(num_samples, 0.0);
    std::vector<double> weights(num_samples, 0.0);
    if (num_samples > 0) {
        weights[0] = sqrt(similarityMatrix[0]);
        for (int col = 0; col < num_samples; ++col) {
            for (int row = col; row < num_samples; ++row) {
                double sum = 0.0;
                if (row == col) {
                    for (int i = 0; i < row; ++i) {
                        sum += chelovskyDP[row * num_samples + i] * chelovskyDP[row * num_samples + i];
                    }
                } else {
                    for (int i = 0; i < col; ++i) {
                        sum += chelovskyDP[row * num_samples + i] * chelovskyDP[col * num_samples + i];
                    }
                }
                if (row == col) {
                    chelovskyDP[col * num_samples + col] = sqrt(similarityMatrix[col * num_samples + col] - sum);
                    diagonal[col] = chelovskyDP[col * num_samples + col];
                } else {
                    chelovskyDP[row * num_samples + col] = (similarityMatrix[row * num_samples + col] - sum) / chelovskyDP[col * num_samples + col];   
                }
            }
        }
    }
    std::vector<double> transformedCosts = costs;
    // forward substitution fir L*z=costs
    std::vector<double> z(num_samples);
    z[0] = costs[0] / diagonal[0];
    for (int col = 0; col < num_samples; ++col) {
    for (int row = 0; row <= col; ++row) {
        transformedCosts[row] -= chelovskyDP[row * num_samples + col] * z[col];
    }  
    z[col] = transformedCosts[col] / diagonal[col];
    }
    std::vector<double> weights(num_samples, 0.0);
    std::vector<double> transformedZs = z;
    weights[num_samples - 1] = z[num_samples - 1] / diagonal[num_samples - 1];
    for (int col = num_samples - 1; col >= 0; --col) {
    for (int row = num_samples - 1; row > col; --row) {
        transformedZs[row] -= chelovskyDP[col * num_samples + row] * weights[row];
    }
    weights[col] = transformedZs[col] / diagonal[col]; 
}
return weights;
}
#endif