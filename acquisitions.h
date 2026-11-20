#ifndef ACQUISITIONS_IMPORTED
#define ACQUISITIONS_IMPORTED
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <random>
#include <vector>
#include "helpers.h"
#include "lbfgs.h"
#include "perihelion.h"
#include "surrogates.h"
inline double normPDF(double x) {
    return normalPDF(x);
}
inline double normCDF(double x) {
    return normalCDF(x);
}
inline double computeExpectedImprovement(const GaussianPrediction& prediction, double bestCost) {
    const double stdDev = std::sqrt(std::max(prediction.variance, 1e-12));
    const double improvement = bestCost - prediction.mean;
    const double z = improvement / stdDev;
    return std::max(0.0, improvement * normalCDF(z) + stdDev * normalPDF(z));
}
inline double computeLogExpectedImprovement(const GaussianPrediction& prediction, double bestCost) {
    const double stdDev = std::sqrt(std::max(prediction.variance, 1e-12));
    const double z = (bestCost - prediction.mean) / stdDev;
    double logH = 0.0;
    if (z >= -10.0) {
        const double h = std::max(1e-300, z * normalCDF(z) + normalPDF(z));
        logH = std::log(h);
    } else {
        const double t = -z;
        const double t2 = t * t;
        const double t4 = t2 * t2;
        const double t6 = t4 * t2;
        const double t8 = t4 * t4;
        const double poly = 1.0 / t2 - 3.0 / t4 + 15.0 / t6 - 105.0 / t8;
        const double logPhi = -0.5 * std::log(2.0 * kPi) - 0.5 * t2;
        logH = logPhi + std::log(std::max(1e-300, poly));
    }
    return std::log(stdDev) + logH;
}
inline double minNormalizedDistanceToSamples(
    const std::vector<double>& candidate,
    const std::vector<double>& points,
    size_t dimension
) {
    if (dimension == 0 || points.size() < dimension) {
        return std::numeric_limits<double>::infinity();
    }
    const size_t numPoints = points.size() / dimension;
    double minSqDist = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < numPoints; ++i) {
        double sqDist = 0.0;
        for (size_t d = 0; d < dimension; ++d) {
            const double diff = candidate[d] - points[i * dimension + d];
            sqDist += diff * diff;
        }
        if (sqDist < minSqDist) {
            minSqDist = sqDist;
        }
    }
    return std::sqrt(minSqDist / static_cast<double>(dimension));
}
inline double computePenalizedLogExpectedImprovement(
    const SurrogateModel& surrogate,
    const std::vector<double>& candidate,
    double bestCost,
    const std::vector<double>& points,
    size_t dimension,
    double duplicateRadius = 0.025
) {
    double logEI = computeLogExpectedImprovement(surrogate.predict(candidate), bestCost);
    const double rmsDist = minNormalizedDistanceToSamples(candidate, points, dimension);
    if (rmsDist < duplicateRadius) {
        const double proximity = 1.0 - (rmsDist / duplicateRadius);
        logEI -= 250.0 * proximity * proximity;
    }
    return logEI;
}
inline std::vector<double> expectedImprovement(
    const SurrogateModel& surrogate,
    const std::vector<double>& currentBest,
    const std::vector<double>& points,
    const std::vector<double>& costs
) {
    const size_t dimension = std::max<size_t>(1, currentBest.size());
    const double bestCost = costs.empty() ? 0.0 : *std::min_element(costs.begin(), costs.end());
    std::vector<double> bestPoint(dimension, 0.5);
    double bestLogEI = -std::numeric_limits<double>::infinity();
    if (!currentBest.empty()) {
        std::vector<double> clampedBest = currentBest;
        clampUnitPoint(clampedBest);
        const double logEI = computePenalizedLogExpectedImprovement(surrogate, clampedBest, bestCost, points, dimension);
        if (logEI > bestLogEI) {
            bestLogEI = logEI;
            bestPoint = clampedBest;
        }
    }
    for (int i = 0; i < 100; ++i) {
        std::vector<double> randomPoint(dimension);
        for (size_t d = 0; d < dimension; ++d) {
            randomPoint[d] = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
        }
        const double logEI = computePenalizedLogExpectedImprovement(surrogate, randomPoint, bestCost, points, dimension);
        if (logEI > bestLogEI) {
            bestLogEI = logEI;
            bestPoint = randomPoint;
        }
    }
    return bestPoint;
}
inline std::vector<double> expectedImprovementPerihelion(
    const SurrogateModel& surrogate,
    const std::vector<double>& currentBest,
    const std::vector<double>& points,
    const std::vector<double>& costs,
    PerihelionMethod method = PerihelionMethod::HALLEY
) {
    const size_t dimension = std::max<size_t>(1, currentBest.size());
    const double bestCost = costs.empty() ? 0.0 : *std::min_element(costs.begin(), costs.end());
    constexpr int numRandomPoints = 50;
    std::vector<std::vector<double>> candidatePoints(numRandomPoints, std::vector<double>(dimension, 0.5));
    std::vector<double> candidateNegLogEI(numRandomPoints, 0.0);
    std::vector<double> bestPoint = candidatePoints[0];
    double bestLogEI = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < numRandomPoints; ++i) {
        for (size_t d = 0; d < dimension; ++d) {
            candidatePoints[i][d] = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
        }
        const double logEI = computePenalizedLogExpectedImprovement(surrogate, candidatePoints[i], bestCost, points, dimension);
        candidateNegLogEI[i] = -logEI;
        if (logEI > bestLogEI) {
            bestLogEI = logEI;
            bestPoint = candidatePoints[i];
        }
    }
    const size_t numRealPoints = std::min(costs.size(), points.size() / dimension);
    if (numRealPoints >= 2 && !currentBest.empty()) {
        std::vector<std::vector<double>> realPointHistory(numRealPoints, std::vector<double>(dimension, 0.0));
        for (size_t j = 0; j < numRealPoints; ++j) {
            for (size_t d = 0; d < dimension; ++d) {
                realPointHistory[j][d] = points[j * dimension + d];
            }
        }
        std::vector<double> realCosts(costs.begin(), costs.begin() + static_cast<std::ptrdiff_t>(numRealPoints));
        std::vector<double> perihelionPoint = perihelion_diagonal_all(currentBest, realPointHistory, realCosts, method);
        clampUnitPoint(perihelionPoint);
        const double logEI = computePenalizedLogExpectedImprovement(surrogate, perihelionPoint, bestCost, points, dimension);
        candidatePoints.push_back(perihelionPoint);
        candidateNegLogEI.push_back(-logEI);
        if (logEI > bestLogEI) {
            bestLogEI = logEI;
            bestPoint = perihelionPoint;
        }
    }
    constexpr int perihelion_iters = 30;
    constexpr int perihelion_random_start = 5;
    const size_t origSize = candidatePoints.size();
    std::vector<size_t> startIndices(origSize);
    std::iota(startIndices.begin(), startIndices.end(), 0);
    std::mt19937 shuffler(static_cast<unsigned int>(std::rand()));
    std::shuffle(startIndices.begin(), startIndices.end(), shuffler);
    const size_t numStarts = std::min<size_t>(perihelion_random_start, origSize);
    for (size_t s = 0; s < numStarts; ++s) {
        std::vector<std::vector<double>> trajPoints = candidatePoints;
        std::vector<double> trajNegLogEI = candidateNegLogEI;
        std::vector<double> current = candidatePoints[startIndices[s]];
        for (int j = 0; j < perihelion_iters; ++j) {
            std::vector<double> newPoint = perihelion_diagonal_all(current, trajPoints, trajNegLogEI, method);
            clampUnitPoint(newPoint);
            const double logEI = computePenalizedLogExpectedImprovement(surrogate, newPoint, bestCost, points, dimension);
            if (logEI > bestLogEI) {
                bestLogEI = logEI;
                bestPoint = newPoint;
            }
            trajPoints.push_back(newPoint);
            trajNegLogEI.push_back(-logEI);
            if (nDimensionalDistance(newPoint, current) < 1e-7) {
                break;
            }
            current = newPoint;
        }
    }
    return bestPoint;
}
inline std::vector<double> expectedImprovementPerihelionDefault(
    const SurrogateModel& surrogate,
    const std::vector<double>& currentBest,
    const std::vector<double>& points,
    const std::vector<double>& costs
) {
    return expectedImprovementPerihelion(surrogate, currentBest, points, costs, PerihelionMethod::HALLEY);
}
inline std::vector<double> expectedImprovementLBFGS(
    const SurrogateModel& surrogate,
    const std::vector<double>& currentBest,
    const std::vector<double>& points,
    const std::vector<double>& costs
) {
    const size_t dimension = std::max<size_t>(1, currentBest.size());
    const double bestCost = costs.empty() ? 0.0 : *std::min_element(costs.begin(), costs.end());
    auto negLogEI = [&](const std::vector<double>& x) -> double {
        return -computePenalizedLogExpectedImprovement(surrogate, x, bestCost, points, dimension);
    };
    constexpr int numSeedSamples = 64;
    constexpr int numRestarts = 8;
    std::vector<std::pair<double, std::vector<double>>> scoredSeeds;
    scoredSeeds.reserve(numSeedSamples + 1);
    if (!currentBest.empty()) {
        std::vector<double> clampedBest = currentBest;
        clampUnitPoint(clampedBest);
        scoredSeeds.push_back({negLogEI(clampedBest), clampedBest});
    }
    for (int i = 0; i < numSeedSamples; ++i) {
        std::vector<double> candidate(dimension);
        for (size_t d = 0; d < dimension; ++d) {
            candidate[d] = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
        }
        scoredSeeds.push_back({negLogEI(candidate), candidate});
    }
    std::sort(
        scoredSeeds.begin(),
        scoredSeeds.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; }
    );
    std::vector<double> bestPoint = scoredSeeds.front().second;
    double bestNegLogEI = scoredSeeds.front().first;
    const LbfgsConfig config;
    const int restarts = std::min<int>(numRestarts, static_cast<int>(scoredSeeds.size()));
    for (int r = 0; r < restarts; ++r) {
        std::vector<double> optimized = lbfgsMinimizeBox(scoredSeeds[r].second, negLogEI, config);
        clampUnitPoint(optimized);
        const double val = negLogEI(optimized);
        if (val < bestNegLogEI) {
            bestNegLogEI = val;
            bestPoint = optimized;
        }
    }
    return bestPoint;
}
#endif