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
	std::vector<SurrogateModel> surrogate_model; // Weird approach: Allow *MULTIPLE* surrogate models, and they'll ALL be used at once 
	AcquisitionFunction acquisition_function;		

	
}
#endif
