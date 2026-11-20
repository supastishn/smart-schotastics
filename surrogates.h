#ifndef SURROGATES_IMPORTED
#define SURROGATES_IMPORTED
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "helpers.h"
struct GaussianPrediction {
    double mean;
    double variance;
};
class SurrogateModel {
public:
    virtual ~SurrogateModel() = default;
    virtual void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) = 0;
    virtual GaussianPrediction predict(const std::vector<double>& question) const = 0;
};
class Kernel {
public:
    virtual ~Kernel() = default;
    virtual double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const = 0;
};
class RBFKernel : public Kernel {
private:
    double lengthScale;
public:
    explicit RBFKernel(double lengthScale) : lengthScale(std::max(1e-6, lengthScale)) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        const double distSq = squaredEuclideanDistance(x1, x2);
        return std::exp(-distSq / (2.0 * lengthScale * lengthScale));
    }
};
class Matern12Kernel : public Kernel {
private:
    double lengthScale;
public:
    explicit Matern12Kernel(double lengthScale) : lengthScale(std::max(1e-6, lengthScale)) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        const double dist = nDimensionalDistance(x1, x2);
        return std::exp(-dist / lengthScale);
    }
};
class Matern32Kernel : public Kernel {
private:
    double lengthScale;
public:
    explicit Matern32Kernel(double lengthScale) : lengthScale(std::max(1e-6, lengthScale)) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        const double s = std::sqrt(3.0) * nDimensionalDistance(x1, x2) / lengthScale;
        return (1.0 + s) * std::exp(-s);
    }
};
class Matern52Kernel : public Kernel {
private:
    double lengthScale;
public:
    explicit Matern52Kernel(double lengthScale) : lengthScale(std::max(1e-6, lengthScale)) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        const double s = std::sqrt(5.0) * nDimensionalDistance(x1, x2) / lengthScale;
        return (1.0 + s + (s * s) / 3.0) * std::exp(-s);
    }
};
class RationalQuadraticKernel : public Kernel {
private:
    double lengthScale;
    double alpha;
public:
    explicit RationalQuadraticKernel(double lengthScale, double alpha = 1.0)
        : lengthScale(std::max(1e-6, lengthScale)), alpha(std::max(1e-3, alpha)) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        const double distSq = squaredEuclideanDistance(x1, x2);
        return std::pow(1.0 + distSq / (2.0 * alpha * lengthScale * lengthScale), -alpha);
    }
};
class GaussianProcess : public SurrogateModel {
private:
    Kernel* kernel;
    std::vector<double> similarityMatrix;
    std::vector<double> weights;
    std::vector<double> trainingSamples;
    CholeskyFactor choleskyFactor;
    double priorVariance;
    double noiseJitter;
    int trainingDim;
    int numSamplesStored;
public:
    explicit GaussianProcess(Kernel* kernel, double noiseJitter = 1e-6)
        : kernel(kernel),
          choleskyFactor{{}, {}, 0},
          priorVariance(1.0),
          noiseJitter(noiseJitter),
          trainingDim(0),
          numSamplesStored(0) {}
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        numSamplesStored = num_samples;
        if (num_samples <= 0 || costs.empty() || !kernel) {
            numSamplesStored = 0;
            return;
        }
        trainingDim = static_cast<int>(samples.size() / static_cast<size_t>(num_samples));
        if (trainingDim <= 0) {
            numSamplesStored = 0;
            return;
        }
        trainingSamples = samples;
        similarityMatrix.assign(num_samples * num_samples, 0.0);
        std::vector<double> xi(trainingDim);
        std::vector<double> xj(trainingDim);
        for (int i = 0; i < num_samples; ++i) {
            for (int d = 0; d < trainingDim; ++d) {
                xi[d] = samples[i * trainingDim + d];
            }
            for (int j = 0; j <= i; ++j) {
                for (int d = 0; d < trainingDim; ++d) {
                    xj[d] = samples[j * trainingDim + d];
                }
                const double kVal = kernel->evaluate(xi, xj);
                similarityMatrix[i * num_samples + j] = kVal;
                similarityMatrix[j * num_samples + i] = kVal;
            }
        }
        choleskyFactor = computeCholeskyFactor(similarityMatrix, num_samples, noiseJitter);
        weights = solveWithCholesky(choleskyFactor, costs);
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        if (numSamplesStored == 0 || !kernel) {
            return {0.0, priorVariance};
        }
        double mean = 0.0;
        std::vector<double> k(numSamplesStored, 0.0);
        std::vector<double> xi(trainingDim);
        for (int i = 0; i < numSamplesStored; ++i) {
            for (int d = 0; d < trainingDim; ++d) {
                xi[d] = trainingSamples[i * trainingDim + d];
            }
            k[i] = kernel->evaluate(question, xi);
            mean += weights[i] * k[i];
        }
        const std::vector<double> z = forwardSubstituteCholesky(choleskyFactor, k);
        double explainedVariance = 0.0;
        for (int i = 0; i < numSamplesStored; ++i) {
            explainedVariance += z[i] * z[i];
        }
        const double prior = kernel->evaluate(question, question);
        const double endVariance = std::max(1e-12, prior - explainedVariance);
        return {mean, endVariance};
    }
};
class NeuralNetwork : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        (void)samples;
        (void)costs;
        (void)num_samples;
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        (void)question;
        return {0.0, 1.0};
    }
};
class PiecewisePolynomialModel : public SurrogateModel {
private:
    int polynomial_degree;
    int points_per_polynomial;
    std::vector<double> trainingSamples;
    std::vector<double> costs;
    int trainingDim;
public:
    PiecewisePolynomialModel(int degree, int points)
        : polynomial_degree(degree), points_per_polynomial(points), trainingDim(0) {}
    void train(const std::vector<double>& samples, const std::vector<double>& costs_in, int num_samples) override {
        (void)num_samples;
        trainingSamples = samples;
        costs = costs_in;
        if (!costs.empty()) {
            trainingDim = static_cast<int>(samples.size() / costs.size());
        } else {
            trainingDim = 0;
        }
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        if (costs.empty() || trainingDim == 0) {
            return {0.0, 1.0};
        }
        std::vector<std::pair<double, int>> distanceIndices(costs.size());
        std::vector<double> samplePoint(trainingDim);
        for (size_t i = 0; i < costs.size(); ++i) {
            for (int d = 0; d < trainingDim; ++d) {
                samplePoint[d] = trainingSamples[i * trainingDim + d];
            }
            distanceIndices[i] = {nDimensionalDistance(samplePoint, question), static_cast<int>(i)};
        }
        const int k_points = std::min(points_per_polynomial, static_cast<int>(costs.size()));
        if (k_points <= 0) {
            return {0.0, 1.0};
        }
        std::partial_sort(distanceIndices.begin(), distanceIndices.begin() + k_points, distanceIndices.end());
        std::vector<double> selectedSamples(k_points * trainingDim);
        std::vector<double> selectedCosts(k_points);
        for (int i = 0; i < k_points; ++i) {
            const int idx = distanceIndices[i].second;
            for (int d = 0; d < trainingDim; ++d) {
                selectedSamples[i * trainingDim + d] = trainingSamples[idx * trainingDim + d];
            }
            selectedCosts[i] = costs[idx];
        }
        const PolynomialFitResult fit = fitPolynomialLeastSquares(selectedSamples, selectedCosts, trainingDim, polynomial_degree);
        const int num_coefficients = static_cast<int>(fit.exponentsList.size());
        std::vector<double> phi(num_coefficients, 0.0);
        double prediction = 0.0;
        for (int i = 0; i < num_coefficients; ++i) {
            phi[i] = evaluateMonomial(question, fit.exponentsList[i]);
            prediction += fit.coefficients[i] * phi[i];
        }
        double noise_variance = 0.0;
        if (k_points > num_coefficients) {
            for (int i = 0; i < k_points; ++i) {
                std::vector<double> sample(selectedSamples.begin() + i * trainingDim, selectedSamples.begin() + (i + 1) * trainingDim);
                double approx = 0.0;
                for (int j = 0; j < num_coefficients; ++j) {
                    approx += fit.coefficients[j] * evaluateMonomial(sample, fit.exponentsList[j]);
                }
                const double error = approx - selectedCosts[i];
                noise_variance += error * error;
            }
            noise_variance /= static_cast<double>(k_points - num_coefficients);
        } else {
            const ZScoreStats stats = computeZScoreStats(selectedCosts);
            noise_variance = stats.std_dev * stats.std_dev;
        }
        noise_variance = std::max(1e-6, noise_variance);
        double leverage = 1.0;
        if (fit.designFactor.size == num_coefficients && num_coefficients > 0) {
            const std::vector<double> z = forwardSubstituteCholesky(fit.designFactor, phi);
            double zSqSum = 0.0;
            for (int i = 0; i < num_coefficients; ++i) {
                zSqSum += z[i] * z[i];
            }
            leverage = std::max(1e-6, zSqSum);
        }
        const double predictive_variance = std::max(1e-6, noise_variance * leverage);
        return {prediction, predictive_variance};
    }
};
class RandomForest : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        (void)samples;
        (void)costs;
        (void)num_samples;
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        (void)question;
        return {0.0, 1.0};
    }
};
#endif