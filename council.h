#ifndef COUNCIL_IMPORTED
#define COUNCIL_IMPORTED
#include "surrogates.h"
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include "helpers.h"
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
    std::mt19937 gen;
    double validationGravity;
    double softmaxBase;
    std::vector<double> modelWeights;
    RegionArgs regionArgs;
    std::vector<double> getSamplePoint(int index) {
        std::vector<double> point(fx_arguments);
        for (int d = 0; d < fx_arguments; ++d) {
            point[d] = samples[index * fx_arguments + d];
        }
        return point;
    }
public:
    Council(const std::vector<SurrogateModel*>& models, const RegionArgs& regionArgs)
        : models(models), fx_arguments(0), softmaxBase(1.5), validationGravity(1.3), regionArgs(regionArgs), gen(rd()) {}
    void setModels(const std::vector<SurrogateModel*>& newModels) {
        models = newModels;
    }
    void train(const std::vector<double>& newSamples, const std::vector<double>& newCosts, int num_samples) override {
        samples = newSamples;
        costs = newCosts;
        fx_arguments = (int)(samples.size() / costs.size());
        for (SurrogateModel* model : models) {
            model->train(samples, costs, (int)costs.size());
        }
    }
    GaussianPrediction predict(const std::vector<double>& question) const override {
        std::vector<GaussianPrediction> predictions;
        for (SurrogateModel* model : models) {
            GaussianPrediction prediction = model->predict(question);	    
            predictions.push_back(prediction);
        }
        std::vector<double> weightedPredictions(models.size(), 0.0);
        double mean = 0.0;
        double variance = 0.0;
        double weightSum = 0.0;
        for (const auto& softmax : softmaxes) {
            double distance = nDimensionalDistance(softmax.first, question);
            double weight = 1.0 / pow(std::max(1e-9, distance), validationGravity);
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
            for (double& wp : weightedPredictions) {
                wp = 1.0 / models.size();
            }
        }
        for (size_t i = 0; i < models.size(); ++i) {
            mean += weightedPredictions[i] * predictions[i].mean;
            variance += weightedPredictions[i] * predictions[i].variance;
        }
        return {mean, variance};
    }
    void setSamples(const std::vector<double>& newSamples, const std::vector<double>& newCosts) {
        samples = newSamples;
        costs = newCosts;
        fx_arguments = (int)(samples.size() / costs.size());
    }
    void train() {
        for (SurrogateModel* model : models) {
            model->train(samples, costs, (int)costs.size());
        }
    }
    void validate(int validationSize) {
        softmaxes.clear();
        std::vector<int> validationCosts;
        std::uniform_int_distribution<> dis(0, (int)costs.size() - 1);
        for (int i = 0; i < validationSize; ++i) {
            if (i == 0 || i == validationSize - 1) {
                validationCosts.push_back(i);
                continue;
            }
            int index = dis(gen);
            int attempts = 0;
            while (std::find(validationCosts.begin(), validationCosts.end(), index) != validationCosts.end() && attempts < 100) {
                index = dis(gen);
                attempts++;
            }
            validationCosts.push_back(index);
        }
        for (size_t i = 0; i < validationCosts.size(); ++i) {
            std::vector<double> errors;
            std::vector<double> samplePoint = getSamplePoint(validationCosts[i]);
            double maxErr = 0.0;
            for (SurrogateModel* model : models) {
                GaussianPrediction prediction = model->predict(samplePoint);
                double error = fabs(prediction.mean - costs[validationCosts[i]]);
                errors.push_back(error);
                maxErr = std::max(maxErr, error);
            }
            std::vector<double> softmaxesForSample(models.size(), 0.0);
            double sumExp = 0.0;
            for (size_t j = 0; j < models.size(); ++j) {
                sumExp += exp(softmaxBase * (maxErr - errors[j]));
            }
            if (sumExp > 1e-9) {
                for (size_t j = 0; j < models.size(); ++j) {
                    softmaxesForSample[j] = exp(softmaxBase * (maxErr - errors[j])) / sumExp;
                }
            } else {
                for (size_t j = 0; j < models.size(); ++j) {
                    softmaxesForSample[j] = 1.0 / models.size();
                }
            }
            softmaxes.push_back({samplePoint, softmaxesForSample});
        }
    }
};
#endif