#ifndef BAYESIAN_IMPORTED
#define BAYESIAN_IMPORTED
#include "surrogates.h"
#include "council.h"
#include <cmath>
#include <vector>
#include <limits>
#include <cstdlib>
typedef float (*CostFunction)(const std::vector<double>&);
typedef std::vector<double> (*AcquisitionFunction)(const SurrogateModel&, const std::vector<double>&, const std::vector<double> const std::vector<double>&);
class SmartBayesian {
private:
    std::vector<double> samples;
    std::vector<double> costs;
    int num_samples;
    std::vector<double> current_solution;
    double current_cost;
    CostFunction cost_function;
    float variance_threshold; 
    SurrogateModel* surrogate;
    AcquisitionFunction acquisition_function;
public:
    SmartBayesian(CostFunction cost_function, SurrogateModel* surrogate, AcquisitionFunction acquisition_function, float variance_threshold = 1e-5) 
        : cost_function(cost_function), surrogate(surrogate), acquisition_function(acquisition_function), variance_threshold(variance_threshold) {
        current_solution = std::vector<double>();
        current_cost = std::numeric_limits<double>::infinity();
        num_samples = 0;
    }
    void set_training_data(const std::vector<double>& samples_in, const std::vector<double>& costs_in) {
        samples = samples_in;
        costs = costs_in;
        num_samples = (int)costs_in.size();
    }
    void add_sample(const std::vector<double>& sample) {
        double cost = cost_function(sample);
        samples.insert(samples.end(), sample.begin(), sample.end());
        costs.push_back(cost);
        num_samples++;
        if (cost < current_cost) {
            current_solution = sample;
            current_cost = cost;
        }
    }
    std::vector<double> suggest_next(bool train = true) {
        int dim = current_solution.empty() ? (costs.empty() ? 1 : (int)(samples.size() / costs.size())) : (int)current_solution.size();
        if (num_samples < 5) {
            std::vector<double> random_suggestion(dim);
            for (size_t i = 0; i < random_suggestion.size(); ++i) {
                random_suggestion[i] = static_cast<double>(rand()) / RAND_MAX;
            }
            return random_suggestion;
        }
        if (train && surrogate) {
            surrogate->train(samples, costs, num_samples);
        }
        if (surrogate) {
            return acquisition_function(*surrogate, current_solution);
        }
        std::vector<double> random_suggestion(dim);
        for (size_t i = 0; i < random_suggestion.size(); ++i) {
            random_suggestion[i] = static_cast<double>(rand()) / RAND_MAX;
        }
        return random_suggestion;
    }
    std::vector<double> get_current_solution() const {
        return current_solution;
    }
    double get_current_cost() const {
        return current_cost;
    }
    std::vector<double> get_samples() const {
        return samples;
    }
    std::vector<double> get_costs() const {
        return costs;
    }
    std::vector<double> bayesianLoop(int iterations) {
        for (int i = 0; i < iterations; ++i) {
            std::vector<double> next_sample = suggest_next();
            add_sample(next_sample);
        }
        return current_solution;
    }
};
#endif