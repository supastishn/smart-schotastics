#ifndef BAYESIAN_IMPORTED
#define BAYESIAN_IMPORTED
#include "surrogates.h"
#include "council.h"
#include <cmath>
#include <vector>

typedef float (*CostFunction)(const std::vector<double>&);
typedef std::vector<double> (*AcquisitionFunction)(const SurrogateModel&, const std::vector<double>&);





class SmartBayesian {
	private:
	std::vector<double> samples;
	std::vector<double> costs;
	int num_samples;
	std::vector<double> current_solution;
	double current_cost;
	CostFunction cost_function;
	float variance_threshold; 
	SurrogateModel surrogate;
	AcquisitionFunction acquisition_function;		public:
	SmartBayesian(CostFunction cost_function, float variance_threshold = 1e-5) : cost_function(cost_function), variance_threshold(variance_threshold) {
		current_solution = std::vector<double>(); // Initialize with an empty solution
		current_cost = std::numeric_limits<double>::infinity(); // Start with an infinitely bad cost
		num_samples = 0;
	}
	
	void set_training_data(const std::vector<double>& samples, const std::vector<double>& costs) {
		this->samples = samples;
		this->costs = costs;
		this->num_samples = samples.size();
	}
	void add_sample(const std::vector<double>& sample) {
		double cost = cost_function(sample);
		samples.push_back(sample);
		costs.push_back(cost);
		num_samples++;
		if (cost < current_cost) {
			current_solution = sample;
			current_cost = cost;
		}
	}

	std::vector<double> suggest_next(bool train = true) {
		if (num_samples < 5) {
			// Not enough data to train the surrogate model, return a random suggestion
			std::vector<double> random_suggestion(current_solution.size());
			for (size_t i = 0; i < random_suggestion.size(); ++i
) {
				random_suggestion[i] = static_cast<double>(rand()) / RAND_MAX; // Random value between 0 and 1
			}
			return random_suggestion;
		}
		if (train) {
			surrogate.train(samples, costs, num_samples);
		}
		std::vector<double> suggestion = acquisition_function(surrogate, current_solution);
		return suggestion;
	}

	std::vector<double> get_current_solution() const {
		return current_solution;
	}

	std::vector<double> get_current_cost() const {
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

	
}
#endif
