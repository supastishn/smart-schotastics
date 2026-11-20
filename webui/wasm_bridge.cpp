#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "acquisitions.h"
#include "bayesian.h"
#include "council.h"
#include "helpers.h"
#include "lbfgs.h"
#include "perihelion.h"
#include "surrogates.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#endif
struct WasmParamConfig {
    std::string name;
    std::string type;
    double lower;
    double upper;
    bool log_scale;
    int num_categories;
};
struct WasmSuggestionResult {
    std::vector<double> values;
    double predicted_mean;
    double predicted_std;
    bool used_warmup;
};
struct WasmPredictionResult {
    double mean;
    double std_dev;
};
inline float dummyBridgeCostFunction(const std::vector<double>& point) {
    (void)point;
    return 0.0f;
}
inline std::vector<ParameterBound> buildParameterBounds(const std::vector<WasmParamConfig>& params) {
    std::vector<ParameterBound> bounds;
    bounds.reserve(params.size());
    for (const WasmParamConfig& p : params) {
        if (p.type == "categorical") {
            const double upperCat = static_cast<double>(std::max(1, p.num_categories) - 1);
            bounds.push_back({0.0, upperCat, false});
        } else {
            const double hi = (p.upper > p.lower) ? p.upper : (p.lower + 1.0);
            const bool allowLog = p.log_scale && p.lower > 0.0;
            bounds.push_back({p.lower, hi, allowLog});
        }
    }
    return bounds;
}
inline void snapValuesToParamTypes(std::vector<double>& values, const std::vector<WasmParamConfig>& params) {
    const size_t dim = std::min(values.size(), params.size());
    for (size_t i = 0; i < dim; ++i) {
        const WasmParamConfig& p = params[i];
        if (p.type == "categorical") {
            const double upperCat = static_cast<double>(std::max(1, p.num_categories) - 1);
            const double rounded = std::round(values[i]);
            values[i] = std::max(0.0, std::min(upperCat, rounded));
        } else if (p.type == "int") {
            const double hi = (p.upper > p.lower) ? p.upper : (p.lower + 1.0);
            const double rounded = std::round(values[i]);
            values[i] = std::max(std::ceil(p.lower), std::min(std::floor(hi), rounded));
        } else {
            const double hi = (p.upper > p.lower) ? p.upper : (p.lower + 1.0);
            values[i] = std::max(p.lower, std::min(hi, values[i]));
        }
    }
}
inline AcquisitionFunction selectAcquisitionFunction(const std::string& acquisition_type) {
    if (acquisition_type == "perihelion") {
        return expectedImprovementPerihelionDefault;
    }
    if (acquisition_type == "random") {
        return expectedImprovement;
    }
    return expectedImprovementLBFGS;
}
struct SurrogateBundle {
    std::unique_ptr<RBFKernel> kernel;
    std::unique_ptr<GaussianProcess> gp;
    std::unique_ptr<PiecewisePolynomialModel> poly;
    std::unique_ptr<Council> council;
    SurrogateModel* active;
};
inline SurrogateBundle createSurrogateBundle(
    const std::string& surrogate_type,
    double rbf_length_scale,
    int poly_degree,
    int poly_points
) {
    SurrogateBundle bundle;
    bundle.kernel = std::make_unique<RBFKernel>(std::max(1e-3, rbf_length_scale));
    bundle.gp = std::make_unique<GaussianProcess>(bundle.kernel.get(), 1e-5);
    bundle.poly = std::make_unique<PiecewisePolynomialModel>(
        std::max(1, poly_degree),
        std::max(4, poly_points)
    );
    if (surrogate_type == "gaussian_process") {
        bundle.active = bundle.gp.get();
    } else if (surrogate_type == "polynomial") {
        bundle.active = bundle.poly.get();
    } else {
        std::vector<SurrogateModel*> models = {bundle.gp.get(), bundle.poly.get()};
        bundle.council = std::make_unique<Council>(models, RegionArgs{1, 10});
        bundle.active = bundle.council.get();
    }
    return bundle;
}
inline WasmSuggestionResult suggestNextTrial(
    const std::vector<WasmParamConfig>& params,
    const std::vector<double>& flat_trial_params,
    const std::vector<double>& trial_objectives,
    bool maximize,
    const std::string& surrogate_type,
    const std::string& acquisition_type,
    double rbf_length_scale,
    int poly_degree,
    int poly_points,
    int warmup_trials
) {
    const std::vector<ParameterBound> bounds = buildParameterBounds(params);
    const size_t dim = bounds.size();
    WasmSuggestionResult result{{}, 0.0, 1.0, false};
    if (dim == 0) {
        return result;
    }
    const size_t num_trials = std::min(trial_objectives.size(), flat_trial_params.size() / dim);
    const size_t required_warmup = static_cast<size_t>(std::max(1, warmup_trials));
    if (num_trials < required_warmup) {
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> unit_dist(0.0, 1.0);
        std::vector<double> unit_point(dim, 0.5);
        for (size_t d = 0; d < dim; ++d) {
            unit_point[d] = unit_dist(rng);
        }
        result.values = denormalizeInput(unit_point, bounds);
        snapValuesToParamTypes(result.values, params);
        result.predicted_mean = 0.0;
        result.predicted_std = 1.0;
        result.used_warmup = true;
        return result;
    }
    std::vector<double> oriented_costs(num_trials, 0.0);
    std::vector<double> used_samples(
        flat_trial_params.begin(),
        flat_trial_params.begin() + static_cast<std::ptrdiff_t>(num_trials * dim)
    );
    for (size_t i = 0; i < num_trials; ++i) {
        oriented_costs[i] = maximize ? -trial_objectives[i] : trial_objectives[i];
    }
    SurrogateBundle bundle = createSurrogateBundle(surrogate_type, rbf_length_scale, poly_degree, poly_points);
    AcquisitionFunction acq = selectAcquisitionFunction(acquisition_type);
    SmartBayesian optimizer(dummyBridgeCostFunction, bundle.active, acq, bounds);
    optimizer.set_training_data(used_samples, oriented_costs);
    std::vector<double> unit_samples;
    unit_samples.reserve(num_trials * dim);
    std::vector<double> raw_point(dim, 0.0);
    for (size_t i = 0; i < num_trials; ++i) {
        for (size_t d = 0; d < dim; ++d) {
            raw_point[d] = used_samples[i * dim + d];
        }
        const std::vector<double> unit_pt = normalizeInput(raw_point, bounds);
        unit_samples.insert(unit_samples.end(), unit_pt.begin(), unit_pt.end());
    }
    const ZScoreStats cost_stats = computeZScoreStats(oriented_costs);
    const std::vector<double> z_costs = normalizeCostsZScore(oriented_costs, cost_stats);
    const std::vector<double> unit_best = normalizeInput(optimizer.get_current_solution(), bounds);
    bundle.active->train(unit_samples, z_costs, static_cast<int>(num_trials));
    std::vector<double> unit_next = acq(*bundle.active, unit_best, unit_samples, z_costs);
    clampUnitPoint(unit_next);
    result.values = denormalizeInput(unit_next, bounds);
    snapValuesToParamTypes(result.values, params);
    const std::vector<double> snapped_unit = normalizeInput(result.values, bounds);
    const GaussianPrediction pred = bundle.active->predict(snapped_unit);
    double unscaled_mean = pred.mean * cost_stats.std_dev + cost_stats.mean;
    if (maximize) {
        unscaled_mean = -unscaled_mean;
    }
    result.predicted_mean = unscaled_mean;
    result.predicted_std = std::sqrt(std::max(1e-12, pred.variance)) * cost_stats.std_dev;
    result.used_warmup = false;
    return result;
}
inline WasmPredictionResult predictObjectiveAtPoint(
    const std::vector<WasmParamConfig>& params,
    const std::vector<double>& flat_trial_params,
    const std::vector<double>& trial_objectives,
    const std::vector<double>& query_point,
    bool maximize,
    const std::string& surrogate_type,
    double rbf_length_scale,
    int poly_degree,
    int poly_points
) {
    const std::vector<ParameterBound> bounds = buildParameterBounds(params);
    const size_t dim = bounds.size();
    if (dim == 0 || trial_objectives.empty() || flat_trial_params.size() < dim) {
        return {0.0, 1.0};
    }
    const size_t num_trials = std::min(trial_objectives.size(), flat_trial_params.size() / dim);
    std::vector<double> oriented_costs(num_trials, 0.0);
    for (size_t i = 0; i < num_trials; ++i) {
        oriented_costs[i] = maximize ? -trial_objectives[i] : trial_objectives[i];
    }
    std::vector<double> unit_samples;
    unit_samples.reserve(num_trials * dim);
    std::vector<double> raw_point(dim, 0.0);
    for (size_t i = 0; i < num_trials; ++i) {
        for (size_t d = 0; d < dim; ++d) {
            raw_point[d] = flat_trial_params[i * dim + d];
        }
        const std::vector<double> unit_pt = normalizeInput(raw_point, bounds);
        unit_samples.insert(unit_samples.end(), unit_pt.begin(), unit_pt.end());
    }
    const ZScoreStats cost_stats = computeZScoreStats(oriented_costs);
    const std::vector<double> z_costs = normalizeCostsZScore(oriented_costs, cost_stats);
    SurrogateBundle bundle = createSurrogateBundle(surrogate_type, rbf_length_scale, poly_degree, poly_points);
    bundle.active->train(unit_samples, z_costs, static_cast<int>(num_trials));
    const GaussianPrediction pred = bundle.active->predict(normalizeInput(query_point, bounds));
    double unscaled_mean = pred.mean * cost_stats.std_dev + cost_stats.mean;
    if (maximize) {
        unscaled_mean = -unscaled_mean;
    }
    const double unscaled_std = std::sqrt(std::max(1e-12, pred.variance)) * cost_stats.std_dev;
    return {unscaled_mean, unscaled_std};
}
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(optima_module) {
    emscripten::value_object<WasmParamConfig>("WasmParamConfig")
        .field("name", &WasmParamConfig::name)
        .field("type", &WasmParamConfig::type)
        .field("lower", &WasmParamConfig::lower)
        .field("upper", &WasmParamConfig::upper)
        .field("log_scale", &WasmParamConfig::log_scale)
        .field("num_categories", &WasmParamConfig::num_categories);
    emscripten::value_object<WasmSuggestionResult>("WasmSuggestionResult")
        .field("values", &WasmSuggestionResult::values)
        .field("predicted_mean", &WasmSuggestionResult::predicted_mean)
        .field("predicted_std", &WasmSuggestionResult::predicted_std)
        .field("used_warmup", &WasmSuggestionResult::used_warmup);
    emscripten::value_object<WasmPredictionResult>("WasmPredictionResult")
        .field("mean", &WasmPredictionResult::mean)
        .field("std_dev", &WasmPredictionResult::std_dev);
    emscripten::register_vector<double>("VectorDouble");
    emscripten::register_vector<WasmParamConfig>("VectorParamConfig");
    emscripten::function("suggestNextTrial", &suggestNextTrial);
    emscripten::function("predictObjectiveAtPoint", &predictObjectiveAtPoint);
}
#endif