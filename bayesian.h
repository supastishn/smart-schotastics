#ifndef BAYESIAN_IMPORTED
#define BAYESIAN_IMPORTED
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>
#include "council.h"
#include "helpers.h"
#include "surrogates.h"
typedef float (*CostFunction)(const std::vector<double>&);
typedef std::vector<double> (*AcquisitionFunction)(
    const SurrogateModel&,
    const std::vector<double>&,
    const std::vector<double>&,
    const std::vector<double>&
);
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
    std::vector<ParameterBound> bounds;
    std::mt19937 rng;
    int infer_dimension() const {
        if (!bounds.empty()) {
            return static_cast<int>(bounds.size());
        }
        if (!current_solution.empty()) {
            return static_cast<int>(current_solution.size());
        }
        if (!costs.empty() && !samples.empty()) {
            return static_cast<int>(samples.size() / costs.size());
        }
        return 1;
    }
    std::vector<double> to_unit_space(const std::vector<double>& raw_point) const {
        if (bounds.empty()) {
            return raw_point;
        }
        return normalizeInput(raw_point, bounds);
    }
    std::vector<double> from_unit_space(const std::vector<double>& unit_point) const {
        if (bounds.empty()) {
            return unit_point;
        }
        return denormalizeInput(unit_point, bounds);
    }
    std::vector<double> normalize_all_samples(int dim) const {
        if (bounds.empty()) {
            return samples;
        }
        std::vector<double> unit_samples;
        unit_samples.reserve(static_cast<size_t>(num_samples) * static_cast<size_t>(dim));
        std::vector<double> raw_point(dim);
        for (int i = 0; i < num_samples; ++i) {
            for (int d = 0; d < dim; ++d) {
                raw_point[d] = samples[static_cast<size_t>(i) * dim + d];
            }
            const std::vector<double> unit_point = normalizeInput(raw_point, bounds);
            unit_samples.insert(unit_samples.end(), unit_point.begin(), unit_point.end());
        }
        return unit_samples;
    }
public:
    SmartBayesian(
        CostFunction cost_function,
        SurrogateModel* surrogate,
        AcquisitionFunction acquisition_function,
        const std::vector<ParameterBound>& bounds = {},
        float variance_threshold = 1e-5f
    )
        : num_samples(0),
          current_cost(std::numeric_limits<double>::infinity()),
          cost_function(cost_function),
          variance_threshold(variance_threshold),
          surrogate(surrogate),
          acquisition_function(acquisition_function),
          bounds(bounds),
          rng(std::random_device{}()) {}
    void set_bounds(const std::vector<ParameterBound>& new_bounds) {
        bounds = new_bounds;
    }
    void set_training_data(const std::vector<double>& samples_in, const std::vector<double>& costs_in) {
        samples = samples_in;
        costs = costs_in;
        num_samples = static_cast<int>(costs_in.size());
        current_solution.clear();
        current_cost = std::numeric_limits<double>::infinity();
        if (num_samples <= 0) {
            return;
        }
        const int dim = static_cast<int>(samples.size() / costs.size());
        if (dim <= 0) {
            return;
        }
        for (int i = 0; i < num_samples; ++i) {
            if (costs[i] < current_cost) {
                current_cost = costs[i];
                current_solution.assign(
                    samples.begin() + static_cast<std::ptrdiff_t>(i) * dim,
                    samples.begin() + static_cast<std::ptrdiff_t>(i + 1) * dim
                );
            }
        }
    }
    void add_sample(const std::vector<double>& sample) {
        const double cost = static_cast<double>(cost_function(sample));
        samples.insert(samples.end(), sample.begin(), sample.end());
        costs.push_back(cost);
        num_samples++;
        if (cost < current_cost) {
            current_solution = sample;
            current_cost = cost;
        }
    }
    std::vector<double> suggest_next(bool train = true) {
        const int dim = infer_dimension();
        std::uniform_real_distribution<double> unit_dist(0.0, 1.0);
        if (num_samples < 5 || !surrogate || !acquisition_function) {
            std::vector<double> unit_random(dim);
            for (int i = 0; i < dim; ++i) {
                unit_random[i] = unit_dist(rng);
            }
            return from_unit_space(unit_random);
        }
        const std::vector<double> unit_samples = normalize_all_samples(dim);
        const ZScoreStats cost_stats = computeZScoreStats(costs);
        const std::vector<double> z_costs = normalizeCostsZScore(costs, cost_stats);
        const std::vector<double> unit_best = current_solution.empty()
            ? std::vector<double>(dim, 0.5)
            : to_unit_space(current_solution);
        if (train) {
            surrogate->train(unit_samples, z_costs, num_samples);
        }
        std::vector<double> unit_next = acquisition_function(*surrogate, unit_best, unit_samples, z_costs);
        if (!bounds.empty()) {
            clampUnitPoint(unit_next);
        }
        return from_unit_space(unit_next);
    }
    bool is_point_confident(const std::vector<double>& point) const {
        if (!surrogate) {
            return false;
        }
        const GaussianPrediction prediction = surrogate->predict(to_unit_space(point));
        return prediction.variance <= static_cast<double>(variance_threshold);
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
            const std::vector<double> next_sample = suggest_next();
            add_sample(next_sample);
        }
        return current_solution;
    }
};
#endif