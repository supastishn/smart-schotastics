#ifndef SURROGATES_IMPORTED
#define SURROGATES_IMPORTED
#include <cmath>
#include <vector>



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

class RBFKernel : public Kernel {
	private:
		double lengthScale;
	public:
		RBFKernel(double lengthScale) : lengthScale(lengthScale) {}
		double evaluate(const std::vector<double>& x1, const std::vector<double>& x2) override {
			double sum = 0.0;
			for (size_t i = 0; i < x1.size(); ++i) {
				double diff = x1[i] - x2[i];
				sum += diff * diff;
			}
			return exp(-sum / (2 * lengthScale * lengthScale));
		}
};

class GaussianProcess : public SurrogateModel {
	private:
	Kernel* kernel;
	std::vector<double> similarityMatrix;
	std::vector<double> weights;
	public:
	GaussianProcess(Kernel* kernel) : kernel(kernel) {}
	void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
		for (int i = 0; i < num_samples; ++i) {
		    for (int j = 0; j < num_samples; ++j) {
		        similarityMatrix[i * num_samples + j] = kernel->evaluate(samples[i], samples[j]);
		    }
		}
		// chelovsky
		vector<double> weights(num_samples);
		vector<double> chelovskyDP(num_samples*num_samples);
		weights[0] = sqrt(similarityMatrix[0]);
		// loop over all cols
		for (int col = 0; col < num_rows; ++col) {
		    for (int row = col; row < num_rows; ++row) {		// matmt for row x col y iter i = e.x ab, ad, b²+c², bd+ce
										// if row == col: sqrt(similarity) - (dp[row][col-1] - ... dp[row][0] (set weight to this)
										// if row ≠ cil: 
			// (similarity - sig_i(dp[row][col-i] * sig_i[col][col-i])) / dp[col][col] 

			//dp[row][col] =>
			//sum(dp[col])
			float sum = 0.0f
				if (row == col) {
			for (int i = 0; i < row; ++i) {				
				sum += chelovskyDP[row * num_rows + i] * chelovskyDP[row * num_rows + i] // dp[row][col-1...0]
			}
			} else {
			for (int i = 0; i < col; ++i) {
				sum += chelovskyDP[row * num_rows + i] * chelovskyDP[col * num_rows + i]
			}
			}


			if (row == col) {
			dp[col * row_size + col] = sqrt(similarity[col * row_size + col] - sum);
			weights[col] = dp[col * row_size + col]; 

			} else {
			dp[row * row_size + col] = (similarity[row * row_size + col] - sum) / chelovskyDP[col * row_size + col]

			}
		    }
	}
}
	double predict(const std::vector<double>& question) override {

		double prediction = 0.0;
		for (int i = 0; i < num_samples; ++i) {
			double k = kernel->evaluate(question, samples[i]);
			prediction += weights[i] * k;
		}
		return prediction;

	}
};

struct RegionArgs {
	int samplesPerRegion;
	int minDistance; // if distanxe is less than, it'll consider itself the same region even if n > samplesPerRegion
	int maxDistance; // if distance exceeds, it'll consider itself a diff region even if n < samplesPerRegion
}



class Council {
	private:

	std::vector<double> samples;
	std::vector<double> costs;
	std::vector<int> regionSizes
	int fx_arguments; //args of f(xyz...)
	std::vector<SurrogateModel*> models;
	RegionArgs regionArgs;
	double softmaxBase = 1.5; // higher = amplify better models.
	vector<double> modelWeights;
	public:
	Council(const std::vector<SurrogateModel*>& models, const RegionArgs& regionArgs) : models(models), regionArgs(regionArgs) {

	}
	void setModels(const std::vector<SurrogateModel*>& models) {
		this->models = models;
	}
	void setSamples(const std::vector<double>& samples, const std::vector<double>& costs,) {
		this->samples = samples;
		this->costs = costs;
		this->fx_arguments = samples.size() / costs.size()

	}
	void train() {
		for (SurrogateModel* model : models) {
			model->train(samples, costs, samples.size());
		}
	vector<double> validate() {
		vector<double> errors(models.size() * samples.size(), 0.0);
		for (SurrogateModel* model : models) {
			for (int i = 0; i < samples.size(); ++i) {
				double prediction = model->predict(samples[i]);
				double error = abs(prediction - costs[i]);
			errors.push_back(error);
			}
		}
	return errors;
	}
	//greedy form regions
	void makeRegions() {
		int prevI = 0;
		for (int i = 0; i < samples.size(); ++i) {
		if (i < regionArgs.samplesPerRegion && ) continue; // not enough samples to make a region
		regionSizes.push_back(i - prevI + 1);
		prevI = i;	
		}
	}


}







