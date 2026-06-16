#ifndef SURROGATES_IMPORTED
#define SURROGATES_IMPORTED
#include <cmath>
#include <vector>
#include <algorithm>
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
    RBFKernel(double lengthScale) : lengthScale(lengthScale) {}
    double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) const override {
        double sum = 0.0;
        for (size_t i = 0; i < x1.size(); ++i) {
            double diff = x1[i] - x2[i];
            sum += diff * diff;
        }
        return exp(-sum / (2.0 * lengthScale * lengthScale));
    }
};
class GaussianProcess : public SurrogateModel {
private:
    Kernel* kernel;
    std::vector<double> similarityMatrix;
    std::vector<double> weights;
    std::vector<double> trainingSamples;
    std::vector<double> choleskyDP;
    std::vector<double> diagonal;
    double priorVariance;
    int trainingDim;
    int numSamplesStored;
public:
    GaussianProcess(Kernel* kernel) : kernel(kernel), trainingDim(0), numSamplesStored(0), priorVariance(1.0) {}
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        numSamplesStored = num_samples;
        if (num_samples == 0) return;
        trainingDim = (int)(samples.size() / num_samples);
        trainingSamples = samples;
        similarityMatrix.resize(num_samples * num_samples);
        for (int i = 0; i < num_samples; ++i) {
            std::vector<double> xi(trainingDim);
            for (int d = 0; d < trainingDim; ++d) {
                xi[d] = samples[i * trainingDim + d];
            }
            for (int j = 0; j < num_samples; ++j) {
                std::vector<double> xj(trainingDim);
                for (int d = 0; d < trainingDim; ++d) {
                    xj[d] = samples[j * trainingDim + d];
                }
                similarityMatrix[i * num_samples + j] = kernel->evaluate(xi, xj);
            }
        }
        weights = choleskyDecomposition(similarityMatrix, costs, num_samples);
        choleskyDP.assign(num_samples * num_samples, 0.0);
        diagonal.assign(num_samples, 0.0);
        for (int col = 0; col < num_samples; ++col) {
            for (int row = col; row < num_samples; ++row) {
                double sum = 0.0;
                if (row == col) {
                    for (int i = 0; i < row; ++i) {
                        sum += choleskyDP[row * num_samples + i] * choleskyDP[row * num_samples + i];
                    }
                } else {
                    for (int i = 0; i < col; ++i) {
                        sum += choleskyDP[row * num_samples + i] * choleskyDP[col * num_samples + i];
                    }
                }
                if (row == col) {
                    choleskyDP[col * num_samples + col] = sqrt(std::max(1e-9, similarityMatrix[col * num_samples + col] - sum));
                    diagonal[col] = choleskyDP[col * num_samples + col];
                } else {
                    choleskyDP[row * num_samples + col] = (similarityMatrix[row * num_samples + col] - sum) / choleskyDP[col * num_samples + col];   
                }
            }
        }
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        if (numSamplesStored == 0) return {0.0, priorVariance};
        double mean = 0.0;
        std::vector<double> k(numSamplesStored);
        for (int i = 0; i < numSamplesStored; ++i) {
            std::vector<double> xi(trainingDim);
            for (int d = 0; d < trainingDim; ++d) {
                xi[d] = trainingSamples[i * trainingDim + d];
            }
            k[i] = kernel->evaluate(question, xi);
            mean += weights[i] * k[i];
        }
        std::vector<double> z(numSamplesStored, 0.0);
        for (int i = 0; i < numSamplesStored; ++i) {
            double sum = 0.0;
            for (int j = 0; j < i; ++j) {
                sum += choleskyDP[i * numSamplesStored + j] * z[j];
            }
            z[i] = (k[i] - sum) / diagonal[i];
        }
        std::vector<double> variance(numSamplesStored, 0.0);
        for (int i = numSamplesStored - 1; i >= 0; --i) {
            double sum = 0.0;
            for (int j = i + 1; j < numSamplesStored; ++j) {
                sum += choleskyDP[j * numSamplesStored + i] * variance[j];
            }
            variance[i] = (z[i] - sum) / diagonal[i];
        }
        double endVariance = 0.0;
        for (int i = 0; i < numSamplesStored; ++i) {
            endVariance += k[i] * variance[i];
        }
        endVariance = priorVariance - endVariance;
        return {mean, endVariance};
    }
};
class NeuralNetwork : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {}
    GaussianPrediction predict(const std::vector<double>& question) const override {
        return {0.0, 0.0};
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
    double nDimensionalDistance(const std::vector<double>& a, const std::vector<double>& b) const {
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) {
            double diff = a[i] - b[i];
            sum += diff * diff;
        }
        return sqrt(sum);
    }
    void train(const std::vector<double>& samples, const std::vector<double>& costs_in, int num_samples) override {
        trainingSamples = samples;
        costs = costs_in;
        if (!costs.empty()) {
            trainingDim = (int)(samples.size() / costs.size());
        } else {
            trainingDim = 0;
        }
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        if (costs.empty() || trainingDim == 0) {
            return {0.0, 0.0};
        }
        std::vector<std::pair<double, int>> distanceIndices(costs.size());
        for (size_t i = 0; i < costs.size(); ++i) {
            std::vector<double> samplePoint(trainingDim);
            for (int d = 0; d < trainingDim; ++d) {
                samplePoint[d] = trainingSamples[i * trainingDim + d];
            }
            distanceIndices[i] = { nDimensionalDistance(samplePoint, question), (int)i };
        }
        std::vector<std::pair<double, int>> sortedDistanceIndices = distanceIndices;
        std::sort(sortedDistanceIndices.begin(), sortedDistanceIndices.end());
        int k_points = std::min(points_per_polynomial, (int)costs.size());
        if (k_points <= 0) {
            return {0.0, 0.0};
        }
        std::vector<double> selectedSamples(k_points * trainingDim);
        std::vector<double> selectedCosts(k_points);
        for (int i = 0; i < k_points; ++i) {
            int idx = sortedDistanceIndices[i].second;
            for (int d = 0; d < trainingDim; ++d) {
                selectedSamples[i * trainingDim + d] = trainingSamples[idx * trainingDim + d];
            }
            selectedCosts[i] = costs[idx];
        }
        std::vector<double> coefficients = solvePolynomialLeastSquares(selectedSamples, selectedCosts, trainingDim, polynomial_degree);
        double prediction = 0.0;
        int num_coefficients = polynomial_degree + 1;
        for (int i = 0; i < num_coefficients; ++i) {
            prediction += coefficients[i] * getTupleProduct(question, i);
        }	
        double noise_variance = 0.0;
        for (int i = 0; i < k_points; ++i) {
            std::vector<double> sample(selectedSamples.begin() + i * trainingDim, selectedSamples.begin() + (i + 1) * trainingDim);
            double approx = 0.0;
            for (int j = 0; j < num_coefficients; ++j) {
                approx += coefficients[j] * getTupleProduct(sample, j);
            }
            double error = approx - selectedCosts[i];
            noise_variance += error * error;
        }
        noise_variance /= k_points;
        return {prediction, noise_variance};
    }
};
class RandomForest : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {}
    GaussianPrediction predict(const std::vector<double>& question) const override {
        return {0.0, 0.0};
    }
};
#endif