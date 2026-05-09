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
		// Predict the cost for the given question using the Gaussian Process
		return 0.0; // Placeholder return value
	}
};



