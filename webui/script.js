(function() {
    const STORAGE_KEY_EXPERIMENTS = 'experiments';
    const STORAGE_KEY_SETTINGS = 'settings';
    function loadExperimentsFromStorage() {
        const raw = localStorage.getItem(STORAGE_KEY_EXPERIMENTS);
        if (!raw) return [];
        try {
            return JSON.parse(raw);
        } catch (e) {
            return [];
        }
    }
    function saveExperimentsToStorage(experiments) {
        localStorage.setItem(STORAGE_KEY_EXPERIMENTS, JSON.stringify(experiments));
    }
    function loadSettingsFromStorage() {
        const raw = localStorage.getItem(STORAGE_KEY_SETTINGS);
        if (!raw) return { defaultDirection: 'minimize' };
        try {
            return JSON.parse(raw);
        } catch (e) {
            return { defaultDirection: 'minimize' };
        }
    }
    function generateExperimentId() {
        return Date.now().toString(36) + Math.random().toString(36).substring(2, 6);
    }
    function createExperimentFromForm() {
        const nameInput = document.getElementById('expName');
        const objectiveInput = document.getElementById('objectiveName');
        const directionSelect = document.getElementById('direction');
        const paramRows = document.querySelectorAll('#paramsContainer .param-row');
        if (!nameInput.value.trim()) return null;
        if (!objectiveInput.value.trim()) return null;
        const parameters = [];
        for (const row of paramRows) {
            const nameEl = row.querySelector('.param-name');
            const typeEl = row.querySelector('.param-type');
            const rangeEl = row.querySelector('.param-range');
            if (!nameEl.value.trim() || !rangeEl.value.trim()) continue;
            parameters.push({
                name: nameEl.value.trim(),
                type: typeEl.value,
                range: rangeEl.value.trim()
            });
        }
        if (parameters.length === 0) return null;
        return {
            id: generateExperimentId(),
            name: nameInput.value.trim(),
            objective: objectiveInput.value.trim(),
            direction: directionSelect.value,
            status: 'idle',
            parameters: parameters,
            trials: []
        };
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
        populateExperimentsList();
        document.getElementById('experimentForm').reset();
        const container = document.getElementById('paramsContainer');
        container.innerHTML = '';
        addParameterRow();
    }
    function deleteExperimentById(id) {
        let experiments = loadExperimentsFromStorage();
        experiments = experiments.filter(exp => exp.id !== id);
        saveExperimentsToStorage(experiments);
        populateExperimentsList();
    }
    function populateExperimentsList() {
        const experiments = loadExperimentsFromStorage();
        const container = document.getElementById('experimentsList');
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
            const summary = document.createElement('summary');
            const nameSpan = document.createElement('span');
            nameSpan.textContent = exp.name;
            const statusSpan = document.createElement('span');
            statusSpan.className = 'status-badge';
            statusSpan.textContent = exp.status;
            summary.appendChild(nameSpan);
            summary.appendChild(statusSpan);
            const body = document.createElement('div');
            body.className = 'experiment-body';
            const table = document.createElement('table');
            const thead = document.createElement('thead');
            thead.innerHTML = '<tr><th>Parameter</th><th>Type</th><th>Range</th></tr>';
            table.appendChild(thead);
            const tbody = document.createElement('tbody');
            for (const param of exp.parameters) {
                const tr = document.createElement('tr');
                tr.innerHTML = `<td>${param.name}</td><td>${param.type}</td><td>${param.range}</td>`;
                tbody.appendChild(tr);
            }
            table.appendChild(tbody);
            body.appendChild(table);
            const objectiveP = document.createElement('p');
            objectiveP.innerHTML = `<strong>Objective:</strong> ${exp.objective} (${exp.direction})`;
            body.appendChild(objectiveP);
            const trialInfo = document.createElement('p');
            trialInfo.innerHTML = `<em>${exp.trials.length} trials recorded.</em>`;
            body.appendChild(trialInfo);
            const deleteButton = document.createElement('button');
            deleteButton.className = 'btn btn-danger';
            deleteButton.textContent = 'Delete';
            deleteButton.onclick = function(e) {
                e.stopPropagation();
                if (confirm('Delete experiment "' + exp.name + '"?')) {
                    deleteExperimentById(exp.id);
                }
            };
            body.appendChild(deleteButton);
            details.appendChild(summary);
            details.appendChild(body);
            container.appendChild(details);
        }
    }
    function addParameterRow() {
        const container = document.getElementById('paramsContainer');
        const row = document.createElement('div');
        row.className = 'form-row param-row';
        row.style.marginBottom = '0.5rem';
        row.innerHTML = `
            <div class="form-group">
                <label>Name</label>
                <input type="text" placeholder="param" class="param-name" required>
            </div>
            <div class="form-group">
                <label>Type</label>
                <select class="param-type" required>
                    <option value="float">float</option>
                    <option value="int">int</option>
                    <option value="categorical">categorical</option>
                </select>
            </div>
            <div class="form-group">
                <label>Range</label>
                <input type="text" placeholder="0, 1" class="param-range" required>
            </div>
        `;
        container.appendChild(row);
    }
    function initialiseApp() {
        document.getElementById('createExperimentButton').addEventListener('click', function(e) {
            e.preventDefault();
            addExperimentAndRefresh();
        });
        document.getElementById('addParamButton').addEventListener('click', function() {
            addParameterRow();
        });
        var defaultSettings = loadSettingsFromStorage();
        if (defaultSettings.defaultDirection) {
            var directionSelect = document.getElementById('direction');
            if (directionSelect) directionSelect.value = defaultSettings.defaultDirection;
        }
        populateExperimentsList();
    }
    document.addEventListener('DOMContentLoaded', initialiseApp);
})();