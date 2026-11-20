(function() {
    const STORAGE_KEY_EXPERIMENTS = 'experiments';
    const STORAGE_KEY_SETTINGS = 'settings';
    const SURROGATE_OPTIONS = [
        { id: 'gp_rbf', label: 'GP (RBF)' },
        { id: 'gp_matern52', label: 'GP (Matérn 5/2)' },
        { id: 'gp_matern32', label: 'GP (Matérn 3/2)' },
        { id: 'gp_matern12', label: 'GP (Matérn 1/2)' },
        { id: 'gp_rq', label: 'GP (Rational Quadratic)' },
        { id: 'poly_1', label: 'Polynomial (Deg 1)' },
        { id: 'poly_2', label: 'Polynomial (Deg 2)' },
        { id: 'poly_3', label: 'Polynomial (Deg 3)' },
        { id: 'poly_4', label: 'Polynomial (Deg 4)' }
    ];
    const DEFAULT_SETTINGS = {
        defaultDirection: 'minimize',
        defaultSurrogates: ['gp_rbf', 'poly_2'],
        defaultAcquisition: 'lbfgs',
        defaultWarmupTrials: 5,
        defaultLengthScale: 0.3,
        defaultPolyPoints: 15
    };
    let activeExperimentId = null;
    let editingTrialIndex = null;
    let activeViewMode = 'optimize';
    let lastArbitraryPrediction = null;
    let lastArbitraryQueryValues = null;
    let graphConfigState = {
        points1D: 25,
        pointsPerAxis2D: 15,
        showBand: true,
        mode2D: '3d',
        yaw: -0.66,
        pitch: 0.45
    };
    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }
    function resolveLegacySurrogates(legacySurrogate, polyDegree) {
        const deg = Math.max(1, Number(polyDegree) || 2);
        if (legacySurrogate === 'gaussian_process') return ['gp_rbf'];
        if (legacySurrogate === 'polynomial') return ['poly_' + deg];
        return ['gp_rbf', 'poly_' + deg];
    }
    function loadSettingsFromStorage() {
        const raw = localStorage.getItem(STORAGE_KEY_SETTINGS);
        if (!raw) {
            return Object.assign({}, DEFAULT_SETTINGS, { defaultSurrogates: DEFAULT_SETTINGS.defaultSurrogates.slice() });
        }
        try {
            const parsed = JSON.parse(raw);
            const merged = Object.assign({}, DEFAULT_SETTINGS, parsed);
            if (Array.isArray(parsed.defaultSurrogates) && parsed.defaultSurrogates.length > 0) {
                merged.defaultSurrogates = parsed.defaultSurrogates.slice();
            } else {
                merged.defaultSurrogates = resolveLegacySurrogates(parsed.defaultSurrogate, 2);
            }
            return merged;
        } catch (e) {
            return Object.assign({}, DEFAULT_SETTINGS, { defaultSurrogates: DEFAULT_SETTINGS.defaultSurrogates.slice() });
        }
    }
    function normalizeExperiment(exp) {
        const settings = loadSettingsFromStorage();
        const cfg = exp.config || {};
        let surrogates;
        if (Array.isArray(cfg.surrogates) && cfg.surrogates.length > 0) {
            surrogates = cfg.surrogates.map(String);
        } else if (cfg.surrogate) {
            surrogates = resolveLegacySurrogates(cfg.surrogate, cfg.polyDegree);
        } else {
            surrogates = settings.defaultSurrogates.slice();
        }
        return {
            id: exp.id || generateExperimentId(),
            name: exp.name || 'untitled',
            objective: exp.objective || 'objective',
            direction: exp.direction === 'maximize' ? 'maximize' : 'minimize',
            status: exp.status || 'idle',
            config: {
                surrogates: surrogates,
                acquisition: cfg.acquisition || settings.defaultAcquisition,
                warmupTrials: Number(cfg.warmupTrials) || settings.defaultWarmupTrials,
                lengthScale: Number(cfg.lengthScale) || settings.defaultLengthScale,
                polyDegree: Number(cfg.polyDegree) || 2,
                polyPoints: Number(cfg.polyPoints) || settings.defaultPolyPoints
            },
            parameters: Array.isArray(exp.parameters) ? exp.parameters.map(function(p) {
                return {
                    name: p.name,
                    type: p.type || 'float',
                    range: p.range || '0, 1',
                    logScale: Boolean(p.logScale)
                };
            }) : [],
            trials: Array.isArray(exp.trials) ? exp.trials : [],
            pendingSuggestion: exp.pendingSuggestion || null
        };
    }
    function loadExperimentsFromStorage() {
        const raw = localStorage.getItem(STORAGE_KEY_EXPERIMENTS);
        if (!raw) {
            return [];
        }
        try {
            const parsed = JSON.parse(raw);
            return Array.isArray(parsed) ? parsed.map(normalizeExperiment) : [];
        } catch (e) {
            return [];
        }
    }
    function saveExperimentsToStorage(experiments) {
        localStorage.setItem(STORAGE_KEY_EXPERIMENTS, JSON.stringify(experiments));
    }
    function generateExperimentId() {
        return Date.now().toString(36) + Math.random().toString(36).substring(2, 6);
    }
    function formatSingleSurrogateLabel(id) {
        if (id === 'gp_rbf' || id === 'gaussian_process') return 'GP (RBF)';
        if (id === 'gp_matern52') return 'GP (Matérn 5/2)';
        if (id === 'gp_matern32') return 'GP (Matérn 3/2)';
        if (id === 'gp_matern12') return 'GP (Matérn 1/2)';
        if (id === 'gp_rq') return 'GP (RQ)';
        if (id.indexOf('poly_') === 0) return 'Poly (Deg ' + id.slice(5) + ')';
        if (id === 'polynomial') return 'Poly';
        return id;
    }
    function formatSurrogateLabel(configOrList) {
        const list = Array.isArray(configOrList)
            ? configOrList
            : (configOrList && Array.isArray(configOrList.surrogates) ? configOrList.surrogates : ['gp_rbf']);
        if (list.length === 1) {
            return formatSingleSurrogateLabel(list[0]);
        }
        return 'Council (' + list.map(formatSingleSurrogateLabel).join(' + ') + ')';
    }
    function formatAcquisitionLabel(acq) {
        if (acq === 'perihelion') return 'Perihelion';
        if (acq === 'random') return 'Random EI';
        return 'L-BFGS';
    }
    function formatStatusLabel(status) {
        const s = String(status || 'idle');
        return s.charAt(0).toUpperCase() + s.slice(1);
    }
    function findBestTrial(exp) {
        if (!exp.trials || exp.trials.length === 0) {
            return null;
        }
        const maximize = exp.direction === 'maximize';
        let bestIdx = 0;
        for (let i = 1; i < exp.trials.length; i++) {
            const val = Number(exp.trials[i].objective);
            const bestVal = Number(exp.trials[bestIdx].objective);
            if (maximize ? (val > bestVal) : (val < bestVal)) {
                bestIdx = i;
            }
        }
        return { index: bestIdx, trial: exp.trials[bestIdx] };
    }
    function readParametersFromContainer(container) {
        const paramRows = container.querySelectorAll('.param-row');
        const parameters = [];
        for (const row of paramRows) {
            const nameEl = row.querySelector('.param-name');
            const typeEl = row.querySelector('.param-type');
            const rangeEl = row.querySelector('.param-range');
            const scaleEl = row.querySelector('.param-scale');
            if (!nameEl.value.trim() || !rangeEl.value.trim()) {
                continue;
            }
            const paramType = typeEl.value;
            const logScale = paramType !== 'categorical' && scaleEl && scaleEl.value === 'log';
            parameters.push({
                name: nameEl.value.trim(),
                originalName: row.dataset.originalName || '',
                type: paramType,
                range: rangeEl.value.trim(),
                logScale: logScale
            });
        }
        return parameters;
    }
    function renderSurrogateCheckboxes(container, selectedIds, onChangeCallback) {
        if (!container) {
            return;
        }
        container.innerHTML = '';
        const standardIds = SURROGATE_OPTIONS.map(function(o) { return o.id; });
        let customDegree = 5;
        let customChecked = false;
        selectedIds.forEach(function(id) {
            if (standardIds.indexOf(id) < 0 && id.indexOf('poly_') === 0) {
                const d = parseInt(id.slice(5), 10);
                if (!Number.isNaN(d) && d >= 1) {
                    customDegree = d;
                    customChecked = true;
                }
            }
        });
        SURROGATE_OPTIONS.forEach(function(opt) {
            const label = document.createElement('label');
            label.className = 'surrogate-checkbox-item';
            const cb = document.createElement('input');
            cb.type = 'checkbox';
            cb.value = opt.id;
            cb.checked = selectedIds.indexOf(opt.id) >= 0;
            if (onChangeCallback) {
                cb.addEventListener('change', function() {
                    onChangeCallback(cb);
                });
            }
            label.appendChild(cb);
            label.appendChild(document.createTextNode(' ' + opt.label));
            container.appendChild(label);
        });
        const customLabel = document.createElement('label');
        customLabel.className = 'surrogate-checkbox-item surrogate-custom-item';
        const customCb = document.createElement('input');
        customCb.type = 'checkbox';
        customCb.className = 'custom-poly-checkbox';
        customCb.checked = customChecked;
        const degInput = document.createElement('input');
        degInput.type = 'number';
        degInput.min = '1';
        degInput.max = '10';
        degInput.value = String(customDegree);
        degInput.className = 'custom-poly-degree-input';
        if (onChangeCallback) {
            customCb.addEventListener('change', function() {
                onChangeCallback(customCb);
            });
            degInput.addEventListener('change', function() {
                if (customCb.checked) {
                    onChangeCallback(degInput);
                }
            });
            degInput.addEventListener('click', function(e) {
                e.stopPropagation();
            });
        }
        customLabel.appendChild(customCb);
        customLabel.appendChild(document.createTextNode(' Polynomial (Deg '));
        customLabel.appendChild(degInput);
        customLabel.appendChild(document.createTextNode(')'));
        container.appendChild(customLabel);
    }
    function readSurrogateCheckboxes(container) {
        if (!container) {
            return [];
        }
        const selected = [];
        container.querySelectorAll('input[type="checkbox"]:not(.custom-poly-checkbox)').forEach(function(cb) {
            if (cb.checked) {
                selected.push(cb.value);
            }
        });
        const customCb = container.querySelector('.custom-poly-checkbox');
        const degInput = container.querySelector('.custom-poly-degree-input');
        if (customCb && customCb.checked && degInput) {
            const deg = Math.max(1, Math.min(10, parseInt(degInput.value, 10) || 5));
            const id = 'poly_' + deg;
            if (selected.indexOf(id) < 0) {
                selected.push(id);
            }
        }
        return selected;
    }
    function createExperimentFromForm() {
        const nameInput = document.getElementById('expName');
        const objectiveInput = document.getElementById('objectiveName');
        const directionSelect = document.getElementById('direction');
        const acquisitionSelect = document.getElementById('acquisitionType');
        const warmupInput = document.getElementById('warmupTrials');
        const lengthScaleInput = document.getElementById('rbfLengthScale');
        const polyPointsInput = document.getElementById('polyPoints');
        const surrogatesContainer = document.getElementById('newSurrogatesContainer');
        const container = document.getElementById('paramsContainer');
        if (!nameInput.value.trim() || !objectiveInput.value.trim()) {
            return null;
        }
        const selectedSurrogates = readSurrogateCheckboxes(surrogatesContainer);
        if (selectedSurrogates.length === 0) {
            alert('Please select at least one surrogate model.');
            return null;
        }
        const parameters = readParametersFromContainer(container).map(function(p) {
            return {
                name: p.name,
                type: p.type,
                range: p.range,
                logScale: p.logScale
            };
        });
        if (parameters.length === 0) {
            return null;
        }
        return normalizeExperiment({
            id: generateExperimentId(),
            name: nameInput.value.trim(),
            objective: objectiveInput.value.trim(),
            direction: directionSelect.value,
            status: 'idle',
            config: {
                surrogates: selectedSurrogates,
                acquisition: acquisitionSelect.value,
                warmupTrials: Math.max(1, parseInt(warmupInput.value, 10) || 5),
                lengthScale: Math.max(0.01, parseFloat(lengthScaleInput.value) || 0.3),
                polyPoints: Math.max(4, parseInt(polyPointsInput.value, 10) || 15)
            },
            parameters: parameters,
            trials: []
        });
    }
    function applyDefaultsToForm() {
        const settings = loadSettingsFromStorage();
        const directionSelect = document.getElementById('direction');
        const acquisitionSelect = document.getElementById('acquisitionType');
        const warmupInput = document.getElementById('warmupTrials');
        const lengthScaleInput = document.getElementById('rbfLengthScale');
        const polyPointsInput = document.getElementById('polyPoints');
        const surrogatesContainer = document.getElementById('newSurrogatesContainer');
        if (directionSelect) directionSelect.value = settings.defaultDirection;
        if (acquisitionSelect) acquisitionSelect.value = settings.defaultAcquisition;
        if (warmupInput) warmupInput.value = settings.defaultWarmupTrials;
        if (lengthScaleInput) lengthScaleInput.value = settings.defaultLengthScale;
        if (polyPointsInput) polyPointsInput.value = settings.defaultPolyPoints;
        if (surrogatesContainer) renderSurrogateCheckboxes(surrogatesContainer, settings.defaultSurrogates, null);
    }
    function addParameterRowToContainer(container, isFirst, initialParam) {
        if (!container) {
            return;
        }
        const row = document.createElement('div');
        row.className = 'form-row param-row';
        if (initialParam && initialParam.name) {
            row.dataset.originalName = initialParam.name;
        }
        row.innerHTML =
            '<div class="form-group">' +
                '<label>Name</label>' +
                '<input type="text" placeholder="lr" class="param-name" required>' +
            '</div>' +
            '<div class="form-group">' +
                '<label>Type</label>' +
                '<select class="param-type" required>' +
                    '<option value="float">Float</option>' +
                    '<option value="int">Int</option>' +
                    '<option value="categorical">Categorical</option>' +
                '</select>' +
            '</div>' +
            '<div class="form-group">' +
                '<label>Range / Choices</label>' +
                '<input type="text" placeholder="0.0001, 0.1" class="param-range" required>' +
            '</div>' +
            '<div class="form-group param-scale-group">' +
                '<label>Scale</label>' +
                '<select class="param-scale">' +
                    '<option value="linear">Linear</option>' +
                    '<option value="log">Log</option>' +
                '</select>' +
            '</div>';
        const nameInput = row.querySelector('.param-name');
        const typeSelect = row.querySelector('.param-type');
        const rangeInput = row.querySelector('.param-range');
        const scaleSelect = row.querySelector('.param-scale');
        function syncTypeBehavior() {
            if (typeSelect.value === 'categorical') {
                rangeInput.placeholder = 'adam, sgd, rmsprop';
                scaleSelect.value = 'linear';
                scaleSelect.disabled = true;
            } else if (typeSelect.value === 'int') {
                rangeInput.placeholder = '16, 512';
                scaleSelect.disabled = false;
            } else {
                rangeInput.placeholder = '0.0001, 0.1';
                scaleSelect.disabled = false;
            }
        }
        if (initialParam) {
            nameInput.value = initialParam.name || '';
            typeSelect.value = initialParam.type || 'float';
            rangeInput.value = initialParam.range || '';
            scaleSelect.value = initialParam.logScale ? 'log' : 'linear';
        }
        syncTypeBehavior();
        typeSelect.addEventListener('change', syncTypeBehavior);
        if (!isFirst) {
            const removeWrap = document.createElement('div');
            removeWrap.className = 'form-group param-remove-group';
            const removeLabel = document.createElement('label');
            removeLabel.innerHTML = '&nbsp;';
            const removeBtn = document.createElement('button');
            removeBtn.type = 'button';
            removeBtn.className = 'btn btn-danger';
            removeBtn.textContent = 'Remove';
            removeBtn.addEventListener('click', function() {
                row.remove();
            });
            removeWrap.appendChild(removeLabel);
            removeWrap.appendChild(removeBtn);
            row.appendChild(removeWrap);
        }
        container.appendChild(row);
    }
    function openNewExperimentMenu() {
        const menu = document.getElementById('newExperimentMenu');
        const form = document.getElementById('experimentForm');
        const container = document.getElementById('paramsContainer');
        if (!menu || !form || !container) {
            return;
        }
        form.reset();
        applyDefaultsToForm();
        container.innerHTML = '';
        addParameterRowToContainer(container, true, null);
        menu.hidden = false;
    }
    function closeNewExperimentMenu() {
        const menu = document.getElementById('newExperimentMenu');
        if (menu) {
            menu.hidden = true;
        }
    }
    function addExperimentAndOpen() {
        const newExperiment = createExperimentFromForm();
        if (!newExperiment) {
            if (readSurrogateCheckboxes(document.getElementById('newSurrogatesContainer')).length > 0) {
                alert('Please fill in all required fields and add at least one valid parameter.');
            }
            return;
        }
        const experiments = loadExperimentsFromStorage();
        experiments.push(newExperiment);
        saveExperimentsToStorage(experiments);
        closeNewExperimentMenu();
        openExperimentView(newExperiment.id);
    }
    function updateExperimentById(id, updater) {
        const experiments = loadExperimentsFromStorage();
        const idx = experiments.findIndex(function(e) { return e.id === id; });
        if (idx < 0) {
            return;
        }
        experiments[idx] = updater(experiments[idx]);
        saveExperimentsToStorage(experiments);
    }
    function deleteExperimentById(id) {
        let experiments = loadExperimentsFromStorage();
        experiments = experiments.filter(function(exp) { return exp.id !== id; });
        saveExperimentsToStorage(experiments);
        if (activeExperimentId === id) {
            closeExperimentView();
        } else {
            renderExperimentsSummaryList();
        }
    }
    function openEditExperimentMenu(id) {
        const experiments = loadExperimentsFromStorage();
        const exp = experiments.find(function(e) { return e.id === id; });
        const menu = document.getElementById('editExperimentMenu');
        if (!exp || !menu) {
            return;
        }
        document.getElementById('editExpName').value = exp.name;
        document.getElementById('editObjectiveName').value = exp.objective;
        document.getElementById('editDirection').value = exp.direction;
        document.getElementById('editStatus').value = exp.status;
        document.getElementById('editAcquisitionType').value = exp.config.acquisition;
        document.getElementById('editWarmupTrials').value = exp.config.warmupTrials;
        document.getElementById('editRbfLengthScale').value = exp.config.lengthScale;
        document.getElementById('editPolyPoints').value = exp.config.polyPoints;
        renderSurrogateCheckboxes(document.getElementById('editSurrogatesContainer'), exp.config.surrogates, null);
        const container = document.getElementById('editParamsContainer');
        container.innerHTML = '';
        if (exp.parameters.length === 0) {
            addParameterRowToContainer(container, true, null);
        } else {
            exp.parameters.forEach(function(param, index) {
                addParameterRowToContainer(container, index === 0, param);
            });
        }
        menu.hidden = false;
    }
    function closeEditExperimentMenu() {
        const menu = document.getElementById('editExperimentMenu');
        if (menu) {
            menu.hidden = true;
        }
    }
    function saveEditedExperimentInputs() {
        if (!activeExperimentId) {
            return;
        }
        const nameVal = document.getElementById('editExpName').value.trim();
        const objVal = document.getElementById('editObjectiveName').value.trim();
        const dirVal = document.getElementById('editDirection').value;
        const statusVal = document.getElementById('editStatus').value;
        const selectedSurrogates = readSurrogateCheckboxes(document.getElementById('editSurrogatesContainer'));
        const acqVal = document.getElementById('editAcquisitionType').value;
        const warmupVal = Math.max(1, parseInt(document.getElementById('editWarmupTrials').value, 10) || 5);
        const lengthScaleVal = Math.max(0.01, parseFloat(document.getElementById('editRbfLengthScale').value) || 0.3);
        const polyPointsVal = Math.max(4, parseInt(document.getElementById('editPolyPoints').value, 10) || 15);
        const container = document.getElementById('editParamsContainer');
        const rawParams = readParametersFromContainer(container);
        if (selectedSurrogates.length === 0) {
            alert('Please select at least one surrogate model.');
            return;
        }
        if (!nameVal || !objVal || rawParams.length === 0) {
            alert('Please fill in experiment name, objective, and at least one valid parameter.');
            return;
        }
        const renameMap = {};
        const cleanedParams = rawParams.map(function(p) {
            if (p.originalName && p.originalName !== p.name) {
                renameMap[p.originalName] = p.name;
            }
            return {
                name: p.name,
                type: p.type,
                range: p.range,
                logScale: p.logScale
            };
        });
        updateExperimentById(activeExperimentId, function(cur) {
            cur.name = nameVal;
            cur.objective = objVal;
            cur.direction = dirVal === 'maximize' ? 'maximize' : 'minimize';
            cur.status = statusVal;
            cur.config = {
                surrogates: selectedSurrogates,
                acquisition: acqVal,
                warmupTrials: warmupVal,
                lengthScale: lengthScaleVal,
                polyDegree: cur.config.polyDegree || 2,
                polyPoints: polyPointsVal
            };
            cur.parameters = cleanedParams;
            cur.trials = (cur.trials || []).map(function(trial) {
                const updatedParams = {};
                const oldParams = trial.params || {};
                Object.keys(renameMap).forEach(function(oldKey) {
                    if (Object.prototype.hasOwnProperty.call(oldParams, oldKey)) {
                        updatedParams[renameMap[oldKey]] = oldParams[oldKey];
                    }
                });
                cleanedParams.forEach(function(p) {
                    if (Object.prototype.hasOwnProperty.call(updatedParams, p.name)) {
                        return;
                    }
                    if (Object.prototype.hasOwnProperty.call(oldParams, p.name)) {
                        updatedParams[p.name] = oldParams[p.name];
                    }
                });
                return Object.assign({}, trial, { params: updatedParams });
            });
            cur.pendingSuggestion = null;
            return cur;
        });
        editingTrialIndex = null;
        lastArbitraryPrediction = null;
        lastArbitraryQueryValues = null;
        if (cleanedParams.length > 2 && activeViewMode === 'graph') {
            activeViewMode = 'optimize';
        }
        closeEditExperimentMenu();
        renderActiveExperiment();
    }
    function drawConvergenceChart(canvas, exp) {
        if (!canvas || !exp.trials || exp.trials.length === 0) {
            return;
        }
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        ctx.clearRect(0, 0, width, height);
        const padLeft = 54;
        const padRight = 18;
        const padTop = 16;
        const padBottom = 26;
        const plotW = width - padLeft - padRight;
        const plotH = height - padTop - padBottom;
        const objectives = exp.trials.map(function(t) { return Number(t.objective); });
        const maximize = exp.direction === 'maximize';
        const bestCurve = [];
        let runningBest = objectives[0];
        for (let i = 0; i < objectives.length; i++) {
            if (maximize ? (objectives[i] > runningBest) : (objectives[i] < runningBest)) {
                runningBest = objectives[i];
            }
            bestCurve.push(runningBest);
        }
        let minY = Math.min.apply(null, objectives);
        let maxY = Math.max.apply(null, objectives);
        if (Math.abs(maxY - minY) < 1e-12) {
            minY -= 1.0;
            maxY += 1.0;
        }
        const yPad = (maxY - minY) * 0.08;
        minY -= yPad;
        maxY += yPad;
        function xForIndex(i) {
            if (objectives.length === 1) {
                return padLeft + plotW * 0.5;
            }
            return padLeft + (i / (objectives.length - 1)) * plotW;
        }
        function yForValue(v) {
            return padTop + (1.0 - (v - minY) / (maxY - minY)) * plotH;
        }
        ctx.fillStyle = '#faf9f5';
        ctx.fillRect(padLeft, padTop, plotW, plotH);
        ctx.strokeStyle = '#dcd8c8';
        ctx.lineWidth = 1;
        for (let g = 0; g <= 4; g++) {
            const gy = padTop + (g / 4) * plotH;
            ctx.beginPath();
            ctx.moveTo(padLeft, gy);
            ctx.lineTo(padLeft + plotW, gy);
            ctx.stroke();
            const labelVal = maxY - (g / 4) * (maxY - minY);
            ctx.fillStyle = '#716f64';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'right';
            ctx.fillText(labelVal.toPrecision(3), padLeft - 6, gy + 4);
        }
        ctx.strokeStyle = '#999999';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (let i = 0; i < objectives.length; i++) {
            const x = xForIndex(i);
            const y = yForValue(objectives[i]);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.strokeStyle = '#316ac5';
        ctx.lineWidth = 2.5;
        ctx.beginPath();
        for (let i = 0; i < bestCurve.length; i++) {
            const x = xForIndex(i);
            const y = yForValue(bestCurve[i]);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        for (let i = 0; i < objectives.length; i++) {
            const x = xForIndex(i);
            const y = yForValue(objectives[i]);
            ctx.fillStyle = '#316ac5';
            ctx.beginPath();
            ctx.arc(x, y, 3.5, 0, Math.PI * 2);
            ctx.fill();
        }
        ctx.fillStyle = '#716f64';
        ctx.font = '11px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Trials (1 .. ' + objectives.length + ')', padLeft + plotW * 0.5, height - 6);
    }
    function buildEquidistantAxisValues(parsedParam, count) {
        if (parsedParam.type === 'categorical') {
            return parsedParam.categories.map(function(cat, index) {
                return { numeric: index, label: cat };
            });
        }
        const safeCount = Math.max(2, count);
        const vals = [];
        for (let i = 0; i < safeCount; i++) {
            const t = i / (safeCount - 1);
            const num = parsedParam.lower + t * (parsedParam.upper - parsedParam.lower);
            vals.push({ numeric: num, label: Number(num.toPrecision(4)).toString() });
        }
        return vals;
    }
    function interpolateColorForRatio(t) {
        const clamped = Math.max(0, Math.min(1, t));
        const r = Math.round(49 + clamped * 170);
        const g = Math.round(106 + (1 - Math.abs(clamped - 0.5) * 2) * 70 - clamped * 40);
        const b = Math.round(197 - clamped * 155);
        return 'rgb(' + r + ',' + Math.max(40, g) + ',' + b + ')';
    }
    function encodeSingleTrialNumericValue(rawVal, parsedParam) {
        if (parsedParam.type === 'categorical') {
            if (typeof rawVal === 'number' && !Number.isNaN(rawVal)) {
                return rawVal;
            }
            const idx = parsedParam.categories.indexOf(String(rawVal));
            return idx >= 0 ? idx : 0;
        }
        const num = parseFloat(rawVal);
        return Number.isNaN(num) ? (parsedParam.lower + parsedParam.upper) * 0.5 : num;
    }
    function draw1DSurrogateGraph(canvas, exp, parsedParam, axisPoints, predictions, showBand, hoverIndex) {
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        ctx.clearRect(0, 0, width, height);
        const padLeft = 64;
        const padRight = 24;
        const padTop = 22;
        const padBottom = 42;
        const plotW = width - padLeft - padRight;
        const plotH = height - padTop - padBottom;
        const allY = [];
        predictions.forEach(function(p) {
            if (showBand) {
                allY.push(p.mean - p.stdDev);
                allY.push(p.mean + p.stdDev);
            } else {
                allY.push(p.mean);
            }
        });
        (exp.trials || []).forEach(function(t) {
            allY.push(Number(t.objective));
        });
        let minY = Math.min.apply(null, allY);
        let maxY = Math.max.apply(null, allY);
        if (Math.abs(maxY - minY) < 1e-12) {
            minY -= 1.0;
            maxY += 1.0;
        }
        const yPad = (maxY - minY) * 0.08;
        minY -= yPad;
        maxY += yPad;
        const minX = axisPoints[0].numeric;
        const maxX = axisPoints[axisPoints.length - 1].numeric;
        const spanX = Math.max(1e-12, maxX - minX);
        function px(xVal) {
            return padLeft + ((xVal - minX) / spanX) * plotW;
        }
        function py(yVal) {
            return padTop + (1.0 - (yVal - minY) / (maxY - minY)) * plotH;
        }
        ctx.fillStyle = '#faf9f5';
        ctx.fillRect(padLeft, padTop, plotW, plotH);
        ctx.strokeStyle = '#dcd8c8';
        ctx.lineWidth = 1;
        for (let g = 0; g <= 4; g++) {
            const gy = padTop + (g / 4) * plotH;
            ctx.beginPath();
            ctx.moveTo(padLeft, gy);
            ctx.lineTo(padLeft + plotW, gy);
            ctx.stroke();
            const labelVal = maxY - (g / 4) * (maxY - minY);
            ctx.fillStyle = '#716f64';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'right';
            ctx.fillText(labelVal.toPrecision(3), padLeft - 8, gy + 4);
        }
        const xTicks = Math.min(6, axisPoints.length);
        for (let t = 0; t < xTicks; t++) {
            const idx = xTicks === 1 ? 0 : Math.round((t / (xTicks - 1)) * (axisPoints.length - 1));
            const pt = axisPoints[idx];
            const gx = px(pt.numeric);
            ctx.strokeStyle = '#ece9dc';
            ctx.beginPath();
            ctx.moveTo(gx, padTop);
            ctx.lineTo(gx, padTop + plotH);
            ctx.stroke();
            ctx.fillStyle = '#716f64';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText(pt.label, gx, padTop + plotH + 16);
        }
        ctx.strokeStyle = '#aca899';
        ctx.strokeRect(padLeft, padTop, plotW, plotH);
        if (showBand) {
            ctx.fillStyle = 'rgba(49, 106, 197, 0.16)';
            ctx.beginPath();
            for (let i = 0; i < axisPoints.length; i++) {
                const x = px(axisPoints[i].numeric);
                const y = py(predictions[i].mean + predictions[i].stdDev);
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            for (let i = axisPoints.length - 1; i >= 0; i--) {
                const x = px(axisPoints[i].numeric);
                const y = py(predictions[i].mean - predictions[i].stdDev);
                ctx.lineTo(x, y);
            }
            ctx.closePath();
            ctx.fill();
        }
        ctx.strokeStyle = '#316ac5';
        ctx.lineWidth = 2.2;
        ctx.beginPath();
        for (let i = 0; i < axisPoints.length; i++) {
            const x = px(axisPoints[i].numeric);
            const y = py(predictions[i].mean);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        for (let i = 0; i < axisPoints.length; i++) {
            const x = px(axisPoints[i].numeric);
            const y = py(predictions[i].mean);
            ctx.fillStyle = hoverIndex === i ? '#000000' : '#316ac5';
            ctx.beginPath();
            ctx.arc(x, y, hoverIndex === i ? 4.5 : 2.5, 0, Math.PI * 2);
            ctx.fill();
        }
        (exp.trials || []).forEach(function(trial) {
            const rawVal = trial.params ? trial.params[parsedParam.name] : undefined;
            const numX = encodeSingleTrialNumericValue(rawVal, parsedParam);
            const x = px(numX);
            const y = py(Number(trial.objective));
            ctx.fillStyle = '#b22222';
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.arc(x, y, 5, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();
        });
        ctx.fillStyle = '#333333';
        ctx.font = '12px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(parsedParam.name + ' (' + axisPoints.length + ' equidistant points)', padLeft + plotW * 0.5, height - 6);
        ctx.save();
        ctx.translate(15, padTop + plotH * 0.5);
        ctx.rotate(-Math.PI / 2);
        ctx.fillText(exp.objective, 0, 0);
        ctx.restore();
    }
    function draw2DHeatmapGraph(canvas, exp, pX, pY, axisX, axisY, gridPredictions, hoverCell) {
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        ctx.clearRect(0, 0, width, height);
        const padLeft = 72;
        const padRight = 100;
        const padTop = 22;
        const padBottom = 46;
        const plotW = width - padLeft - padRight;
        const plotH = height - padTop - padBottom;
        const nx = axisX.length;
        const ny = axisY.length;
        let minZ = Infinity;
        let maxZ = -Infinity;
        gridPredictions.forEach(function(p) {
            if (p.mean < minZ) minZ = p.mean;
            if (p.mean > maxZ) maxZ = p.mean;
        });
        if (Math.abs(maxZ - minZ) < 1e-12) {
            minZ -= 1.0;
            maxZ += 1.0;
        }
        const cellW = plotW / nx;
        const cellH = plotH / ny;
        for (let ix = 0; ix < nx; ix++) {
            for (let iy = 0; iy < ny; iy++) {
                const pred = gridPredictions[ix * ny + iy];
                const t = (pred.mean - minZ) / (maxZ - minZ);
                ctx.fillStyle = interpolateColorForRatio(t);
                const xPos = padLeft + ix * cellW;
                const yPos = padTop + (ny - 1 - iy) * cellH;
                ctx.fillRect(xPos, yPos, Math.ceil(cellW), Math.ceil(cellH));
                if (hoverCell && hoverCell.ix === ix && hoverCell.iy === iy) {
                    ctx.strokeStyle = '#000000';
                    ctx.lineWidth = 2;
                    ctx.strokeRect(xPos, yPos, cellW, cellH);
                }
            }
        }
        ctx.strokeStyle = '#aca899';
        ctx.lineWidth = 1;
        ctx.strokeRect(padLeft, padTop, plotW, plotH);
        const minX = axisX[0].numeric;
        const spanX = Math.max(1e-12, axisX[nx - 1].numeric - minX);
        const minY = axisY[0].numeric;
        const spanY = Math.max(1e-12, axisY[ny - 1].numeric - minY);
        (exp.trials || []).forEach(function(trial) {
            const tx = encodeSingleTrialNumericValue(trial.params ? trial.params[pX.name] : undefined, pX);
            const ty = encodeSingleTrialNumericValue(trial.params ? trial.params[pY.name] : undefined, pY);
            const px = padLeft + ((tx - minX) / spanX) * plotW;
            const py = padTop + (1.0 - (ty - minY) / spanY) * plotH;
            ctx.fillStyle = '#ffffff';
            ctx.strokeStyle = '#000000';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.arc(px, py, 4.5, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();
        });
        const xTicks = Math.min(5, nx);
        for (let i = 0; i < xTicks; i++) {
            const idx = xTicks === 1 ? 0 : Math.round((i / (xTicks - 1)) * (nx - 1));
            const cx = padLeft + (idx + 0.5) * cellW;
            ctx.fillStyle = '#716f64';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText(axisX[idx].label, cx, padTop + plotH + 16);
        }
        const yTicks = Math.min(5, ny);
        for (let j = 0; j < yTicks; j++) {
            const idx = yTicks === 1 ? 0 : Math.round((j / (yTicks - 1)) * (ny - 1));
            const cy = padTop + (ny - 1 - idx + 0.5) * cellH;
            ctx.fillStyle = '#716f64';
            ctx.font = '11px sans-serif';
            ctx.textAlign = 'right';
            ctx.fillText(axisY[idx].label, padLeft - 8, cy + 4);
        }
        const barX = padLeft + plotW + 18;
        const barW = 16;
        for (let s = 0; s < plotH; s++) {
            const ratio = 1.0 - s / Math.max(1, plotH - 1);
            ctx.fillStyle = interpolateColorForRatio(ratio);
            ctx.fillRect(barX, padTop + s, barW, 1);
        }
        ctx.strokeStyle = '#aca899';
        ctx.strokeRect(barX, padTop, barW, plotH);
        ctx.fillStyle = '#333333';
        ctx.font = '11px sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText(maxZ.toPrecision(3), barX + barW + 6, padTop + 10);
        ctx.fillText(((maxZ + minZ) * 0.5).toPrecision(3), barX + barW + 6, padTop + plotH * 0.5 + 4);
        ctx.fillText(minZ.toPrecision(3), barX + barW + 6, padTop + plotH);
        ctx.textAlign = 'center';
        ctx.font = '12px sans-serif';
        ctx.fillText(pX.name, padLeft + plotW * 0.5, height - 8);
        ctx.save();
        ctx.translate(16, padTop + plotH * 0.5);
        ctx.rotate(-Math.PI / 2);
        ctx.fillText(pY.name, 0, 0);
        ctx.restore();
    }
    function draw3DSurfaceGraph(canvas, exp, pX, pY, axisX, axisY, gridPredictions, yaw, pitch) {
        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;
        ctx.clearRect(0, 0, width, height);
        ctx.fillStyle = '#faf9f5';
        ctx.fillRect(0, 0, width, height);
        const nx = axisX.length;
        const ny = axisY.length;
        let minZ = Infinity;
        let maxZ = -Infinity;
        gridPredictions.forEach(function(p) {
            if (p.mean < minZ) minZ = p.mean;
            if (p.mean > maxZ) maxZ = p.mean;
        });
        (exp.trials || []).forEach(function(t) {
            const val = Number(t.objective);
            if (val < minZ) minZ = val;
            if (val > maxZ) maxZ = val;
        });
        if (Math.abs(maxZ - minZ) < 1e-12) {
            minZ -= 1.0;
            maxZ += 1.0;
        }
        const minX = axisX[0].numeric;
        const spanX = Math.max(1e-12, axisX[nx - 1].numeric - minX);
        const minY = axisY[0].numeric;
        const spanY = Math.max(1e-12, axisY[ny - 1].numeric - minY);
        const spanZ = Math.max(1e-12, maxZ - minZ);
        const cosYaw = Math.cos(yaw);
        const sinYaw = Math.sin(yaw);
        const cosPitch = Math.cos(pitch);
        const sinPitch = Math.sin(pitch);
        const scale = Math.min(width, height) * 0.38;
        const centerX = width * 0.5;
        const centerY = height * 0.53;
        function projectPoint(nxVal, nyVal, nzVal) {
            const x = nxVal - 0.5;
            const y = nyVal - 0.5;
            const z = nzVal - 0.5;
            const x1 = x * cosYaw - y * sinYaw;
            const y1 = x * sinYaw + y * cosYaw;
            const y2 = y1 * cosPitch - z * sinPitch;
            const z2 = y1 * sinPitch + z * cosPitch;
            const persp = 3.2 / (3.2 - y2 * 0.65);
            return {
                x: centerX + x1 * scale * persp,
                y: centerY - z2 * scale * persp,
                depth: y2
            };
        }
        const corners = [
            projectPoint(0, 0, 0),
            projectPoint(1, 0, 0),
            projectPoint(1, 1, 0),
            projectPoint(0, 1, 0),
            projectPoint(0, 0, 1)
        ];
        ctx.strokeStyle = '#9b9789';
        ctx.lineWidth = 1.2;
        ctx.beginPath();
        ctx.moveTo(corners[0].x, corners[0].y);
        ctx.lineTo(corners[1].x, corners[1].y);
        ctx.lineTo(corners[2].x, corners[2].y);
        ctx.lineTo(corners[3].x, corners[3].y);
        ctx.closePath();
        ctx.moveTo(corners[0].x, corners[0].y);
        ctx.lineTo(corners[4].x, corners[4].y);
        ctx.stroke();
        const projectedGrid = [];
        for (let ix = 0; ix < nx; ix++) {
            projectedGrid[ix] = [];
            const u = (axisX[ix].numeric - minX) / spanX;
            for (let iy = 0; iy < ny; iy++) {
                const v = (axisY[iy].numeric - minY) / spanY;
                const pred = gridPredictions[ix * ny + iy];
                const w = (pred.mean - minZ) / spanZ;
                const proj = projectPoint(u, v, w);
                proj.ratio = w;
                projectedGrid[ix][iy] = proj;
            }
        }
        const quads = [];
        for (let ix = 0; ix < nx - 1; ix++) {
            for (let iy = 0; iy < ny - 1; iy++) {
                const p00 = projectedGrid[ix][iy];
                const p10 = projectedGrid[ix + 1][iy];
                const p11 = projectedGrid[ix + 1][iy + 1];
                const p01 = projectedGrid[ix][iy + 1];
                const avgDepth = (p00.depth + p10.depth + p11.depth + p01.depth) * 0.25;
                const avgRatio = (p00.ratio + p10.ratio + p11.ratio + p01.ratio) * 0.25;
                quads.push({
                    pts: [p00, p10, p11, p01],
                    depth: avgDepth,
                    ratio: avgRatio
                });
            }
        }
        quads.sort(function(a, b) {
            return a.depth - b.depth;
        });
        quads.forEach(function(q) {
            ctx.fillStyle = interpolateColorForRatio(q.ratio);
            ctx.globalAlpha = 0.72;
            ctx.beginPath();
            ctx.moveTo(q.pts[0].x, q.pts[0].y);
            ctx.lineTo(q.pts[1].x, q.pts[1].y);
            ctx.lineTo(q.pts[2].x, q.pts[2].y);
            ctx.lineTo(q.pts[3].x, q.pts[3].y);
            ctx.closePath();
            ctx.fill();
            ctx.globalAlpha = 1.0;
            ctx.strokeStyle = 'rgba(30, 30, 30, 0.45)';
            ctx.lineWidth = 0.9;
            ctx.stroke();
        });
        (exp.trials || []).forEach(function(trial) {
            const tx = encodeSingleTrialNumericValue(trial.params ? trial.params[pX.name] : undefined, pX);
            const ty = encodeSingleTrialNumericValue(trial.params ? trial.params[pY.name] : undefined, pY);
            const tz = Number(trial.objective);
            const u = (tx - minX) / spanX;
            const v = (ty - minY) / spanY;
            const w = (tz - minZ) / spanZ;
            const proj = projectPoint(u, v, w);
            ctx.fillStyle = '#000000';
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.arc(proj.x, proj.y, 4.5, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();
        });
        const xAxisMid = projectPoint(0.5, 0, 0);
        const yAxisMid = projectPoint(0, 0.5, 0);
        const zAxisTop = projectPoint(0, 0, 1.08);
        ctx.fillStyle = '#222222';
        ctx.font = 'bold 12px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(pX.name + ' [' + axisX[0].label + ' .. ' + axisX[nx - 1].label + ']', xAxisMid.x, xAxisMid.y + 18);
        ctx.fillText(pY.name + ' [' + axisY[0].label + ' .. ' + axisY[ny - 1].label + ']', yAxisMid.x, yAxisMid.y + 18);
        ctx.fillText(exp.objective, zAxisTop.x, zAxisTop.y - 6);
        ctx.font = '11px sans-serif';
        ctx.textAlign = 'left';
        ctx.fillStyle = '#555555';
        ctx.fillText('Drag canvas to rotate 3D surface | Z range: [' + minZ.toPrecision(4) + ', ' + maxZ.toPrecision(4) + ']', 12, 18);
    }
    function buildParamControlElement(param, prefillVal, inputClassName) {
        const parsed = window.OptimaEngine.parseParamConfig(param);
        if (parsed.type === 'categorical') {
            const select = document.createElement('select');
            select.className = inputClassName;
            select.dataset.paramName = param.name;
            parsed.categories.forEach(function(cat) {
                const opt = document.createElement('option');
                opt.value = cat;
                opt.textContent = cat;
                if (prefillVal !== undefined && String(prefillVal) === cat) {
                    opt.selected = true;
                }
                select.appendChild(opt);
            });
            return select;
        }
        const input = document.createElement('input');
        input.type = 'number';
        input.className = inputClassName;
        input.dataset.paramName = param.name;
        input.step = parsed.type === 'int' ? '1' : 'any';
        input.min = String(parsed.lower);
        input.max = String(parsed.upper);
        if (prefillVal !== undefined && prefillVal !== null && prefillVal !== '') {
            input.value = String(prefillVal);
        } else {
            input.value = String(parsed.type === 'int'
                ? Math.round((parsed.lower + parsed.upper) * 0.5)
                : ((parsed.lower + parsed.upper) * 0.5));
        }
        return input;
    }
    function buildAskTellInputForParam(param, prefillVal, inputClassName) {
        const wrapper = document.createElement('div');
        wrapper.className = 'form-group';
        const label = document.createElement('label');
        label.textContent = param.name + ' (' + param.type + (param.logScale ? ', log' : '') + ')';
        wrapper.appendChild(label);
        wrapper.appendChild(buildParamControlElement(param, prefillVal, inputClassName || 'trial-param-input'));
        return wrapper;
    }
    function renderExperimentsSummaryList() {
        const experiments = loadExperimentsFromStorage();
        const container = document.getElementById('experimentsList');
        if (!container) {
            return;
        }
        container.innerHTML = '';
        if (experiments.length === 0) {
            const emptyDiv = document.createElement('div');
            emptyDiv.className = 'empty-state';
            emptyDiv.textContent = 'No experiments defined. Click "+ New Experiment" to create one.';
            container.appendChild(emptyDiv);
            return;
        }
        for (const exp of experiments) {
            const best = findBestTrial(exp);
            const card = document.createElement('div');
            card.className = 'experiment-summary-card';
            const infoSide = document.createElement('div');
            infoSide.className = 'experiment-summary-info';
            const titleRow = document.createElement('div');
            titleRow.className = 'experiment-summary-title-row';
            const nameSpan = document.createElement('span');
            nameSpan.className = 'experiment-summary-name';
            nameSpan.textContent = exp.name;
            const statusSpan = document.createElement('span');
            statusSpan.className = 'status-badge status-' + exp.status;
            statusSpan.textContent = formatStatusLabel(exp.status);
            titleRow.appendChild(nameSpan);
            titleRow.appendChild(statusSpan);
            const metaLine = document.createElement('div');
            metaLine.className = 'summary-meta';
            metaLine.textContent =
                'Objective: ' + exp.objective + ' (' + exp.direction + ') · ' +
                formatSurrogateLabel(exp.config) + ' / ' +
                formatAcquisitionLabel(exp.config.acquisition) + ' · ' +
                exp.trials.length + ' trials' +
                (best ? (' · Best: ' + Number(best.trial.objective).toPrecision(4)) : '');
            infoSide.appendChild(titleRow);
            infoSide.appendChild(metaLine);
            const actionsSide = document.createElement('div');
            actionsSide.className = 'experiment-summary-actions';
            const openBtn = document.createElement('button');
            openBtn.type = 'button';
            openBtn.className = 'btn btn-primary';
            openBtn.textContent = 'Open';
            openBtn.addEventListener('click', function(e) {
                e.stopPropagation();
                openExperimentView(exp.id);
            });
            const deleteBtn = document.createElement('button');
            deleteBtn.type = 'button';
            deleteBtn.className = 'btn btn-danger';
            deleteBtn.textContent = 'Delete';
            deleteBtn.addEventListener('click', function(e) {
                e.stopPropagation();
                if (confirm('Delete experiment "' + exp.name + '"?')) {
                    deleteExperimentById(exp.id);
                }
            });
            actionsSide.appendChild(openBtn);
            actionsSide.appendChild(deleteBtn);
            card.appendChild(infoSide);
            card.appendChild(actionsSide);
            card.addEventListener('click', function() {
                openExperimentView(exp.id);
            });
            container.appendChild(card);
        }
    }
    function renderGraphModeSection(body, exp) {
        const graphPanel = document.createElement('div');
        graphPanel.className = 'graph-mode-panel';
        const dim = exp.parameters.length;
        if (dim === 0 || dim > 2) {
            const msg = document.createElement('div');
            msg.className = 'empty-state';
            msg.textContent = 'Graph Mode supports experiments with 1 or 2 parameters (this experiment has ' + dim + ').';
            graphPanel.appendChild(msg);
            body.appendChild(graphPanel);
            return;
        }
        if (!exp.trials || exp.trials.length === 0) {
            const msg = document.createElement('div');
            msg.className = 'empty-state';
            msg.textContent = 'Record at least 1 trial in Optimization view before plotting surrogate predictions.';
            graphPanel.appendChild(msg);
            body.appendChild(graphPanel);
            return;
        }
        const controls = document.createElement('div');
        controls.className = 'graph-controls-bar';
        const pointsGroup = document.createElement('div');
        pointsGroup.className = 'graph-control-item';
        const pointsLabel = document.createElement('label');
        pointsLabel.textContent = dim === 1 ? 'Equidistant Points' : 'Points Per Axis';
        const pointsInput = document.createElement('input');
        pointsInput.type = 'number';
        pointsInput.className = 'compact-input';
        pointsInput.min = '2';
        pointsInput.max = dim === 1 ? '200' : '45';
        pointsInput.value = String(dim === 1 ? graphConfigState.points1D : graphConfigState.pointsPerAxis2D);
        pointsGroup.appendChild(pointsLabel);
        pointsGroup.appendChild(pointsInput);
        controls.appendChild(pointsGroup);
        let bandCheckbox = null;
        let modeSelect = null;
        let resetAngleBtn = null;
        if (dim === 1) {
            const bandGroup = document.createElement('label');
            bandGroup.className = 'graph-checkbox-label';
            bandCheckbox = document.createElement('input');
            bandCheckbox.type = 'checkbox';
            bandCheckbox.checked = graphConfigState.showBand;
            bandGroup.appendChild(bandCheckbox);
            bandGroup.appendChild(document.createTextNode(' Show ±1 Std Band'));
            controls.appendChild(bandGroup);
        } else {
            const modeGroup = document.createElement('div');
            modeGroup.className = 'graph-control-item';
            const modeLabel = document.createElement('label');
            modeLabel.textContent = 'Display';
            modeSelect = document.createElement('select');
            modeSelect.className = 'compact-select';
            [
                { value: '3d', label: '3D Surface (Wireframe)' },
                { value: 'heatmap', label: '2D Heatmap' }
            ].forEach(function(optDef) {
                const opt = document.createElement('option');
                opt.value = optDef.value;
                opt.textContent = optDef.label;
                if (graphConfigState.mode2D === optDef.value) opt.selected = true;
                modeSelect.appendChild(opt);
            });
            modeGroup.appendChild(modeLabel);
            modeGroup.appendChild(modeSelect);
            controls.appendChild(modeGroup);
            resetAngleBtn = document.createElement('button');
            resetAngleBtn.type = 'button';
            resetAngleBtn.className = 'btn btn-compact';
            resetAngleBtn.textContent = 'Reset Angle';
            controls.appendChild(resetAngleBtn);
        }
        const refreshBtn = document.createElement('button');
        refreshBtn.type = 'button';
        refreshBtn.className = 'btn btn-primary btn-compact';
        refreshBtn.textContent = 'Update Graph';
        controls.appendChild(refreshBtn);
        graphPanel.appendChild(controls);
        const readout = document.createElement('div');
        readout.className = 'prediction-banner';
        readout.textContent = dim === 1
            ? 'Hover over the graph to inspect interpolated equidistant surrogate predictions.'
            : 'Drag the 3D surface to rotate, or switch to 2D Heatmap and hover cells.';
        graphPanel.appendChild(readout);
        const canvasWrap = document.createElement('div');
        canvasWrap.className = 'chart-container';
        const canvas = document.createElement('canvas');
        canvas.width = 860;
        canvas.height = 440;
        canvas.className = 'surrogate-graph-canvas';
        canvasWrap.appendChild(canvas);
        graphPanel.appendChild(canvasWrap);
        body.appendChild(graphPanel);
        let currentAxisX = null;
        let currentAxisY = null;
        let currentPredictions = null;
        let currentParsedParams = null;
        function computeAndDraw() {
            if (!window.OptimaEngine.isReady()) {
                readout.textContent = 'WebAssembly module (optima.js / optima.wasm) is not loaded. Build it with ./webui/build_wasm.sh to render surrogate predictions.';
                return;
            }
            const parsedParams = exp.parameters.map(window.OptimaEngine.parseParamConfig);
            currentParsedParams = parsedParams;
            if (dim === 1) {
                const count = Math.max(2, Math.min(200, parseInt(pointsInput.value, 10) || 25));
                graphConfigState.points1D = count;
                pointsInput.value = String(count);
                graphConfigState.showBand = Boolean(bandCheckbox.checked);
                currentAxisX = buildEquidistantAxisValues(parsedParams[0], count);
                const queryList = currentAxisX.map(function(item) { return [item.numeric]; });
                currentPredictions = window.OptimaEngine.predictBatch(exp, exp.config, queryList);
                if (!currentPredictions) {
                    return;
                }
                draw1DSurrogateGraph(canvas, exp, parsedParams[0], currentAxisX, currentPredictions, graphConfigState.showBand, -1);
                readout.textContent = 'Plotted ' + currentAxisX.length + ' equidistant points using ' + formatSurrogateLabel(exp.config) + ' (length scale ' + exp.config.lengthScale + '). Red dots show recorded trials.';
            } else {
                const count = Math.max(2, Math.min(45, parseInt(pointsInput.value, 10) || 15));
                graphConfigState.pointsPerAxis2D = count;
                pointsInput.value = String(count);
                graphConfigState.mode2D = modeSelect.value;
                currentAxisX = buildEquidistantAxisValues(parsedParams[0], count);
                currentAxisY = buildEquidistantAxisValues(parsedParams[1], count);
                const queryList = [];
                for (let ix = 0; ix < currentAxisX.length; ix++) {
                    for (let iy = 0; iy < currentAxisY.length; iy++) {
                        queryList.push([currentAxisX[ix].numeric, currentAxisY[iy].numeric]);
                    }
                }
                currentPredictions = window.OptimaEngine.predictBatch(exp, exp.config, queryList);
                if (!currentPredictions) {
                    return;
                }
                if (graphConfigState.mode2D === 'heatmap') {
                    draw2DHeatmapGraph(canvas, exp, parsedParams[0], parsedParams[1], currentAxisX, currentAxisY, currentPredictions, null);
                } else {
                    draw3DSurfaceGraph(canvas, exp, parsedParams[0], parsedParams[1], currentAxisX, currentAxisY, currentPredictions, graphConfigState.yaw, graphConfigState.pitch);
                }
                readout.textContent = 'Evaluated ' + currentAxisX.length + ' x ' + currentAxisY.length + ' (' + queryList.length + ' equidistant grid points) using ' + formatSurrogateLabel(exp.config) + '.';
            }
        }
        refreshBtn.addEventListener('click', computeAndDraw);
        pointsInput.addEventListener('change', computeAndDraw);
        if (bandCheckbox) {
            bandCheckbox.addEventListener('change', computeAndDraw);
        }
        if (modeSelect) {
            modeSelect.addEventListener('change', computeAndDraw);
        }
        if (resetAngleBtn) {
            resetAngleBtn.addEventListener('click', function() {
                graphConfigState.yaw = -0.66;
                graphConfigState.pitch = 0.45;
                if (currentPredictions && graphConfigState.mode2D === '3d') {
                    draw3DSurfaceGraph(canvas, exp, currentParsedParams[0], currentParsedParams[1], currentAxisX, currentAxisY, currentPredictions, graphConfigState.yaw, graphConfigState.pitch);
                }
            });
        }
        let dragging = false;
        let lastX = 0;
        let lastY = 0;
        canvas.addEventListener('mousedown', function(e) {
            if (dim === 2 && graphConfigState.mode2D === '3d') {
                dragging = true;
                lastX = e.clientX;
                lastY = e.clientY;
            }
        });
        window.addEventListener('mouseup', function() {
            dragging = false;
        });
        canvas.addEventListener('mousemove', function(e) {
            const rect = canvas.getBoundingClientRect();
            const scaleX = canvas.width / Math.max(1, rect.width);
            const scaleY = canvas.height / Math.max(1, rect.height);
            const cx = (e.clientX - rect.left) * scaleX;
            const cy = (e.clientY - rect.top) * scaleY;
            if (dim === 1 && currentAxisX && currentPredictions) {
                const padLeft = 64;
                const padRight = 24;
                const plotW = canvas.width - padLeft - padRight;
                const ratio = (cx - padLeft) / Math.max(1, plotW);
                const idx = Math.max(0, Math.min(currentAxisX.length - 1, Math.round(ratio * (currentAxisX.length - 1))));
                draw1DSurrogateGraph(canvas, exp, currentParsedParams[0], currentAxisX, currentPredictions, graphConfigState.showBand, idx);
                const p = currentPredictions[idx];
                readout.textContent = currentParsedParams[0].name + ' = ' + currentAxisX[idx].label +
                    '  |  Predicted ' + exp.objective + ': ' + Number(p.mean).toPrecision(5) + ' ± ' + Number(p.stdDev).toPrecision(4);
            } else if (dim === 2 && currentPredictions) {
                if (graphConfigState.mode2D === '3d' && dragging) {
                    const dx = e.clientX - lastX;
                    const dy = e.clientY - lastY;
                    lastX = e.clientX;
                    lastY = e.clientY;
                    graphConfigState.yaw += dx * 0.01;
                    graphConfigState.pitch = Math.max(-1.35, Math.min(1.35, graphConfigState.pitch + dy * 0.01));
                    draw3DSurfaceGraph(canvas, exp, currentParsedParams[0], currentParsedParams[1], currentAxisX, currentAxisY, currentPredictions, graphConfigState.yaw, graphConfigState.pitch);
                } else if (graphConfigState.mode2D === 'heatmap') {
                    const padLeft = 72;
                    const padRight = 100;
                    const padTop = 22;
                    const padBottom = 46;
                    const plotW = canvas.width - padLeft - padRight;
                    const plotH = canvas.height - padTop - padBottom;
                    const rx = (cx - padLeft) / Math.max(1, plotW);
                    const ry = 1.0 - (cy - padTop) / Math.max(1, plotH);
                    if (rx >= 0 && rx <= 1 && ry >= 0 && ry <= 1) {
                        const ix = Math.max(0, Math.min(currentAxisX.length - 1, Math.floor(rx * currentAxisX.length)));
                        const iy = Math.max(0, Math.min(currentAxisY.length - 1, Math.floor(ry * currentAxisY.length)));
                        const pred = currentPredictions[ix * currentAxisY.length + iy];
                        draw2DHeatmapGraph(canvas, exp, currentParsedParams[0], currentParsedParams[1], currentAxisX, currentAxisY, currentPredictions, { ix: ix, iy: iy });
                        readout.textContent = currentParsedParams[0].name + ' = ' + currentAxisX[ix].label +
                            ', ' + currentParsedParams[1].name + ' = ' + currentAxisY[iy].label +
                            '  |  Predicted ' + exp.objective + ': ' + Number(pred.mean).toPrecision(5) + ' ± ' + Number(pred.stdDev).toPrecision(4);
                    }
                }
            }
        });
        computeAndDraw();
    }
    function renderActiveExperiment() {
        const panel = document.getElementById('activeExperimentPanel');
        if (!panel) {
            return;
        }
        const experiments = loadExperimentsFromStorage();
        const exp = experiments.find(function(e) { return e.id === activeExperimentId; });
        if (!exp) {
            closeExperimentView();
            return;
        }
        const best = findBestTrial(exp);
        panel.innerHTML = '';
        const header = document.createElement('div');
        header.className = 'active-experiment-header';
        const headerLeft = document.createElement('div');
        headerLeft.className = 'active-experiment-title-group';
        const titleEl = document.createElement('h2');
        titleEl.textContent = exp.name;
        const statusBadge = document.createElement('span');
        statusBadge.className = 'status-badge status-' + exp.status;
        statusBadge.textContent = formatStatusLabel(exp.status);
        const objSummary = document.createElement('span');
        objSummary.className = 'summary-meta';
        objSummary.textContent = exp.objective + ' (' + exp.direction + ')';
        headerLeft.appendChild(titleEl);
        headerLeft.appendChild(statusBadge);
        headerLeft.appendChild(objSummary);
        const headerRight = document.createElement('div');
        headerRight.className = 'active-experiment-header-actions';
        const optViewBtn = document.createElement('button');
        optViewBtn.type = 'button';
        optViewBtn.className = 'btn' + (activeViewMode === 'optimize' ? ' btn-primary' : '');
        optViewBtn.textContent = 'Optimization';
        optViewBtn.addEventListener('click', function() {
            activeViewMode = 'optimize';
            renderActiveExperiment();
        });
        const graphAllowed = exp.parameters.length >= 1 && exp.parameters.length <= 2;
        const graphViewBtn = document.createElement('button');
        graphViewBtn.type = 'button';
        graphViewBtn.className = 'btn' + (activeViewMode === 'graph' ? ' btn-primary' : '');
        graphViewBtn.textContent = graphAllowed ? 'Graph Mode' : 'Graph Mode (<= 2 inputs)';
        graphViewBtn.disabled = !graphAllowed;
        graphViewBtn.addEventListener('click', function() {
            activeViewMode = 'graph';
            renderActiveExperiment();
        });
        const editInputsBtn = document.createElement('button');
        editInputsBtn.type = 'button';
        editInputsBtn.className = 'btn';
        editInputsBtn.textContent = 'Input Data & Setup';
        editInputsBtn.addEventListener('click', function() {
            openEditExperimentMenu(exp.id);
        });
        const closeXBtn = document.createElement('button');
        closeXBtn.type = 'button';
        closeXBtn.className = 'btn close-x-btn';
        closeXBtn.textContent = 'X';
        closeXBtn.title = 'Return to Experiments';
        closeXBtn.setAttribute('aria-label', 'Return to Experiments');
        closeXBtn.addEventListener('click', function() {
            closeExperimentView();
        });
        headerRight.appendChild(optViewBtn);
        headerRight.appendChild(graphViewBtn);
        headerRight.appendChild(editInputsBtn);
        headerRight.appendChild(closeXBtn);
        header.appendChild(headerLeft);
        header.appendChild(headerRight);
        panel.appendChild(header);
        const body = document.createElement('div');
        body.className = 'experiment-body';
        const infoBar = document.createElement('div');
        infoBar.className = 'experiment-controls-bar';
        infoBar.innerHTML =
            '<div><strong>Warmup:</strong> ' + escapeHtml(exp.config.warmupTrials) +
            ' &nbsp;|&nbsp; <strong>Parameters:</strong> ' + escapeHtml(exp.parameters.map(function(p) { return p.name; }).join(', ')) +
            '</div>';
        const controlsRow = document.createElement('div');
        controlsRow.className = 'inline-controls';
        const statusSelect = document.createElement('select');
        statusSelect.className = 'compact-select';
        ['idle', 'running', 'completed'].forEach(function(st) {
            const opt = document.createElement('option');
            opt.value = st;
            opt.textContent = 'Status: ' + formatStatusLabel(st);
            if (exp.status === st) opt.selected = true;
            statusSelect.appendChild(opt);
        });
        statusSelect.addEventListener('change', function() {
            updateExperimentById(exp.id, function(cur) {
                cur.status = statusSelect.value;
                return cur;
            });
            renderActiveExperiment();
        });
        const acqSelect = document.createElement('select');
        acqSelect.className = 'compact-select';
        [
            { value: 'lbfgs', label: 'Acq: L-BFGS' },
            { value: 'perihelion', label: 'Acq: Perihelion' },
            { value: 'random', label: 'Acq: Random EI' }
        ].forEach(function(item) {
            const opt = document.createElement('option');
            opt.value = item.value;
            opt.textContent = item.label;
            if (exp.config.acquisition === item.value) opt.selected = true;
            acqSelect.appendChild(opt);
        });
        acqSelect.addEventListener('change', function() {
            updateExperimentById(exp.id, function(cur) {
                cur.config.acquisition = acqSelect.value;
                cur.pendingSuggestion = null;
                return cur;
            });
            renderActiveExperiment();
        });
        const lsWrap = document.createElement('label');
        lsWrap.className = 'compact-inline-label';
        lsWrap.textContent = 'Length Scale: ';
        const lsInput = document.createElement('input');
        lsInput.type = 'number';
        lsInput.min = '0.01';
        lsInput.max = '5';
        lsInput.step = '0.05';
        lsInput.className = 'compact-input';
        lsInput.value = String(exp.config.lengthScale);
        lsInput.addEventListener('change', function() {
            const newLs = Math.max(0.01, parseFloat(lsInput.value) || 0.3);
            updateExperimentById(exp.id, function(cur) {
                cur.config.lengthScale = newLs;
                cur.pendingSuggestion = null;
                return cur;
            });
            lastArbitraryPrediction = null;
            renderActiveExperiment();
        });
        lsWrap.appendChild(lsInput);
        const ppWrap = document.createElement('label');
        ppWrap.className = 'compact-inline-label';
        ppWrap.textContent = 'Poly Points: ';
        const ppInput = document.createElement('input');
        ppInput.type = 'number';
        ppInput.min = '4';
        ppInput.max = '100';
        ppInput.className = 'compact-input';
        ppInput.value = String(exp.config.polyPoints);
        ppInput.addEventListener('change', function() {
            const newPp = Math.max(4, parseInt(ppInput.value, 10) || 15);
            updateExperimentById(exp.id, function(cur) {
                cur.config.polyPoints = newPp;
                cur.pendingSuggestion = null;
                return cur;
            });
            lastArbitraryPrediction = null;
            renderActiveExperiment();
        });
        ppWrap.appendChild(ppInput);
        controlsRow.appendChild(statusSelect);
        controlsRow.appendChild(acqSelect);
        controlsRow.appendChild(lsWrap);
        controlsRow.appendChild(ppWrap);
        infoBar.appendChild(controlsRow);
        body.appendChild(infoBar);
        const councilBox = document.createElement('div');
        councilBox.className = 'inline-council-box';
        const councilTitle = document.createElement('div');
        councilTitle.className = 'inline-council-title';
        councilTitle.textContent = 'Council Surrogates (' + formatSurrogateLabel(exp.config) + ')';
        const councilGrid = document.createElement('div');
        councilGrid.className = 'surrogate-checkbox-grid';
        renderSurrogateCheckboxes(councilGrid, exp.config.surrogates, function(changedEl) {
            const nextSurrogates = readSurrogateCheckboxes(councilGrid);
            if (nextSurrogates.length === 0) {
                if (changedEl && changedEl.type === 'checkbox') {
                    changedEl.checked = true;
                }
                alert('At least one surrogate model must remain selected.');
                return;
            }
            updateExperimentById(exp.id, function(cur) {
                cur.config.surrogates = nextSurrogates;
                cur.pendingSuggestion = null;
                return cur;
            });
            lastArbitraryPrediction = null;
            renderActiveExperiment();
        });
        councilBox.appendChild(councilTitle);
        councilBox.appendChild(councilGrid);
        body.appendChild(councilBox);
        if (activeViewMode === 'graph' && graphAllowed) {
            renderGraphModeSection(body, exp);
            panel.appendChild(body);
            return;
        }
        if (best) {
            const bestBanner = document.createElement('div');
            bestBanner.className = 'best-banner';
            const bestParamStr = exp.parameters.map(function(p) {
                return escapeHtml(p.name) + '=' + escapeHtml(best.trial.params ? best.trial.params[p.name] : '');
            }).join(', ');
            bestBanner.innerHTML = '<strong>Best Trial #' + (best.index + 1) + ':</strong> ' +
                escapeHtml(exp.objective) + ' = <strong>' + escapeHtml(best.trial.objective) + '</strong>' +
                ' &nbsp;|&nbsp; ' + bestParamStr;
            body.appendChild(bestBanner);
        }
        const askTellBox = document.createElement('div');
        askTellBox.className = 'ask-tell-panel';
        const askHeader = document.createElement('div');
        askHeader.className = 'ask-tell-header';
        const askTitle = document.createElement('h4');
        askTitle.textContent = 'Ask & Tell Optimization Loop';
        const suggestBtn = document.createElement('button');
        suggestBtn.type = 'button';
        suggestBtn.className = 'btn btn-primary';
        suggestBtn.textContent = 'Ask: Suggest Next Trial';
        askHeader.appendChild(askTitle);
        askHeader.appendChild(suggestBtn);
        askTellBox.appendChild(askHeader);
        const predictionBanner = document.createElement('div');
        predictionBanner.className = 'prediction-banner';
        if (exp.pendingSuggestion) {
            if (exp.pendingSuggestion.usedWarmup) {
                predictionBanner.textContent = 'Warmup exploration suggestion (' + exp.trials.length + '/' + exp.config.warmupTrials + ' warmup trials completed).';
            } else if (exp.pendingSuggestion.predictedMean !== null) {
                predictionBanner.textContent = 'Surrogate prediction: ' +
                    Number(exp.pendingSuggestion.predictedMean).toPrecision(5) +
                    ' ± ' + Number(exp.pendingSuggestion.predictedStd).toPrecision(4) +
                    ' (' + formatSurrogateLabel(exp.config) + ' + ' + formatAcquisitionLabel(exp.config.acquisition) + ')';
            }
        } else {
            predictionBanner.textContent = 'Click "Ask: Suggest Next Trial" to generate parameters, or enter your own values below.';
        }
        askTellBox.appendChild(predictionBanner);
        const trialInputsGrid = document.createElement('div');
        trialInputsGrid.className = 'form-row';
        const prefillParams = exp.pendingSuggestion ? exp.pendingSuggestion.params : {};
        exp.parameters.forEach(function(param) {
            trialInputsGrid.appendChild(buildAskTellInputForParam(param, prefillParams[param.name], 'trial-param-input'));
        });
        const objInputGroup = document.createElement('div');
        objInputGroup.className = 'form-group';
        const objLabel = document.createElement('label');
        objLabel.textContent = 'Observed ' + exp.objective;
        const objInput = document.createElement('input');
        objInput.type = 'number';
        objInput.step = 'any';
        objInput.placeholder = 'Enter measured result';
        objInput.className = 'trial-objective-input';
        objInputGroup.appendChild(objLabel);
        objInputGroup.appendChild(objInput);
        trialInputsGrid.appendChild(objInputGroup);
        askTellBox.appendChild(trialInputsGrid);
        const tellActions = document.createElement('div');
        tellActions.className = 'tell-actions';
        const recordBtn = document.createElement('button');
        recordBtn.type = 'button';
        recordBtn.className = 'btn';
        recordBtn.textContent = 'Tell: Record Trial';
        tellActions.appendChild(recordBtn);
        askTellBox.appendChild(tellActions);
        suggestBtn.addEventListener('click', function() {
            if (!window.OptimaEngine.isReady()) {
                alert('WebAssembly module (optima.js / optima.wasm) is not loaded. Build it with ./webui/build_wasm.sh once Emscripten is installed.');
                return;
            }
            const suggestion = window.OptimaEngine.suggestNext(exp, exp.config);
            if (!suggestion) {
                alert('Unable to generate suggestion for this experiment.');
                return;
            }
            updateExperimentById(exp.id, function(cur) {
                cur.pendingSuggestion = suggestion;
                return cur;
            });
            renderActiveExperiment();
        });
        recordBtn.addEventListener('click', function() {
            const objVal = parseFloat(objInput.value);
            if (Number.isNaN(objVal)) {
                alert('Please enter a valid numeric value for ' + exp.objective + '.');
                return;
            }
            const paramInputs = trialInputsGrid.querySelectorAll('.trial-param-input');
            const recordedParams = {};
            paramInputs.forEach(function(inputEl) {
                const pName = inputEl.dataset.paramName;
                const pDef = exp.parameters.find(function(p) { return p.name === pName; });
                if (pDef && pDef.type === 'categorical') {
                    recordedParams[pName] = inputEl.value;
                } else if (pDef && pDef.type === 'int') {
                    recordedParams[pName] = Math.round(parseFloat(inputEl.value));
                } else {
                    recordedParams[pName] = parseFloat(inputEl.value);
                }
            });
            updateExperimentById(exp.id, function(cur) {
                cur.trials.push({
                    id: generateExperimentId(),
                    params: recordedParams,
                    objective: objVal,
                    predictedMean: cur.pendingSuggestion ? cur.pendingSuggestion.predictedMean : null,
                    predictedStd: cur.pendingSuggestion ? cur.pendingSuggestion.predictedStd : null,
                    timestamp: new Date().toISOString()
                });
                if (cur.status === 'idle') {
                    cur.status = 'running';
                }
                cur.pendingSuggestion = null;
                return cur;
            });
            lastArbitraryPrediction = null;
            renderActiveExperiment();
        });
        body.appendChild(askTellBox);
        const queryPanel = document.createElement('div');
        queryPanel.className = 'ask-tell-panel';
        const queryHeader = document.createElement('div');
        queryHeader.className = 'ask-tell-header';
        const queryTitle = document.createElement('h4');
        queryTitle.textContent = 'Ask Surrogate Prediction (Arbitrary Input)';
        const queryBtn = document.createElement('button');
        queryBtn.type = 'button';
        queryBtn.className = 'btn btn-primary';
        queryBtn.textContent = 'Predict Objective';
        queryHeader.appendChild(queryTitle);
        queryHeader.appendChild(queryBtn);
        queryPanel.appendChild(queryHeader);
        const queryResultBanner = document.createElement('div');
        queryResultBanner.className = 'prediction-banner';
        if (exp.trials.length === 0) {
            queryResultBanner.textContent = 'Record at least 1 trial before querying the surrogate model.';
        } else if (lastArbitraryPrediction) {
            queryResultBanner.textContent = 'Surrogate prediction for ' + exp.objective + ': ' +
                Number(lastArbitraryPrediction.mean).toPrecision(5) +
                ' ± ' + Number(lastArbitraryPrediction.stdDev).toPrecision(4) +
                ' (' + formatSurrogateLabel(exp.config) + ')';
        } else {
            queryResultBanner.textContent = 'Enter any parameter combination below and click "Predict Objective".';
        }
        queryPanel.appendChild(queryResultBanner);
        const queryInputsGrid = document.createElement('div');
        queryInputsGrid.className = 'form-row';
        const queryPrefill = lastArbitraryQueryValues || {};
        exp.parameters.forEach(function(param) {
            queryInputsGrid.appendChild(buildAskTellInputForParam(param, queryPrefill[param.name], 'query-param-input'));
        });
        queryPanel.appendChild(queryInputsGrid);
        queryBtn.addEventListener('click', function() {
            if (!window.OptimaEngine.isReady()) {
                alert('WebAssembly module (optima.js / optima.wasm) is not loaded. Build it with ./webui/build_wasm.sh once Emscripten is installed.');
                return;
            }
            if (exp.trials.length === 0) {
                alert('Record at least 1 trial first so the surrogate has data to train on.');
                return;
            }
            const queryParams = {};
            queryInputsGrid.querySelectorAll('.query-param-input').forEach(function(inputEl) {
                const pName = inputEl.dataset.paramName;
                const pDef = exp.parameters.find(function(p) { return p.name === pName; });
                if (pDef && pDef.type === 'categorical') {
                    queryParams[pName] = inputEl.value;
                } else if (pDef && pDef.type === 'int') {
                    queryParams[pName] = Math.round(parseFloat(inputEl.value));
                } else {
                    queryParams[pName] = parseFloat(inputEl.value);
                }
            });
            const pred = window.OptimaEngine.predictPoint(exp, exp.config, queryParams);
            if (!pred) {
                alert('Unable to compute surrogate prediction.');
                return;
            }
            lastArbitraryQueryValues = queryParams;
            lastArbitraryPrediction = pred;
            renderActiveExperiment();
        });
        body.appendChild(queryPanel);
        if (exp.trials.length > 0) {
            const chartWrap = document.createElement('div');
            chartWrap.className = 'chart-container';
            const canvas = document.createElement('canvas');
            canvas.width = 860;
            canvas.height = 200;
            canvas.className = 'convergence-canvas';
            chartWrap.appendChild(canvas);
            body.appendChild(chartWrap);
            drawConvergenceChart(canvas, exp);
            const trialsTable = document.createElement('table');
            trialsTable.className = 'trials-table';
            const tHead = document.createElement('thead');
            const paramHeaders = exp.parameters.map(function(p) {
                return '<th>' + escapeHtml(p.name) + '</th>';
            }).join('');
            tHead.innerHTML = '<tr><th>#</th>' + paramHeaders + '<th>' + escapeHtml(exp.objective) + '</th><th>Predicted</th><th>Actions</th></tr>';
            trialsTable.appendChild(tHead);
            const tBody = document.createElement('tbody');
            exp.trials.forEach(function(trial, idx) {
                const tr = document.createElement('tr');
                if (best && best.index === idx) {
                    tr.className = 'best-trial-row';
                }
                const numTd = document.createElement('td');
                numTd.textContent = String(idx + 1);
                tr.appendChild(numTd);
                const isEditingRow = editingTrialIndex === idx;
                if (isEditingRow) {
                    exp.parameters.forEach(function(p) {
                        const td = document.createElement('td');
                        const currentVal = trial.params ? trial.params[p.name] : '';
                        td.appendChild(buildParamControlElement(p, currentVal, 'trial-edit-input'));
                        tr.appendChild(td);
                    });
                    const objTd = document.createElement('td');
                    const objEditInput = document.createElement('input');
                    objEditInput.type = 'number';
                    objEditInput.step = 'any';
                    objEditInput.className = 'trial-edit-input trial-edit-objective';
                    objEditInput.value = String(trial.objective);
                    objTd.appendChild(objEditInput);
                    tr.appendChild(objTd);
                    const predTd = document.createElement('td');
                    predTd.textContent = (trial.predictedMean !== null && trial.predictedMean !== undefined)
                        ? (Number(trial.predictedMean).toPrecision(4) + ' ± ' + Number(trial.predictedStd).toPrecision(3))
                        : 'Warmup / Manual';
                    tr.appendChild(predTd);
                    const actionTd = document.createElement('td');
                    actionTd.className = 'trial-row-actions';
                    const saveTrialBtn = document.createElement('button');
                    saveTrialBtn.type = 'button';
                    saveTrialBtn.className = 'btn btn-primary btn-compact';
                    saveTrialBtn.textContent = 'Save';
                    saveTrialBtn.addEventListener('click', function() {
                        const newObjVal = parseFloat(objEditInput.value);
                        if (Number.isNaN(newObjVal)) {
                            alert('Please enter a valid numeric value for ' + exp.objective + '.');
                            return;
                        }
                        const updatedParams = {};
                        tr.querySelectorAll('.trial-edit-input[data-param-name]').forEach(function(el) {
                            const pName = el.dataset.paramName;
                            const pDef = exp.parameters.find(function(p) { return p.name === pName; });
                            if (pDef && pDef.type === 'categorical') {
                                updatedParams[pName] = el.value;
                            } else if (pDef && pDef.type === 'int') {
                                updatedParams[pName] = Math.round(parseFloat(el.value));
                            } else {
                                updatedParams[pName] = parseFloat(el.value);
                            }
                        });
                        updateExperimentById(exp.id, function(cur) {
                            if (cur.trials[idx]) {
                                cur.trials[idx].params = updatedParams;
                                cur.trials[idx].objective = newObjVal;
                            }
                            return cur;
                        });
                        editingTrialIndex = null;
                        lastArbitraryPrediction = null;
                        renderActiveExperiment();
                    });
                    const cancelTrialBtn = document.createElement('button');
                    cancelTrialBtn.type = 'button';
                    cancelTrialBtn.className = 'btn btn-compact';
                    cancelTrialBtn.textContent = 'Cancel';
                    cancelTrialBtn.addEventListener('click', function() {
                        editingTrialIndex = null;
                        renderActiveExperiment();
                    });
                    actionTd.appendChild(saveTrialBtn);
                    actionTd.appendChild(cancelTrialBtn);
                    tr.appendChild(actionTd);
                } else {
                    exp.parameters.forEach(function(p) {
                        const td = document.createElement('td');
                        const val = trial.params ? trial.params[p.name] : '';
                        td.textContent = val !== undefined ? String(val) : '';
                        tr.appendChild(td);
                    });
                    const objTd = document.createElement('td');
                    objTd.innerHTML = '<strong>' + escapeHtml(trial.objective) + '</strong>';
                    tr.appendChild(objTd);
                    const predTd = document.createElement('td');
                    predTd.textContent = (trial.predictedMean !== null && trial.predictedMean !== undefined)
                        ? (Number(trial.predictedMean).toPrecision(4) + ' ± ' + Number(trial.predictedStd).toPrecision(3))
                        : 'Warmup / Manual';
                    tr.appendChild(predTd);
                    const actionTd = document.createElement('td');
                    actionTd.className = 'trial-row-actions';
                    const editTrialBtn = document.createElement('button');
                    editTrialBtn.type = 'button';
                    editTrialBtn.className = 'btn btn-compact';
                    editTrialBtn.textContent = 'Edit';
                    editTrialBtn.addEventListener('click', function() {
                        editingTrialIndex = idx;
                        renderActiveExperiment();
                    });
                    const delTrialBtn = document.createElement('button');
                    delTrialBtn.type = 'button';
                    delTrialBtn.className = 'btn btn-danger';
                    delTrialBtn.textContent = 'Remove';
                    delTrialBtn.addEventListener('click', function() {
                        updateExperimentById(exp.id, function(cur) {
                            cur.trials.splice(idx, 1);
                            return cur;
                        });
                        editingTrialIndex = null;
                        lastArbitraryPrediction = null;
                        renderActiveExperiment();
                    });
                    actionTd.appendChild(editTrialBtn);
                    actionTd.appendChild(delTrialBtn);
                    tr.appendChild(actionTd);
                }
                tBody.appendChild(tr);
            });
            trialsTable.appendChild(tBody);
            body.appendChild(trialsTable);
        }
        const footerActions = document.createElement('div');
        footerActions.className = 'experiment-footer-actions';
        const exportButton = document.createElement('button');
        exportButton.type = 'button';
        exportButton.className = 'btn';
        exportButton.textContent = 'Export JSON';
        exportButton.addEventListener('click', function() {
            const blob = new Blob([JSON.stringify(exp, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = exp.name.replace(/[^a-zA-Z0-9_-]/g, '_') + '.json';
            a.click();
            URL.revokeObjectURL(url);
        });
        const deleteButton = document.createElement('button');
        deleteButton.type = 'button';
        deleteButton.className = 'btn btn-danger';
        deleteButton.textContent = 'Delete Experiment';
        deleteButton.addEventListener('click', function() {
            if (confirm('Delete experiment "' + exp.name + '"?')) {
                deleteExperimentById(exp.id);
            }
        });
        footerActions.appendChild(exportButton);
        footerActions.appendChild(deleteButton);
        body.appendChild(footerActions);
        panel.appendChild(body);
    }
    function openExperimentView(id) {
        const experiments = loadExperimentsFromStorage();
        const exists = experiments.some(function(e) { return e.id === id; });
        if (!exists) {
            closeExperimentView();
            return;
        }
        activeExperimentId = id;
        editingTrialIndex = null;
        activeViewMode = 'optimize';
        lastArbitraryPrediction = null;
        lastArbitraryQueryValues = null;
        const targetUrl = window.location.pathname + '?exp=' + encodeURIComponent(id);
        if (window.location.pathname + window.location.search !== targetUrl) {
            window.history.replaceState(null, '', targetUrl);
        }
        document.getElementById('experimentsMenuView').hidden = true;
        document.getElementById('activeExperimentView').hidden = false;
        renderActiveExperiment();
    }
    function closeExperimentView() {
        activeExperimentId = null;
        editingTrialIndex = null;
        activeViewMode = 'optimize';
        lastArbitraryPrediction = null;
        lastArbitraryQueryValues = null;
        closeEditExperimentMenu();
        if (window.location.search) {
            window.history.replaceState(null, '', window.location.pathname);
        }
        document.getElementById('activeExperimentView').hidden = true;
        document.getElementById('experimentsMenuView').hidden = false;
        renderExperimentsSummaryList();
    }
    function initialiseApp() {
        const navExperimentsLink = document.getElementById('navExperimentsLink');
        const openNewBtn = document.getElementById('openNewExperimentMenuButton');
        const closeNewBtn = document.getElementById('closeNewExperimentMenuButton');
        const cancelNewBtn = document.getElementById('cancelNewExperimentButton');
        const createBtn = document.getElementById('createExperimentButton');
        const addParamBtn = document.getElementById('addParamButton');
        const paramsContainer = document.getElementById('paramsContainer');
        const newExperimentMenu = document.getElementById('newExperimentMenu');
        const editExperimentMenu = document.getElementById('editExperimentMenu');
        const closeEditBtn = document.getElementById('closeEditExperimentMenuButton');
        const cancelEditBtn = document.getElementById('cancelEditExperimentButton');
        const saveEditBtn = document.getElementById('saveExperimentInputsButton');
        const addEditParamBtn = document.getElementById('addEditParamButton');
        const editParamsContainer = document.getElementById('editParamsContainer');
        if (navExperimentsLink) {
            navExperimentsLink.addEventListener('click', function(e) {
                e.preventDefault();
                closeNewExperimentMenu();
                closeExperimentView();
            });
        }
        if (openNewBtn) {
            openNewBtn.addEventListener('click', openNewExperimentMenu);
        }
        if (closeNewBtn) {
            closeNewBtn.addEventListener('click', closeNewExperimentMenu);
        }
        if (cancelNewBtn) {
            cancelNewBtn.addEventListener('click', closeNewExperimentMenu);
        }
        if (newExperimentMenu) {
            newExperimentMenu.addEventListener('click', function(e) {
                if (e.target === newExperimentMenu) {
                    closeNewExperimentMenu();
                }
            });
        }
        if (createBtn) {
            createBtn.addEventListener('click', function(e) {
                e.preventDefault();
                addExperimentAndOpen();
            });
        }
        if (addParamBtn) {
            addParamBtn.addEventListener('click', function() {
                addParameterRowToContainer(paramsContainer, false, null);
            });
        }
        if (closeEditBtn) {
            closeEditBtn.addEventListener('click', closeEditExperimentMenu);
        }
        if (cancelEditBtn) {
            cancelEditBtn.addEventListener('click', closeEditExperimentMenu);
        }
        if (editExperimentMenu) {
            editExperimentMenu.addEventListener('click', function(e) {
                if (e.target === editExperimentMenu) {
                    closeEditExperimentMenu();
                }
            });
        }
        if (saveEditBtn) {
            saveEditBtn.addEventListener('click', function(e) {
                e.preventDefault();
                saveEditedExperimentInputs();
            });
        }
        if (addEditParamBtn) {
            addEditParamBtn.addEventListener('click', function() {
                addParameterRowToContainer(editParamsContainer, false, null);
            });
        }
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape') {
                if (newExperimentMenu && !newExperimentMenu.hidden) {
                    closeNewExperimentMenu();
                } else if (editExperimentMenu && !editExperimentMenu.hidden) {
                    closeEditExperimentMenu();
                }
            }
        });
        if (window.OptimaEngine) {
            window.OptimaEngine.init();
        }
        const urlParams = new URLSearchParams(window.location.search);
        const requestedExpId = urlParams.get('exp');
        if (requestedExpId) {
            openExperimentView(requestedExpId);
        } else {
            closeExperimentView();
        }
    }
    document.addEventListener('DOMContentLoaded', initialiseApp);
})();