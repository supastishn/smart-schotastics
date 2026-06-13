#ifndef SURROGATES_IMPORTED
#define SURROGATES_IMPORTED
#include <cmath>
#include <vector>
#include <algorithm>
#include "helpers.h"
class SurrogateModel {
public:
    virtual ~SurrogateModel() = default;
    virtual void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) = 0;
    virtual double predict(const std::vector<double>& question) = 0;
};
class Kernel {
public:
    virtual ~Kernel() = default;
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
        return exp(-sum / (2.0 * lengthScale * lengthScale));
    }
};

struct GaussianPrediction {
    double mean;
    double variance;
};
class GaussianProcess : public SurrogateModel {
private:
    Kernel* kernel;
    std::vector<double> similarityMatrix;
    std::vector<double> weights;
    std::vector<double> trainingSamples;
    std::vector<double> choleskyDP;
    std::vector<double> diagonal;
    double priorVariance;
    int trainingDim;
    int numSamplesStored;
public:
    GaussianProcess(Kernel* kernel) : kernel(kernel), trainingDim(0), numSamplesStored(0) {}
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        numSamplesStored = num_samples;
        trainingDim = (int)(samples.size() / num_samples);
        trainingSamples = samples;
        similarityMatrix.resize(num_samples * num_samples);
	choleskyDP.resize(num_samples * num_samples);
     choleskyDP = choleskyDecomposition(similarityMatrix, costs, num_samples);


    }
    GaussianPrediction predict(const std::vector<double>& question) override {
        double mean = 0.0;
	std::vector<double> k(numSamplesStored);
        for (int i = 0; i < numSamplesStored; ++i) {
            std::vector<double> xi(trainingDim);
            for (int d = 0; d < trainingDim; ++d) {
                xi[d] = trainingSamples[i * trainingDim + d];
            }
            k[i] = kernel->evaluate(question, xi);
            mean += weights[i] * k[i];
        }
	// variance = priorVariance - k^T * var
	// var = K^-1 * k,  k = K^-1 * var
	// L*z = k, L.t*var = z
	std::vector<double> transformedK = k;
	std::vector<double> z(numSamplesStored);
	for (int col = 0; col < numSamplesStored; ++col) {
	    for (int row = 0; row <= col; ++row) {
		transformedK[row] -= choleskyDP[row * numSamplesStored + col] * z[col];
	    }
	    z[col] = transformedK[col] / diagonal[col];

    }

    std::vector<double> transformedZs = z;
    std::vector<double> variance(numSamplesStored);
    for (int col = numSamplesStored - 1; col >= 0; --col) {
	for (int row = numSamplesStored - 1; row > col; --row) {
	    transformedZs[row] -= chelovskyDP[col * numSamplesStored + row] * var[row];
	}
	variance[col] = transformedZs[col] / diagonal[col];
    }

    
    double endVariance
    for (int i = 0; i < numSamplesStored; ++i) {
	endVariance += k[i] * variance[i];
    }
    endVariance = priorVariance - endVariance;
    return {mean, endVariance};
	
}
};

#ifndef NN_IMPORTED
// TODO: IMPORT THINGS
// Yes I made the NN lib but I'm too lazy to implement it here :sob: 
class NeuralNetwork : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
	//TODO: Implement training logic for the neural network surrogate model
    }
    double predict(const std::vector<double>& question) override {
	//TODO: Implement prediction logic for the neural network surrogate model
	return 0.0; // Placeholder return value
    }
};
#endif

// Mls
class PiecewisePolynomialModel : public SurrogateModel {
	private:
	int polynomial_degree;
	int points_per_polynomial;
	vector<double> trainingSamples;
	vector<double> costs;
	int trainingDim;
	public:

	PiecewisePolynomialModel(int degree, int points) : polynomial_degree(degree), points_per_polynomial(points) {}

	void nDimensionalDistance(const std::vector<double>& a, const std::vector<double>& b) {
	double sum = 0.0;
	for (size_t i = 0; i < a.size(); ++i) {
		double diff = a[i] - b[i];
		sum += diff * diff;
	}
	return sqrt(sum);
	}
	void train(const std::vector<double>& samples, const std::vector<double>& costs) {
	trainingSamples = samples;
this->costs = costs;
trainingDim = (int)(samples.size() / costs.size());

	}

	void predict(const std::vector<double>& question) {
	std::vector<int> distances(costs.size());
	for (int i = 0; i < costs.size(); ++i) {
		std::vector<double> samplePoint(trainingDim);
		for (int d = 0; d < trainingDim; ++d) {
			samplePoint[d] = trainingSamples[i * trainingDim + d];
		}
		distances[i] = nDimensionalDistance(samplePoint, question);
	}



	sort(distances.begin(), distances.end());
	// combinatorial: (n+d, d)
	// col of matrix = variable, row = equation
	// a0 * sum(x_i^1) + a1 * sum(x_i^2) + ... = sum(y_i*x_i)
    // sum of products of monomials of [row] and column

	std::vector<double> leastSquaresMatrix(points_per_polynomial * points_per_polynomial, 0.0);
	std::vector<double> leastSquaresResults(points_per_polynomial, 0.0);
	for (int row = 0; row < points_per_polynomial; ++row) {
		for (int col = 0; col <= row; ++col) {
			double sum = 0;
				for (int i = 0; i < points_per_polynomial; ++i) {
					std::vector<double> sample(trainingSamples.begin() + distances[i] * trainingDim, trainingSamples.begin() + (distances[i] + 1) * trainingDim);
				//a more efficient aporoach wpuld be to make a vector double of sums of all tuples beforehand. hkwever icslready coddd this and am now too lazy to refactir.
				sum += sumAllProductsOfTuples(sample, col)[col - 1] * sumAllProductsOfTuples(sample, row)[row - 1];
				} 
			leastSquaresMatrix[row * points_per_polynomial + col] = sum;
		
		}
		
	}
    std::vector<double> coefficients(points_per_polynomial, 0.0);
    coefficients = choleskyDecomposition(leastSquaresMatrix, leastSquaresResults, points_per_polynomial);
    double prediction = 0.0;
    for (int i = 0; i < points_per_polynomial; ++i) {
        prediction += coefficients[i] * sumAllProductsOfTuples(question, i + 1)[i];
    }	
    double noise_variance = 0.0f; // avg of all errors
    for (int i = 0; i < points_per_polynomial; ++i) {
        double error = 0.0;
        for (int j = 0; j < points_per_polynomial; ++j) {
            error += coefficients[j] * sumAllProductsOfTuples(trainingSamples.begin() + distances[i] * trainingDim, trainingSamples.begin() + (distances[i] + 1) * trainingDim, j + 1)[j];
        }
        error -= costs[distances[i]];
        noise_variance += error * error;
	
}
noise_variance /= points_per_polynomial;
// we should calculate variance pointsise via cholesjy
//but this approxes good
return {prediction, noise_variance};
}
};

class RandomForest : public SurrogateModel {
public:
    void train(const std::vector<double>& samples, const std::vector<double>& costs, int num_samples) override {
        

#endif
