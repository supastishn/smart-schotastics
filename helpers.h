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
#endif