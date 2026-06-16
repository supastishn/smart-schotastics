#ifndef ACQUISITIONS
#define ACQUISITIONS

#include "bayesian.h"
#include "council.h"
#include "helpers.h"
#include "perihelion.h"
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
std::vector<double> expectedImprovement(const SurrogateModel& surrogate, const std::vector<double>& currentBest, const std::vector<double>& points const std::vector<double>& costs) {
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
// same as standard EI but uses perihelion as opposed to random:
// 1. uses perihelion on a subset of points then runs a Newton step
// 2. uses perihelion on GP in order to find the best
std::vector<double> expectedImprovementPerihelion(const SurrogateModel& surrogate, const std::vector<double>& currentBest, const std::vector<double>& points, const std::vector<double>& costs) {
    // eval 50 random points with GP
    int dimension = (int)currentBest.size();
    constexpr int numRandomPoints = 50;
    std::vector<std::vector<double>> randomPoints(numRandomPoints, std::vector<double>(dimension));
    std::vector<GaussianPrediction> predictions(numRandomPoints, 0.0);
    for (int i = 0; i < numRandomPoints; ++i) {
        for (size_t d = 0; d < dimension; ++d) {
            randomPoints[i][d] = static_cast<double>(rand()) / RAND_MAX; // Random value between 0 and 1
        }
        GaussianPrediction prediction = surrogate.predict(randomPoints[i]);
        predictions[i] = prediction;
    }
    std::vector<std::vector<double>> perPointHistory(dimension, std::vector<double>(points.size() / dimension, 0.0));
    // this is inefficient as points often share same cost. i'll fix this later
    std::vector<std::vector<double>> perPointCosts(dimension, std::vector<double>(points.size() / dimension, 0.0));
    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = 0; j < points.size() / dimension; ++j) {
            perPointHistory[i][j] = points[j * dimension + i];
            perPointCosts[i][j] = costs[j];
        }
    }
    std::vector<double> perihelionPoint = perihelion_diagonal_all(currentBest, perPointHistory, perPointCosts, surrogate);
    GaussianPrediction perihelionPrediction = surrogate.predict(perihelionPoint);
    randomPoints.push_back(perihelionPoint);
    predictions.push_back(perihelionPrediction);

    // now we repeatedly eval GP on perihelion for best point
    std::vector<std::vector<double>> gaussianHistory(dimension, std::vector<double>(predictions.size(), 0.0));
    std::vector<std::vector<double>> gaussianCosts(dimension, std::vector<double>(predictions.size(), 0.0));
    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = 0; j < predictions.size(); ++j) {
            gaussianHistory[i][j] = randomPoints[j][i];
            gaussianCosts[i][j] = predictions[j].mean;  
        }
    }  
    constexpr int perihelion_iters = 30;
    constexpr int perihelion_random_start = 5;
    int orig_size = (int)randomPoints.size();
    for (int i = 0; i < perihelion_random_start; ++i) {
        std::vector<double> randomStart(dimension);
        
        randomStart = randomPoints[rand() % orig_size];
        for (int j = 0; j < perihelion_iters; ++j) {
            std::vector<double> newPoint = perihelion_diagonal_all(randomStart, gaussianHistory, gaussianCosts, surrogate);
            GaussianPrediction newPrediction = surrogate.predict(newPoint);
            randomStart = newPoint;
            for (size_t d = 0; d < dimension; ++d) {
                gaussianHistory[d].push_back(newPoint[d]);
                gaussianCosts[d].push_back(newPrediction.mean);
            }
        }
        

    }
    // ei bestpoint of ALL
    std::vector<double> bestPoint = randomPoints[0];
    float bestEI = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < gaussianHistory[0].size(); ++i) {
        std::vector<double> point(dimension);
        for (size_t d = 0; d < dimension; ++d) {
            point[d] = gaussianHistory[d][i];
        }
        GaussianPrediction prediction = surrogate.predict(point);
        double improvement = currentBest[0] - prediction.mean; // Assuming minimization
        double ei = improvement * normCDF(improvement / sqrt(prediction.variance)) + sqrt(prediction.variance) * normPDF(improvement / sqrt(prediction.variance));
        if (ei > bestEI) {
            bestEI = ei;
            bestPoint = point;
        }
    }
    return bestPoint;
}
#endif