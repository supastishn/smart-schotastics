#ifndef ACQUISITIONS
#define ACQUISITIONS

#include "bayesian.h"
#include "council.h"
#include "helpers.h"
#include "surrogates.h"
#include <cmath>
#include <vector>  
double normPDF(double x) {
    return (1.0 / sqrt(2.0 * M_PI)) * exp(-0.5 * x * x);
}
double normCDF(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}
// EI will use random search to find best Expected Improvement overall
std::vector<double> expectedImprovement(const SurrogateModel& surrogate, const std::vector<double>& currentBest) {
    std::vector<double> bestPoint(currentBest.size(), 0.0);
    double bestEI = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < 100; ++i) {
        std::vector<double> randomPoint(currentBest.size());
        for (size_t d = 0; d < currentBest.size(); ++d) {
            randomPoint[d] = static_cast<double>(rand()) / RAND_MAX; // Random value between 0 and 1
        }
        GaussianPrediction prediction = surrogate.predict(randomPoint);
        double improvement = currentBest[0] - prediction.mean; // Assuming minimization
        double ei = improvement * normCDF(improvement / sqrt(prediction.variance)) + sqrt(prediction.variance) * normPDF(improvement / sqrt(prediction.variance));
        if (ei > bestEI) {
            bestEI = ei;
            bestPoint = randomPoint;
        }
    }
    return bestPoint;
}

#endif