#pragma once

inline const char* WEB_UI_HTML = R"rawhtml(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Wii U Skylanders Portal Server</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --accent-color: #38bdf8;
            --accent-hover: #0284c7;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --occupied-bg: #164e63;
            --occupied-border: #06b6d4;
            --danger-bg: #ef4444;
            --danger-hover: #dc2626;
            --success-color: #22c55e;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); min-height: 100vh; padding: 20px; }
        .container { max-width: 1100px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; padding-bottom: 16px; border-bottom: 1px solid #334155; }
        h1 { font-size: 1.5rem; display: flex; align-items: center; gap: 10px; }
        .badge { background: #065f46; color: #6ee7b7; font-size: 0.75rem; padding: 4px 10px; border-radius: 9999px; }
        .status-dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; background: var(--success-color); }
        .grid-layout { display: grid; grid-template-columns: 1fr 2fr; gap: 24px; }
        @media (max-width: 850px) { .grid-layout { grid-template-columns: 1fr; } }
        .card { background: var(--card-bg); border-radius: 16px; padding: 20px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.3); border: 1px solid #334155; }
        .card h2 { font-size: 1.15rem; margin-bottom: 16px; color: var(--accent-color); border-bottom: 1px solid #334155; padding-bottom: 8px; }
        .form-group { margin-bottom: 16px; }
        label { display: block; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 6px; }
        select, input { width: 100%; padding: 10px 12px; background: #0f172a; border: 1px solid #475569; border-radius: 8px; color: #fff; font-size: 0.95rem; }
        select:focus, input:focus { outline: none; border-color: var(--accent-color); }
        .btn { width: 100%; padding: 12px; border: none; border-radius: 8px; font-size: 0.95rem; font-weight: 600; cursor: pointer; transition: 0.2s; }
        .btn-primary { background: var(--accent-color); color: #0f172a; }
        .btn-primary:hover { background: var(--accent-hover); color: #fff; }
        .btn-danger { background: var(--danger-bg); color: #fff; margin-top: 10px; }
        .btn-danger:hover { background: var(--danger-hover); }
        .slots-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; }
        .slot-card { background: #0f172a; border: 1px dashed #475569; border-radius: 12px; padding: 12px; display: flex; flex-direction: column; justify-content: space-between; min-height: 100px; transition: 0.2s; }
        .slot-card.occupied { border: 1px solid var(--occupied-border); background: var(--occupied-bg); }
        .slot-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; }
        .slot-number { font-size: 0.75rem; font-weight: 700; color: var(--text-muted); text-transform: uppercase; }
        .slot-name { font-size: 0.95rem; font-weight: 600; word-break: break-word; }
        .btn-sm-remove { background: rgba(239, 68, 68, 0.2); border: 1px solid rgba(239, 68, 68, 0.4); color: #fca5a5; font-size: 0.75rem; padding: 4px 8px; border-radius: 6px; cursor: pointer; align-self: flex-start; margin-top: 8px; }
        .btn-sm-remove:hover { background: rgba(239, 68, 68, 0.4); color: #fff; }
        #toast { position: fixed; bottom: 20px; right: 20px; padding: 12px 20px; border-radius: 8px; background: #334155; color: #fff; font-size: 0.9rem; opacity: 0; transition: 0.3s; pointer-events: none; z-index: 100; box-shadow: 0 10px 15px -3px rgba(0,0,0,0.5); }
        #toast.show { opacity: 1; transform: translateY(-5px); }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🌀 Wii U Skylanders Portal <span class="badge"><span class="status-dot"></span> Online</span></h1>
            <div id="portal-count" style="color: var(--text-muted); font-size: 0.9rem;">0/16 Ranuras</div>
        </header>

        <div class="grid-layout">
            <!-- Panel de Control -->
            <div class="card">
                <h2>⚡ Colocar Figura</h2>
                <div class="form-group">
                    <label for="skylander-search">Buscar Personaje</label>
                    <input type="text" id="skylander-search" placeholder="Escribe para filtrar...">
                </div>
                <div class="form-group">
                    <label for="skylander-select">Seleccionar Skylander</label>
                    <select id="skylander-select" size="6" style="height: 160px;"></select>
                </div>
                <div class="form-group">
                    <label for="slot-select">Ranura del Portal</label>
                    <select id="slot-select">
                        <!-- Generado por JS -->
                    </select>
                </div>
                <button class="btn btn-primary" id="btn-load">✨ Colocar en Portal</button>
                <button class="btn btn-danger" id="btn-clear">🧹 Limpiar Todo el Portal</button>
            </div>

            <!-- Ranuras del Portal -->
            <div class="card">
                <h2>🎮 Estado del Portal (16 Ranuras)</h2>
                <div class="slots-grid" id="slots-container">
                    <!-- Generado dinamicamente -->
                </div>
            </div>
        </div>
    </div>

    <div id="toast"></div>

    <script>
        let allSkylanders = [];

        function showToast(msg, isError = false) {
            const toast = document.getElementById('toast');
            toast.textContent = msg;
            toast.style.background = isError ? '#991b1b' : '#065f46';
            toast.classList.add('show');
            setTimeout(() => toast.classList.remove('show'), 2500);
        }

        async function fetchSkylanders() {
            try {
                const res = await fetch('/api/skylanders');
                allSkylanders = await res.json();
                renderSkylanderOptions(allSkylanders);
            } catch (err) {
                console.error("Error al cargar el catálogo de Skylanders:", err);
            }
        }

        function renderSkylanderOptions(list) {
            const select = document.getElementById('skylander-select');
            select.innerHTML = '';
            list.forEach(sky => {
                const opt = document.createElement('option');
                opt.value = JSON.stringify({ id: sky.id, variant: sky.variant, name: sky.name });
                opt.textContent = `${sky.name} (ID: ${sky.id}, Var: ${sky.variant})`;
                select.appendChild(opt);
            });
            if (list.length > 0) select.selectedIndex = 0;
        }

        document.getElementById('skylander-search').addEventListener('input', (e) => {
            const query = e.target.value.toLowerCase();
            const filtered = allSkylanders.filter(s => s.name.toLowerCase().includes(query));
            renderSkylanderOptions(filtered);
        });

        // Inicializar selector de ranuras
        const slotSelect = document.getElementById('slot-select');
        for (let i = 0; i < 16; i++) {
            const opt = document.createElement('option');
            opt.value = i;
            opt.textContent = `Ranura ${i + 1} (Slot ${i})`;
            slotSelect.appendChild(opt);
        }

        async function refreshStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();
                if (!data.success) return;

                const container = document.getElementById('slots-container');
                container.innerHTML = '';
                let count = 0;

                data.slots.forEach(slot => {
                    if (slot.occupied) count++;
                    const card = document.createElement('div');
                    card.className = `slot-card ${slot.occupied ? 'occupied' : ''}`;
                    card.innerHTML = `
                        <div>
                            <div class="slot-header">
                                <span class="slot-number">Ranura ${slot.slot + 1}</span>
                                ${slot.occupied ? '<span style="color: #38bdf8; font-size: 0.75rem;">● Activo</span>' : '<span style="color: #64748b; font-size: 0.75rem;">Vacío</span>'}
                            </div>
                            <div class="slot-name">${slot.occupied ? slot.name : 'Sin figura'}</div>
                        </div>
                        ${slot.occupied ? `<button class="btn-sm-remove" onclick="removeSlot(${slot.slot})">✕ Quitar</button>` : ''}
                    `;
                    container.appendChild(card);
                });

                document.getElementById('portal-count').textContent = `${count}/16 Ranuras en uso`;
            } catch (err) {
                console.error("Error al actualizar estado:", err);
            }
        }

        async function loadSelectedSkylander() {
            const select = document.getElementById('skylander-select');
            if (!select.value) return showToast("Selecciona un Skylander primero", true);
            const data = JSON.parse(select.value);
            const slot = parseInt(document.getElementById('slot-select').value);

            try {
                const res = await fetch('/api/load', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ id: data.id, variant: data.variant, name: data.name, slot: slot })
                });
                const result = await res.json();
                if (result.success) {
                    showToast(`✅ ${result.name} colocado en ranura ${slot + 1}`);
                    refreshStatus();
                } else {
                    showToast(`❌ Error: ${result.error || 'No se pudo cargar'}`, true);
                }
            } catch (err) {
                showToast("❌ Error de conexión", true);
            }
        }

        async function removeSlot(slot) {
            try {
                const res = await fetch('/api/remove', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ slot: slot })
                });
                const result = await res.json();
                if (result.success) {
                    showToast(`🗑️ Ranura ${slot + 1} retirada`);
                    refreshStatus();
                }
            } catch (err) {
                showToast("❌ Error al retirar", true);
            }
        }

        async function clearAll() {
            if (!confirm("¿Deseas quitar todas las figuras del portal?")) return;
            try {
                const res = await fetch('/api/clear', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({})
                });
                const result = await res.json();
                if (result.success) {
                    showToast("🧹 Portal limpiado");
                    refreshStatus();
                }
            } catch (err) {
                showToast("❌ Error al limpiar portal", true);
            }
        }

        document.getElementById('btn-load').addEventListener('click', loadSelectedSkylander);
        document.getElementById('btn-clear').addEventListener('click', clearAll);

        fetchSkylanders();
        refreshStatus();
        setInterval(refreshStatus, 2500);
    </script>
</body>
</html>
)rawhtml";
