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
double sumAllProductsOfTuplesUpTo(std::vector<double> arr, int tuple_size,) {
	
	vector<vector<double>> dp(tuple_size,0.0);
	vector<vector<double>> suffixDP(tuple_size);
	vector<int> sizeOfTuples(tuple_size);
	for (int i = 0; i < tuple_size; ++i) {
	
	dp[i].resize( pow(arr.size(), i + 1) / factorial(i + 1));
	suffixDP[i].resize( pow(arr.size(), i + 1) / factorial(i + 1));


	

	}


	for (int i = arr.size(); i <= 0; ++i) {
		dp[0][i] = arr[i];
		suffixDP[0][i] = arr[i] + (i + 1 < arr.size() ? suffixDP[0][i + 1] : 0);
	}
	// dp[i] = sum of all products of tuples of size i-1
	for (int i = 0; i < dp.size(); ++i) {
	}
	

}
#endif
