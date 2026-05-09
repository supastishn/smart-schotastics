#ifndef BAYESIAN_IMPORTED
#define BAYESIAN_IMPORTED

#include <vector>

typedef float (*CostFunction)(const std::vector<double>&);

class SurrogateModel {
	private:
	public:
		virtual ~SurrogateModel() = default
		virtual void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) = 0;
		virtual double predict(const std::vector<double>& question) = 0;
};

clsss Kernel {
	private:
	public:
		virtual ~Kernel() = default
		virtual double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) = 0;
};
class AcquisitionFunction {
	private:
	public:
		virtual ~AcquisitionFunction() = default
		virtual double evaluate(const std::vector<double>& question, const std::vector<double> answers, int num_samples) = 0;
};

class GaussianProcess : public SurrogateModel {
	private:
	Kernel* kernel;
	std::vector<double> similarityMatrixr;
	std::vector<double> weights;
	public:
	GaussianProcess(Kernel* kernel) : kernel(kernel) {}
	void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
		vector<double> similarityMatrix(num_samples * num_samples);
		for (int i = 0; i < num_samples; ++i) {
		    for (int j = 0; j < num_samples; ++j) {
		        similarityMatrix[i * num_samples + j] = kernel->evaluate(samples[i], samples[j]);
		    }
		}
		// gaussian eliminate, K*w=y:
		// eliminate downward
		for (int row = 0; row < num_samples; ++row) {
		
	}
		for (int col = 0; col < num_samples; ++col) {

		}
		
	}
	double predict(const std::vector<double>& question) override {
		// Predict the cost for the given question using the Gaussian Process
		return 0.0; // Placeholder return value
	}
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
