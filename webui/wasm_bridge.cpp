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
    std::vector<std::unique_ptr<Kernel>> kernels;
    std::vector<std::unique_ptr<SurrogateModel>> models;
    std::unique_ptr<Council> council;
    SurrogateModel* active;
};
inline int parsePolynomialDegreeId(const std::string& id, int fallbackDegree) {
    if (id.rfind("poly_", 0) == 0 && id.size() > 5) {
        try {
            const int parsed = std::stoi(id.substr(5));
            return std::max(1, std::min(10, parsed));
        } catch (...) {
            return std::max(1, fallbackDegree);
        }
    }
    return std::max(1, fallbackDegree);
}
inline void addSurrogateById(
    SurrogateBundle& bundle,
    const std::string& id,
    double length_scale,
    int fallback_poly_degree,
    int poly_points
) {
    const double safeLen = std::max(1e-3, length_scale);
    const int safePoints = std::max(4, poly_points);
    if (id == "gp_rbf" || id == "gaussian_process") {
        bundle.kernels.push_back(std::make_unique<RBFKernel>(safeLen));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
    } else if (id == "gp_matern52") {
        bundle.kernels.push_back(std::make_unique<Matern52Kernel>(safeLen));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
    } else if (id == "gp_matern32") {
        bundle.kernels.push_back(std::make_unique<Matern32Kernel>(safeLen));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
    } else if (id == "gp_matern12") {
        bundle.kernels.push_back(std::make_unique<Matern12Kernel>(safeLen));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
    } else if (id == "gp_rq") {
        bundle.kernels.push_back(std::make_unique<RationalQuadraticKernel>(safeLen, 1.0));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
    } else if (id == "polynomial" || id.rfind("poly_", 0) == 0) {
        const int degree = parsePolynomialDegreeId(id, fallback_poly_degree);
        bundle.models.push_back(std::make_unique<PiecewisePolynomialModel>(degree, safePoints));
    } else if (id == "council") {
        bundle.kernels.push_back(std::make_unique<RBFKernel>(safeLen));
        bundle.models.push_back(std::make_unique<GaussianProcess>(bundle.kernels.back().get(), 1e-5));
        bundle.models.push_back(std::make_unique<PiecewisePolynomialModel>(std::max(1, fallback_poly_degree), safePoints));
    }
}
inline SurrogateBundle createSurrogateBundle(
    const std::vector<std::string>& surrogate_ids,
    double length_scale,
    int fallback_poly_degree,
    int poly_points
) {
    SurrogateBundle bundle;
    for (const std::string& id : surrogate_ids) {
        addSurrogateById(bundle, id, length_scale, fallback_poly_degree, poly_points);
    }
    if (bundle.models.empty()) {
        addSurrogateById(bundle, "gp_rbf", length_scale, fallback_poly_degree, poly_points);
    }
    if (bundle.models.size() == 1) {
        bundle.active = bundle.models[0].get();
    } else {
        std::vector<SurrogateModel*> modelPtrs;
        modelPtrs.reserve(bundle.models.size());
        for (const auto& m : bundle.models) {
            modelPtrs.push_back(m.get());
        }
        bundle.council = std::make_unique<Council>(modelPtrs, RegionArgs{1, 10});
        bundle.active = bundle.council.get();
    }
    return bundle;
}
inline WasmSuggestionResult suggestNextTrial(
    const std::vector<WasmParamConfig>& params,
    const std::vector<double>& flat_trial_params,
    const std::vector<double>& trial_objectives,
    const std::string& direction,
    double target_value,
    const std::vector<std::string>& surrogate_ids,
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
    const bool maximize = (direction == "maximize");
    const bool is_target = (direction == "target");
    std::vector<double> oriented_costs(num_trials, 0.0);
    std::vector<double> used_samples(
        flat_trial_params.begin(),
        flat_trial_params.begin() + static_cast<std::ptrdiff_t>(num_trials * dim)
    );
    for (size_t i = 0; i < num_trials; ++i) {
        oriented_costs[i] = maximize ? -trial_objectives[i] : trial_objectives[i];
    }
    SurrogateBundle bundle = createSurrogateBundle(surrogate_ids, rbf_length_scale, poly_degree, poly_points);
    std::vector<double> unit_samples;
    unit_samples.reserve(num_trials * dim);
    std::vector<double> raw_point(dim, 0.0);
    size_t best_idx = 0;
    double best_score = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < num_trials; ++i) {
        for (size_t d = 0; d < dim; ++d) {
            raw_point[d] = used_samples[i * dim + d];
        }
        const std::vector<double> unit_pt = normalizeInput(raw_point, bounds);
        unit_samples.insert(unit_samples.end(), unit_pt.begin(), unit_pt.end());
        const double score = is_target ? std::fabs(oriented_costs[i] - target_value) : oriented_costs[i];
        if (score < best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    const ZScoreStats cost_stats = computeZScoreStats(oriented_costs);
    const std::vector<double> z_costs = normalizeCostsZScore(oriented_costs, cost_stats);
    const std::vector<double> unit_best(
        unit_samples.begin() + static_cast<std::ptrdiff_t>(best_idx * dim),
        unit_samples.begin() + static_cast<std::ptrdiff_t>((best_idx + 1) * dim)
    );
    bundle.active->train(unit_samples, z_costs, static_cast<int>(num_trials));
    std::vector<double> unit_next;
    if (is_target) {
        const double target_z = normalizeSingleCostZScore(target_value, cost_stats);
        if (acquisition_type == "perihelion") {
            unit_next = targetExpectedImprovementPerihelionDefault(*bundle.active, unit_best, unit_samples, z_costs, target_z);
        } else if (acquisition_type == "random") {
            unit_next = targetExpectedImprovement(*bundle.active, unit_best, unit_samples, z_costs, target_z);
        } else {
            unit_next = targetExpectedImprovementLBFGS(*bundle.active, unit_best, unit_samples, z_costs, target_z);
        }
    } else {
        AcquisitionFunction acq = selectAcquisitionFunction(acquisition_type);
        unit_next = acq(*bundle.active, unit_best, unit_samples, z_costs);
    }
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
    const std::vector<std::string>& surrogate_ids,
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
    SurrogateBundle bundle = createSurrogateBundle(surrogate_ids, rbf_length_scale, poly_degree, poly_points);
    bundle.active->train(unit_samples, z_costs, static_cast<int>(num_trials));
    const GaussianPrediction pred = bundle.active->predict(normalizeInput(query_point, bounds));
    double unscaled_mean = pred.mean * cost_stats.std_dev + cost_stats.mean;
    if (maximize) {
        unscaled_mean = -unscaled_mean;
    }
    const double unscaled_std = std::sqrt(std::max(1e-12, pred.variance)) * cost_stats.std_dev;
    return {unscaled_mean, unscaled_std};
}
inline std::vector<WasmPredictionResult> predictObjectiveBatch(
    const std::vector<WasmParamConfig>& params,
    const std::vector<double>& flat_trial_params,
    const std::vector<double>& trial_objectives,
    const std::vector<double>& flat_query_points,
    bool maximize,
    const std::vector<std::string>& surrogate_ids,
    double rbf_length_scale,
    int poly_degree,
    int poly_points
) {
    const std::vector<ParameterBound> bounds = buildParameterBounds(params);
    const size_t dim = bounds.size();
    std::vector<WasmPredictionResult> results;
    if (dim == 0 || trial_objectives.empty() || flat_trial_params.size() < dim || flat_query_points.size() < dim) {
        return results;
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
    SurrogateBundle bundle = createSurrogateBundle(surrogate_ids, rbf_length_scale, poly_degree, poly_points);
    bundle.active->train(unit_samples, z_costs, static_cast<int>(num_trials));
    const size_t num_queries = flat_query_points.size() / dim;
    results.reserve(num_queries);
    std::vector<double> query_point(dim, 0.0);
    for (size_t q = 0; q < num_queries; ++q) {
        for (size_t d = 0; d < dim; ++d) {
            query_point[d] = flat_query_points[q * dim + d];
        }
        const GaussianPrediction pred = bundle.active->predict(normalizeInput(query_point, bounds));
        double unscaled_mean = pred.mean * cost_stats.std_dev + cost_stats.mean;
        if (maximize) {
            unscaled_mean = -unscaled_mean;
        }
        const double unscaled_std = std::sqrt(std::max(1e-12, pred.variance)) * cost_stats.std_dev;
        results.push_back({unscaled_mean, unscaled_std});
    }
    return results;
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
    emscripten::register_vector<std::string>("VectorString");
    emscripten::register_vector<WasmParamConfig>("VectorParamConfig");
    emscripten::register_vector<WasmPredictionResult>("VectorPredictionResult");
    emscripten::function("suggestNextTrial", &suggestNextTrial);
    emscripten::function("predictObjectiveAtPoint", &predictObjectiveAtPoint);
    emscripten::function("predictObjectiveBatch", &predictObjectiveBatch);
}
#endif