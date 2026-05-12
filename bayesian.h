#ifndef BAYESIAN_IMPORTED
#define BAYESIAN_IMPORTED
#include "surrogates.h"
#include <cmath>
#include <vector>

typedef float (*CostFunction)(const std::vector<double>&);


class AcquisitionFunction {
	private:
	public:
		virtual ~AcquisitionFunction() = default
		virtual double evaluate(const std::vector<double>& question, const std::vector<double> answers, int num_samples) = 0;
};






class SmartBayesian {
	private:
	std::vector<double> samples;
	std::vector<double> costs;
	int num_samples;
	std::vector<double> current_solution;
	double current_cost;
	CostFunction cost_function;
	float variance_threshold; 
	Council council;
	AcquisitionFunction acquisition_function;		public:
	SmartBayesian(CostFunction cost_function, float variance_threshold = 1e-5) : cost_function(cost_function), variance_threshold(variance_threshold) {
		current_solution = std::vector<double>(); // Initialize with an empty solution
		current_cost = std::numeric_limits<double>::infinity(); // Start with an infinitely bad cost
		num_samples = 0;
	}
	public set_training_data(const std::vector<double>& samples, const std::vector<double>& costs) {
		this->samples = samples;
		this->costs = costs;
		this->num_samples = samples.size();
	}
	
}
#endif
