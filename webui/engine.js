(function(globalScope) {
    function parseParamConfig(param) {
        const type = param.type || 'float';
        const rawRange = String(param.range || '');
        if (type === 'categorical') {
            const categories = rawRange.split(',').map(function(s) {
                return s.trim();
            }).filter(function(s) {
                return s.length > 0;
            });
            const safeCategories = categories.length > 0 ? categories : ['default'];
            return {
                name: param.name,
                type: 'categorical',
                lower: 0,
                upper: Math.max(0, safeCategories.length - 1),
                log_scale: false,
                num_categories: safeCategories.length,
                categories: safeCategories
            };
        }
        const parts = rawRange.split(',').map(function(s) {
            return parseFloat(s.trim());
        }).filter(function(v) {
            return !Number.isNaN(v);
        });
        let lower = parts.length >= 1 ? parts[0] : 0.0;
        let upper = parts.length >= 2 ? parts[1] : lower + 1.0;
        if (upper < lower) {
            const tmp = lower;
            lower = upper;
            upper = tmp;
        }
        if (upper === lower) {
            upper = lower + 1.0;
        }
        const logScale = Boolean(param.logScale) && lower > 0;
        return {
            name: param.name,
            type: type,
            lower: lower,
            upper: upper,
            log_scale: logScale,
            num_categories: 0,
            categories: []
        };
    }
    function encodeTrialParams(paramValuesObj, parsedParams) {
        return parsedParams.map(function(p) {
            const raw = paramValuesObj ? paramValuesObj[p.name] : undefined;
            if (p.type === 'categorical') {
                if (typeof raw === 'number' && !Number.isNaN(raw)) {
                    return Math.max(0, Math.min(p.num_categories - 1, Math.round(raw)));
                }
                const idx = p.categories.indexOf(String(raw));
                return idx >= 0 ? idx : 0;
            }
            const num = parseFloat(raw);
            if (Number.isNaN(num)) {
                return (p.lower + p.upper) * 0.5;
            }
            return Math.max(p.lower, Math.min(p.upper, num));
        });
    }
    function decodeParamValues(numericValues, parsedParams) {
        const obj = {};
        for (let i = 0; i < parsedParams.length; i++) {
            const p = parsedParams[i];
            const val = i < numericValues.length ? numericValues[i] : p.lower;
            if (p.type === 'categorical') {
                const idx = Math.max(0, Math.min(p.categories.length - 1, Math.round(val)));
                obj[p.name] = p.categories[idx];
            } else if (p.type === 'int') {
                obj[p.name] = Math.round(Math.max(Math.ceil(p.lower), Math.min(Math.floor(p.upper), val)));
            } else {
                obj[p.name] = Number(Math.max(p.lower, Math.min(p.upper, val)).toPrecision(6));
            }
        }
        return obj;
    }
    let wasmModuleInstance = null;
    let initPromise = null;
    function init() {
        if (initPromise) {
            return initPromise;
        }
        initPromise = new Promise(function(resolve) {
            if (typeof globalScope.createOptimaModule === 'function') {
                globalScope.createOptimaModule().then(function(mod) {
                    wasmModuleInstance = mod;
                    resolve('wasm');
                }).catch(function() {
                    wasmModuleInstance = null;
                    resolve('unloaded');
                });
                return;
            }
            const script = document.createElement('script');
            script.src = 'optima.js';
            script.async = true;
            script.onload = function() {
                if (typeof globalScope.createOptimaModule === 'function') {
                    globalScope.createOptimaModule().then(function(mod) {
                        wasmModuleInstance = mod;
                        resolve('wasm');
                    }).catch(function() {
                        wasmModuleInstance = null;
                        resolve('unloaded');
                    });
                } else {
                    wasmModuleInstance = null;
                    resolve('unloaded');
                }
            };
            script.onerror = function() {
                wasmModuleInstance = null;
                resolve('unloaded');
            };
            document.head.appendChild(script);
        });
        return initPromise;
    }
    function isReady() {
        return wasmModuleInstance !== null;
    }
    function suggestNext(experiment, config) {
        if (!wasmModuleInstance) {
            return null;
        }
        const parsedParams = (experiment.parameters || []).map(parseParamConfig);
        const dim = parsedParams.length;
        if (dim === 0) {
            return null;
        }
        const paramVec = new wasmModuleInstance.VectorParamConfig();
        const flatVec = new wasmModuleInstance.VectorDouble();
        const objVec = new wasmModuleInstance.VectorDouble();
        try {
            parsedParams.forEach(function(p) {
                paramVec.push_back({
                    name: p.name,
                    type: p.type,
                    lower: p.lower,
                    upper: p.upper,
                    log_scale: p.log_scale,
                    num_categories: p.num_categories
                });
            });
            const trials = experiment.trials || [];
            trials.forEach(function(t) {
                const encoded = encodeTrialParams(t.params, parsedParams);
                encoded.forEach(function(v) { flatVec.push_back(v); });
                objVec.push_back(Number(t.objective));
            });
            const res = wasmModuleInstance.suggestNextTrial(
                paramVec,
                flatVec,
                objVec,
                experiment.direction === 'maximize',
                config.surrogate || 'council',
                config.acquisition || 'lbfgs',
                Number(config.lengthScale) || 0.3,
                Number(config.polyDegree) || 2,
                Number(config.polyPoints) || 15,
                Number(config.warmupTrials) || 5
            );
            const rawVals = [];
            for (let i = 0; i < res.values.size(); i++) {
                rawVals.push(res.values.get(i));
            }
            res.values.delete();
            return {
                params: decodeParamValues(rawVals, parsedParams),
                predictedMean: res.used_warmup ? null : res.predicted_mean,
                predictedStd: res.used_warmup ? null : res.predicted_std,
                usedWarmup: res.used_warmup
            };
        } finally {
            paramVec.delete();
            flatVec.delete();
            objVec.delete();
        }
    }
    globalScope.OptimaEngine = {
        init: init,
        isReady: isReady,
        parseParamConfig: parseParamConfig,
        suggestNext: suggestNext
    };
})(window);