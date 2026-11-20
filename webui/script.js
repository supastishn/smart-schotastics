(function() {
    const STORAGE_KEY_EXPERIMENTS = 'experiments';
    const STORAGE_KEY_SETTINGS = 'settings';
    const DEFAULT_SETTINGS = {
        defaultDirection: 'minimize',
        defaultSurrogate: 'council',
        defaultAcquisition: 'lbfgs',
        defaultWarmupTrials: 5,
        defaultLengthScale: 0.3
    };
    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }
    function loadSettingsFromStorage() {
        const raw = localStorage.getItem(STORAGE_KEY_SETTINGS);
        if (!raw) {
            return Object.assign({}, DEFAULT_SETTINGS);
        }
        try {
            return Object.assign({}, DEFAULT_SETTINGS, JSON.parse(raw));
        } catch (e) {
            return Object.assign({}, DEFAULT_SETTINGS);
        }
    }
    function normalizeExperiment(exp) {
        const settings = loadSettingsFromStorage();
        const cfg = exp.config || {};
        return {
            id: exp.id || generateExperimentId(),
            name: exp.name || 'untitled',
            objective: exp.objective || 'objective',
            direction: exp.direction === 'maximize' ? 'maximize' : 'minimize',
            status: exp.status || 'idle',
            config: {
                surrogate: cfg.surrogate || settings.defaultSurrogate,
                acquisition: cfg.acquisition || settings.defaultAcquisition,
                warmupTrials: Number(cfg.warmupTrials) || settings.defaultWarmupTrials,
                lengthScale: Number(cfg.lengthScale) || settings.defaultLengthScale,
                polyDegree: Number(cfg.polyDegree) || 2,
                polyPoints: Number(cfg.polyPoints) || 15
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
    function formatSurrogateLabel(surrogate) {
        if (surrogate === 'gaussian_process') return 'GP (RBF)';
        if (surrogate === 'polynomial') return 'Polynomial';
        return 'Council';
    }
    function formatAcquisitionLabel(acq) {
        if (acq === 'perihelion') return 'Perihelion';
        if (acq === 'random') return 'Random EI';
        return 'L-BFGS';
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
    function createExperimentFromForm() {
        const nameInput = document.getElementById('expName');
        const objectiveInput = document.getElementById('objectiveName');
        const directionSelect = document.getElementById('direction');
        const surrogateSelect = document.getElementById('surrogateType');
        const acquisitionSelect = document.getElementById('acquisitionType');
        const warmupInput = document.getElementById('warmupTrials');
        const lengthScaleInput = document.getElementById('rbfLengthScale');
        const paramRows = document.querySelectorAll('#paramsContainer .param-row');
        if (!nameInput.value.trim() || !objectiveInput.value.trim()) {
            return null;
        }
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
                type: paramType,
                range: rangeEl.value.trim(),
                logScale: logScale
            });
        }
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
                surrogate: surrogateSelect.value,
                acquisition: acquisitionSelect.value,
                warmupTrials: Math.max(1, parseInt(warmupInput.value, 10) || 5),
                lengthScale: Math.max(0.01, parseFloat(lengthScaleInput.value) || 0.3)
            },
            parameters: parameters,
            trials: []
        });
    }
    function applyDefaultsToForm() {
        const settings = loadSettingsFromStorage();
        const directionSelect = document.getElementById('direction');
        const surrogateSelect = document.getElementById('surrogateType');
        const acquisitionSelect = document.getElementById('acquisitionType');
        const warmupInput = document.getElementById('warmupTrials');
        const lengthScaleInput = document.getElementById('rbfLengthScale');
        if (directionSelect) directionSelect.value = settings.defaultDirection;
        if (surrogateSelect) surrogateSelect.value = settings.defaultSurrogate;
        if (acquisitionSelect) acquisitionSelect.value = settings.defaultAcquisition;
        if (warmupInput) warmupInput.value = settings.defaultWarmupTrials;
        if (lengthScaleInput) lengthScaleInput.value = settings.defaultLengthScale;
    }
    function addExperimentAndRefresh() {
        const newExperiment = createExperimentFromForm();
        if (!newExperiment) {
            alert('Please fill in all required fields and add at least one valid parameter.');
            return;
        }
        const experiments = loadExperimentsFromStorage();
        experiments.push(newExperiment);
        saveExperimentsToStorage(experiments);
        populateExperimentsList(newExperiment.id);
        document.getElementById('experimentForm').reset();
        applyDefaultsToForm();
        const container = document.getElementById('paramsContainer');
        container.innerHTML = '';
        addParameterRow(true);
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
        populateExperimentsList();
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
    function buildAskTellInputForParam(param, prefillVal) {
        const parsed = window.OptimaEngine.parseParamConfig(param);
        const wrapper = document.createElement('div');
        wrapper.className = 'form-group';
        const label = document.createElement('label');
        label.textContent = param.name + ' (' + param.type + (param.logScale ? ', log' : '') + ')';
        wrapper.appendChild(label);
        if (parsed.type === 'categorical') {
            const select = document.createElement('select');
            select.className = 'trial-param-input';
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
            wrapper.appendChild(select);
        } else {
            const input = document.createElement('input');
            input.type = 'number';
            input.className = 'trial-param-input';
            input.dataset.paramName = param.name;
            input.step = parsed.type === 'int' ? '1' : 'any';
            input.min = String(parsed.lower);
            input.max = String(parsed.upper);
            if (prefillVal !== undefined && prefillVal !== null) {
                input.value = String(prefillVal);
            } else {
                input.value = String(parsed.type === 'int'
                    ? Math.round((parsed.lower + parsed.upper) * 0.5)
                    : ((parsed.lower + parsed.upper) * 0.5));
            }
            wrapper.appendChild(input);
        }
        return wrapper;
    }
    function populateExperimentsList(openExperimentId) {
        const experiments = loadExperimentsFromStorage();
        const container = document.getElementById('experimentsList');
        if (!container) {
            return;
        }
        const previouslyOpen = new Set();
        container.querySelectorAll('details.experiment-card[open]').forEach(function(el) {
            if (el.dataset.expId) {
                previouslyOpen.add(el.dataset.expId);
            }
        });
        if (openExperimentId) {
            previouslyOpen.add(openExperimentId);
        }
        container.innerHTML = '';
        if (experiments.length === 0) {
            const emptyDiv = document.createElement('div');
            emptyDiv.className = 'empty-state';
            emptyDiv.textContent = 'No experiments defined. Create one using the form below.';
            container.appendChild(emptyDiv);
            return;
        }
        for (const exp of experiments) {
            const details = document.createElement('details');
            details.className = 'experiment-card';
            details.dataset.expId = exp.id;
            if (previouslyOpen.has(exp.id)) {
                details.open = true;
            }
            const best = findBestTrial(exp);
            const summary = document.createElement('summary');
            const titleGroup = document.createElement('span');
            titleGroup.className = 'summary-left';
            const nameStrong = document.createElement('span');
            nameStrong.textContent = exp.name;
            const metaSpan = document.createElement('span');
            metaSpan.className = 'summary-meta';
            metaSpan.textContent = formatSurrogateLabel(exp.config.surrogate) + ' · ' +
                formatAcquisitionLabel(exp.config.acquisition) + ' · ' +
                exp.trials.length + ' trials' +
                (best ? (' · Best ' + exp.objective + ': ' + Number(best.trial.objective).toPrecision(4)) : '');
            titleGroup.appendChild(nameStrong);
            titleGroup.appendChild(metaSpan);
            const statusSpan = document.createElement('span');
            statusSpan.className = 'status-badge status-' + exp.status;
            statusSpan.textContent = exp.status;
            summary.appendChild(titleGroup);
            summary.appendChild(statusSpan);
            const body = document.createElement('div');
            body.className = 'experiment-body';
            const infoBar = document.createElement('div');
            infoBar.className = 'experiment-controls-bar';
            infoBar.innerHTML =
                '<div><strong>Objective:</strong> ' + escapeHtml(exp.objective) + ' (' + escapeHtml(exp.direction) + ')</div>';
            const controlsRow = document.createElement('div');
            controlsRow.className = 'inline-controls';
            const statusSelect = document.createElement('select');
            statusSelect.className = 'compact-select';
            ['idle', 'running', 'completed'].forEach(function(st) {
                const opt = document.createElement('option');
                opt.value = st;
                opt.textContent = 'Status: ' + st;
                if (exp.status === st) opt.selected = true;
                statusSelect.appendChild(opt);
            });
            statusSelect.addEventListener('change', function() {
                updateExperimentById(exp.id, function(cur) {
                    cur.status = statusSelect.value;
                    return cur;
                });
                populateExperimentsList(exp.id);
            });
            const surrogateSelect = document.createElement('select');
            surrogateSelect.className = 'compact-select';
            [
                { value: 'council', label: 'Surrogate: Council' },
                { value: 'gaussian_process', label: 'Surrogate: GP (RBF)' },
                { value: 'polynomial', label: 'Surrogate: Polynomial' }
            ].forEach(function(item) {
                const opt = document.createElement('option');
                opt.value = item.value;
                opt.textContent = item.label;
                if (exp.config.surrogate === item.value) opt.selected = true;
                surrogateSelect.appendChild(opt);
            });
            surrogateSelect.addEventListener('change', function() {
                updateExperimentById(exp.id, function(cur) {
                    cur.config.surrogate = surrogateSelect.value;
                    return cur;
                });
                populateExperimentsList(exp.id);
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
                    return cur;
                });
                populateExperimentsList(exp.id);
            });
            controlsRow.appendChild(statusSelect);
            controlsRow.appendChild(surrogateSelect);
            controlsRow.appendChild(acqSelect);
            infoBar.appendChild(controlsRow);
            body.appendChild(infoBar);
            const paramTable = document.createElement('table');
            const thead = document.createElement('thead');
            thead.innerHTML = '<tr><th>Parameter</th><th>Type</th><th>Range / Choices</th><th>Scale</th></tr>';
            paramTable.appendChild(thead);
            const tbody = document.createElement('tbody');
            for (const param of exp.parameters) {
                const tr = document.createElement('tr');
                tr.innerHTML = '<td>' + escapeHtml(param.name) + '</td>' +
                    '<td>' + escapeHtml(param.type) + '</td>' +
                    '<td>' + escapeHtml(param.range) + '</td>' +
                    '<td>' + (param.type === 'categorical' ? 'discrete' : (param.logScale ? 'log' : 'linear')) + '</td>';
                tbody.appendChild(tr);
            }
            paramTable.appendChild(tbody);
            body.appendChild(paramTable);
            if (best) {
                const bestBanner = document.createElement('div');
                bestBanner.className = 'best-banner';
                const bestParamStr = exp.parameters.map(function(p) {
                    return escapeHtml(p.name) + '=' + escapeHtml(best.trial.params[p.name]);
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
                        ' (' + formatSurrogateLabel(exp.config.surrogate) + ' + ' + formatAcquisitionLabel(exp.config.acquisition) + ')';
                }
            } else {
                predictionBanner.textContent = 'Click "Ask: Suggest Next Trial" to generate parameters, or enter your own values below.';
            }
            askTellBox.appendChild(predictionBanner);
            const trialInputsGrid = document.createElement('div');
            trialInputsGrid.className = 'form-row';
            const prefillParams = exp.pendingSuggestion ? exp.pendingSuggestion.params : {};
            exp.parameters.forEach(function(param) {
                trialInputsGrid.appendChild(buildAskTellInputForParam(param, prefillParams[param.name]));
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
                populateExperimentsList(exp.id);
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
                populateExperimentsList(exp.id);
            });
            body.appendChild(askTellBox);
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
                tHead.innerHTML = '<tr><th>#</th>' + paramHeaders + '<th>' + escapeHtml(exp.objective) + '</th><th>Predicted</th><th>Action</th></tr>';
                trialsTable.appendChild(tHead);
                const tBody = document.createElement('tbody');
                exp.trials.forEach(function(trial, idx) {
                    const tr = document.createElement('tr');
                    if (best && best.index === idx) {
                        tr.className = 'best-trial-row';
                    }
                    const paramCells = exp.parameters.map(function(p) {
                        const val = trial.params ? trial.params[p.name] : '';
                        return '<td>' + escapeHtml(val) + '</td>';
                    }).join('');
                    const predStr = (trial.predictedMean !== null && trial.predictedMean !== undefined)
                        ? (Number(trial.predictedMean).toPrecision(4) + ' ± ' + Number(trial.predictedStd).toPrecision(3))
                        : 'warmup / manual';
                    tr.innerHTML = '<td>' + (idx + 1) + '</td>' + paramCells +
                        '<td><strong>' + escapeHtml(trial.objective) + '</strong></td>' +
                        '<td>' + escapeHtml(predStr) + '</td>';
                    const actionTd = document.createElement('td');
                    const delTrialBtn = document.createElement('button');
                    delTrialBtn.type = 'button';
                    delTrialBtn.className = 'btn btn-danger';
                    delTrialBtn.textContent = 'Remove';
                    delTrialBtn.addEventListener('click', function() {
                        updateExperimentById(exp.id, function(cur) {
                            cur.trials.splice(idx, 1);
                            return cur;
                        });
                        populateExperimentsList(exp.id);
                    });
                    actionTd.appendChild(delTrialBtn);
                    tr.appendChild(actionTd);
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
            deleteButton.addEventListener('click', function(e) {
                e.stopPropagation();
                if (confirm('Delete experiment "' + exp.name + '"?')) {
                    deleteExperimentById(exp.id);
                }
            });
            footerActions.appendChild(exportButton);
            footerActions.appendChild(deleteButton);
            body.appendChild(footerActions);
            details.appendChild(summary);
            details.appendChild(body);
            container.appendChild(details);
        }
    }
    function addParameterRow(isFirst) {
        const container = document.getElementById('paramsContainer');
        if (!container) {
            return;
        }
        const row = document.createElement('div');
        row.className = 'form-row param-row';
        row.innerHTML =
            '<div class="form-group">' +
                '<label>Name</label>' +
                '<input type="text" placeholder="lr" class="param-name" required>' +
            '</div>' +
            '<div class="form-group">' +
                '<label>Type</label>' +
                '<select class="param-type" required>' +
                    '<option value="float">float</option>' +
                    '<option value="int">int</option>' +
                    '<option value="categorical">categorical</option>' +
                '</select>' +
            '</div>' +
            '<div class="form-group">' +
                '<label>Range / Choices</label>' +
                '<input type="text" placeholder="0.0001, 0.1" class="param-range" required>' +
            '</div>' +
            '<div class="form-group param-scale-group">' +
                '<label>Scale</label>' +
                '<select class="param-scale">' +
                    '<option value="linear">linear</option>' +
                    '<option value="log">log</option>' +
                '</select>' +
            '</div>';
        const typeSelect = row.querySelector('.param-type');
        const rangeInput = row.querySelector('.param-range');
        const scaleSelect = row.querySelector('.param-scale');
        typeSelect.addEventListener('change', function() {
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
        });
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
    function updateEngineBadge() {
        const badge = document.getElementById('engineStatusBadge');
        if (!badge || !window.OptimaEngine) {
            return;
        }
        window.OptimaEngine.init().then(function(backend) {
            badge.textContent = backend === 'wasm'
                ? 'Engine: WebAssembly (C++)'
                : 'Engine: WASM Not Built';
            badge.className = 'engine-badge ' + (backend === 'wasm' ? 'engine-wasm' : 'engine-missing');
        });
    }
    function initialiseApp() {
        const createBtn = document.getElementById('createExperimentButton');
        const addParamBtn = document.getElementById('addParamButton');
        if (createBtn) {
            createBtn.addEventListener('click', function(e) {
                e.preventDefault();
                addExperimentAndRefresh();
            });
        }
        if (addParamBtn) {
            addParamBtn.addEventListener('click', function() {
                addParameterRow(false);
            });
        }
        addParameterRow(true);
        applyDefaultsToForm();
        updateEngineBadge();
        populateExperimentsList();
    }
    document.addEventListener('DOMContentLoaded', initialiseApp);
})();