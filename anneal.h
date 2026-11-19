#ifndef ANNEAL_IMPORTED
#define ANNEAL_IMPORTED
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
typedef float (*CostFunction)(const std::vector<double>&);
class SmartAnneal {
private:
    std::vector<double> current_solution;
    std::vector<double> best_solution;
    double best_cost;
    double current_cost;
    double temp;
    double cooling_rate;
    double randomness;
    double current_randomness;
    CostFunction cost_function;
    double improvementNeeded;
    int stagnation;
    double tempToDescend;
    int dimensions;
    std::random_device rd;
    std::mt19937 gen;
    std::vector<double> random_solution() {
        std::uniform_real_distribution<double> dis(0.0, 1.0);
        std::vector<double> point(dimensions, 0.0);
        for (int i = 0; i < dimensions; ++i) {
            point[i] = dis(gen);
        }
        return point;
    }
    void neighbor_pick() {
        std::normal_distribution<double> dis(0.0, current_randomness);
        std::vector<double> neighbor = current_solution;
        for (size_t i = 0; i < neighbor.size(); ++i) {
            neighbor[i] += dis(gen);
        }
        const double cost = static_cast<double>(cost_function(neighbor));
        if (cost < current_cost) {
            const double requiredDrop = std::abs(current_cost) * (1.0 - improvementNeeded);
            if ((current_cost - cost) > requiredDrop) {
                current_randomness = randomness;
                stagnation = 0;
            } else {
                current_randomness *= improvementNeeded;
                stagnation++;
            }
            current_solution = neighbor;
            current_cost = cost;
            if (cost < best_cost) {
                best_solution = neighbor;
                best_cost = cost;
            }
        } else {
            const double acceptance_probability = std::exp((current_cost - cost) / std::max(1e-12, temp));
            std::uniform_real_distribution<double> acceptDis(0.0, 1.0);
            if (acceptDis(gen) < acceptance_probability) {
                current_solution = neighbor;
                current_cost = cost;
            }
        }
    }
    void descend(double lambda) {
        std::vector<double> candidate = best_solution;
        double candidate_cost = best_cost;
        for (size_t i = 0; i < best_solution.size(); ++i) {
            std::vector<double> negNeighbor = best_solution;
            negNeighbor[i] -= lambda;
            std::vector<double> posNeighbor = best_solution;
            posNeighbor[i] += lambda;
            const double negCost = static_cast<double>(cost_function(negNeighbor));
            const double posCost = static_cast<double>(cost_function(posNeighbor));
            if (std::min(negCost, posCost) < candidate_cost) {
                if (negCost < posCost) {
                    candidate = negNeighbor;
                    candidate_cost = negCost;
                } else {
                    candidate = posNeighbor;
                    candidate_cost = posCost;
                }
            }
        }
        if (candidate_cost < best_cost) {
            best_solution = candidate;
            best_cost = candidate_cost;
            current_solution = candidate;
            current_cost = candidate_cost;
        }
    }
    void cool() {
        temp *= cooling_rate;
    }
public:
    SmartAnneal(
        double initial_temp,
        double cooling_rate,
        double random,
        CostFunction cost_function,
        int dimensions = 1,
        double improvementNeeded = 0.95,
        double tempToDescend = 0.01
    )
        : temp(initial_temp),
          cooling_rate(cooling_rate),
          randomness(random),
          current_randomness(random),
          cost_function(cost_function),
          improvementNeeded(improvementNeeded),
          stagnation(0),
          tempToDescend(tempToDescend),
          dimensions(std::max(1, dimensions)),
          gen(rd()) {
        current_solution = random_solution();
        current_cost = static_cast<double>(cost_function(current_solution));
        best_solution = current_solution;
        best_cost = current_cost;
    }
    void iterate() {
        if (temp < tempToDescend) {
            descend(0.01);
            temp = tempToDescend;
        } else {
            neighbor_pick();
            cool();
        }
    }
    double get_temperature() const {
        return temp;
    }
    int get_stagnation() const {
        return stagnation;
    }
    std::vector<double> get_best_solution() const {
        return best_solution;
    }
    double get_best_cost() const {
        return best_cost;
    }
};
#endif