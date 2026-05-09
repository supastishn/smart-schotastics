#ifndef ANNEAL_IMPORTED
#define ANNEAL_IMPORTED
#include <vector>
#include <random>
#include <cmath>
typedef float (*CostFunction)(const std::vector<double>&);

// Simulted anneal, but after temperature decreases too much it resets to the best and descends into local minimum
class SmartAnneal {
	private:
	std::vector<double> current_solution;
	std::vector<double> best_solution;
	double best_cost;
	double current_cost;
	double temp;
	double cooling_rate;
	double randomness;
	double current_randomness:
	CostFunction cost_function;
	double improvementNeeded;
	int stagnation = 0;
	double tempToDescend;
	random_device rd;
        mt19937 gen(rd())


	

    void neighbor_pick() {
    	// pick random point close enough

	std:normal_distribution<> dis(0, current_randomness);
	vector<double> neighbor = current_solution;
	for (int i = 0; i < neighbor.size(); i++) {
	    neighbor[i] += dis(gen);
	}
	int cost = cost_function(neighbor);
	if (cost < current_cost) {
	    current_solution = neighbor;

	    if (cost < best_cost) {
		best_solution = neighbor;
		best_cost = cost;
	    }
	
	    if (cost < current_cost * improvementNeeded) {
		    current_randomness = randomness;
		stagnation = 0;
	    } else {
		    current_randomness *= 1/improvementNeeded; // reduce randomness if not improving enough
		stagnation++;
		
	    }
	current_cost = cost;
    }
	else {
		double acceptance_probability = exp((current_cost - cost) / temp);
		uniform_real_distribution<> dis(0, 1);
		if (dis(gen) < acceptance_probability) {
			current_solution = neighbor;
			current_cost = cost;
		}
		
}
    void descend(float lambda) {
	vector<double> neighbor = best_solution;
	for (int i = 0; i < best_solution.size(); i++) {
	    vector<double> negNeighbor = best_solution 
	    negNeighbor[i] -= lambda;
	    vector<double> posNeighbor = best_solution + lambda;
	    posNeighbor[i] += lambda;
	    int negCost = cost_function(negNeighbor);
	    int posCost = cost_function(posNeighbor);
	    if (max(negCost, posCost) < best_cost) {
	    neighbor = negCost < posCost ? negNeighbor : posNeighbor;
	}	
	}
}
    void cool() {
	temp *= cooling_rate;
    }

    double get_temperature() const {
	return temp;
    }
}

	
public:
    SmartAnneal(double initial_temp, double cooling_rate, double random, CostFunction cost_function, double improvementNeeded = 0.95, double tempToDescend = 0.01) {
	this->temp = initial_temp;
	this->cooling_rate = cooling_rate;
	this->cost_function = cost_function;
	this->randomness = random; 
	this->improvementNeeded = improvementNeeded;
	this->tempToDescend = tempToDescend;
    	this->current_randomness = randomness;
    	this->current_solution = random_solution();
    	this->current_cost = cost_function(current_solution);
    	this->best_solution = current_solution;
    	this->best_cost = current_cost;
    }

	
    void iterate() {
	if (temp < tempToDescend) {
		descend(0.01); // lambda is 0.01, can be adjusted
		temp = tempToDescend; // reset temp to avoid descending again immediately
	} else {
		neighbor_pick();
		cool();
	}

}
	vector<double> get_best_solution() const {
	return best_solution;
    }



}
