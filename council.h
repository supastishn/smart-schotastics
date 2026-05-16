#ifndef COUNCIL_IMPORTED
#define COUNCIL_IMPORTED
#include "surrogates.h"
#include <cmath>
#include <vector>
struct RegionArgs {
    int minDistance;
    int maxDistance;
};
double nDimensionalDistance(const std::vector<double>& points1, const std::vector<double>& points2) {
    double sum = 0.0;
    for (size_t i = 0; i < points1.size(); ++i) {
        double diff = points1[i] - points2[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}
class Council {
private:
    std::vector<double> samples;
    std::vector<double> costs;
    std::vector<std::vector<double>> softmaxes
    int fx_arguments;
    std::vector<SurrogateModel*> models;
//rand devices
    std::random_device rd;
    std::mt19937 gen;
    
    double softmaxBase;
    std::vector<double> modelWeights;
    std::vector<double> getSamplePoint(int index) {
        std::vector<double> point(fx_arguments);
        for (int d = 0; d < fx_arguments; ++d) {
            point[d] = samples[index * fx_arguments + d]

        }
        return point;
    }
public:
    Council(const std::vector<SurrogateModel*>& models, const RegionArgs& regionArgs)
        : models(models), regionArgs(regionArgs), fx_arguments(0), softmaxBase(1.5) {}
    void setModels(const std::vector<SurrogateModel*>& newModels) {
        models = newModels;
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
    std::vector<double> validate(int validationSize) {
        std::vector<double> softmaxes;
	std::vector<int> validationCosts;
	std::uniform_int_distribution<> dis(0, (int)costs.size() - 1);
	for (int i = 0; i < validationSize; ++i) {
	int index = dis(gen);
	while (std::find(validationCosts.begin(), validationCosts.end(), costs[index]) != validationCosts.end()) {
	    index = dis(gen);

	}
	validationCosts.push_back(index);
	}
            for (size_t = 0; i < validationCosts.size(); ++i) {
		std:vector<double> errors(models.size());		
		    double max;
		for (SurrogateModel* model : models) {
                std::vector<double> samplePoint = getSamplePoint(validationCosts[i]);
                double prediction = model->predict(samplePoint);
                double error = fabs(prediction - costs[validationCosts[i]]);
		errors.push_back(error);
		max = std::max(max, error);
            }
		std::vector<double> softmaxesForSample(models.size());


		double sumExp = 0.0;
		for (size_t j = 0; j < models.size(); ++j) {
			sumExp += exp(softmaxBase * (max - errors[j]));
        }
		for (size_t j = 0; j < models.size(); ++j) {
		softmaxesForSample[j] = exp(softmaxBase * (max - errors[j])) / sumExp;
		softmaxes.push_back(softmaxesForSample[j]);
	}
	    }

	return softmaxes;
    }

};
#endif
