#ifndef COUNCIL_IMPORTED
#define COUNCIL_IMPORTED
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <utility>
#include <vector>
#include "helpers.h"
#include "surrogates.h"
struct RegionArgs {
    int minDistance;
    int maxDistance;
};
class Council : public SurrogateModel {
private:
    std::vector<double> samples;
    std::vector<double> costs;
    std::vector<std::pair<std::vector<double>, std::vector<double>>> softmaxes;
    int fx_arguments;
    std::vector<SurrogateModel*> models;
    std::random_device rd;
    mutable std::mt19937 gen;
    double validationGravity;
    double softmaxBase;
    RegionArgs regionArgs;
    std::vector<double> getSamplePoint(int index) const {
        std::vector<double> point(fx_arguments);
        for (int d = 0; d < fx_arguments; ++d) {
            point[d] = samples[index * fx_arguments + d];
        }
        return point;
    }
public:
    Council(const std::vector<SurrogateModel*>& models, const RegionArgs& regionArgs)
        : fx_arguments(0),
          models(models),
          gen(rd()),
          validationGravity(1.3),
          softmaxBase(1.5),
          regionArgs(regionArgs) {}
    void setModels(const std::vector<SurrogateModel*>& newModels) {
        models = newModels;
    }
    void setSamples(const std::vector<double>& newSamples, const std::vector<double>& newCosts) {
        samples = newSamples;
        costs = newCosts;
        if (!costs.empty()) {
            fx_arguments = static_cast<int>(samples.size() / costs.size());
        } else {
            fx_arguments = 0;
        }
    }
    void train() {
        if (costs.empty() || models.empty() || fx_arguments <= 0) {
            return;
        }
        if (costs.size() >= 4) {
            const int defaultValSize = std::min<int>(10, std::max<int>(1, static_cast<int>(costs.size()) / 4));
            validate(defaultValSize);
        } else {
            softmaxes.clear();
            for (SurrogateModel* model : models) {
                model->train(samples, costs, static_cast<int>(costs.size()));
            }
        }
    }
    void train(const std::vector<double>& newSamples, const std::vector<double>& newCosts, int num_samples) override {
        (void)num_samples;
        setSamples(newSamples, newCosts);
        train();
    }
    void validate(int validationSize) {
        softmaxes.clear();
        if (costs.size() < 4 || models.empty() || fx_arguments <= 0) {
            return;
        }
        const int totalCount = static_cast<int>(costs.size());
        const int actualValSize = std::max(1, std::min(validationSize, totalCount - 2));
        std::vector<int> indices(totalCount);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), gen);
        std::vector<int> validationIndices(indices.begin(), indices.begin() + actualValSize);
        std::vector<int> trainIndices(indices.begin() + actualValSize, indices.end());
        std::vector<double> trainSamples;
        trainSamples.reserve(trainIndices.size() * fx_arguments);
        std::vector<double> trainCosts;
        trainCosts.reserve(trainIndices.size());
        for (int idx : trainIndices) {
            for (int d = 0; d < fx_arguments; ++d) {
                trainSamples.push_back(samples[idx * fx_arguments + d]);
            }
            trainCosts.push_back(costs[idx]);
        }
        for (SurrogateModel* model : models) {
            model->train(trainSamples, trainCosts, static_cast<int>(trainCosts.size()));
        }
        for (int valIdx : validationIndices) {
            const std::vector<double> samplePoint = getSamplePoint(valIdx);
            std::vector<double> errors(models.size(), 0.0);
            double minErr = std::numeric_limits<double>::infinity();
            for (size_t m = 0; m < models.size(); ++m) {
                const GaussianPrediction prediction = models[m]->predict(samplePoint);
                const double error = std::fabs(prediction.mean - costs[valIdx]);
                errors[m] = error;
                minErr = std::min(minErr, error);
            }
            std::vector<double> softmaxesForSample(models.size(), 0.0);
            double sumExp = 0.0;
            for (size_t m = 0; m < models.size(); ++m) {
                const double weight = std::exp(softmaxBase * (minErr - errors[m]));
                softmaxesForSample[m] = weight;
                sumExp += weight;
            }
            for (double& w : softmaxesForSample) {
                w /= sumExp;
            }
            softmaxes.push_back({samplePoint, softmaxesForSample});
        }
        for (SurrogateModel* model : models) {
            model->train(samples, costs, static_cast<int>(costs.size()));
        }
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        if (models.empty()) {
            return {0.0, 1.0};
        }
        std::vector<GaussianPrediction> predictions;
        predictions.reserve(models.size());
        for (SurrogateModel* model : models) {
            predictions.push_back(model->predict(question));
        }
        std::vector<double> weightedPredictions(models.size(), 0.0);
        double weightSum = 0.0;
        for (const auto& softmax : softmaxes) {
            const double distance = nDimensionalDistance(softmax.first, question);
            const double weight = 1.0 / std::pow(std::max(1e-9, distance), validationGravity);
            weightSum += weight;
            for (size_t i = 0; i < models.size(); ++i) {
                weightedPredictions[i] += softmax.second[i] * weight;
            }
        }
        if (weightSum > 1e-9) {
            for (double& wp : weightedPredictions) {
                wp /= weightSum;
            }
        } else {
            const double uniform = 1.0 / static_cast<double>(models.size());
            for (double& wp : weightedPredictions) {
                wp = uniform;
            }
        }
        double mean = 0.0;
        for (size_t i = 0; i < models.size(); ++i) {
            mean += weightedPredictions[i] * predictions[i].mean;
        }
        double variance = 0.0;
        for (size_t i = 0; i < models.size(); ++i) {
            const double diff = predictions[i].mean - mean;
            variance += weightedPredictions[i] * (predictions[i].variance + diff * diff);
        }
        return {mean, std::max(1e-12, variance)};
    }
    RegionArgs getRegionArgs() const {
        return regionArgs;
    }
};
#endif