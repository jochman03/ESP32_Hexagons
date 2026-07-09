async function loadConfig() {
    const res = await fetch("/getConfig.json");
    const data = await res.json();

    // console.log(data);

    // AP
    let apMode = document.getElementById("apMode");
    let apSSID = document.getElementById("apSsid");
    let apPass = document.getElementById("apPassword");
    let apChannel = document.getElementById("apChannel");
    let apHidden = document.getElementById("apHidden");

    apMode.value = data.ap_mode;
    apSSID.value = data.ap_ssid;
    apPass.value = data.ap_pass;
    apChannel.value = data.ap_channel;
    if (data.ap_hidden == "1") {
        apHidden.checked = true;
    }
    else {
        apHidden.checked = false;
    }

    // STA
    let staSSID = document.getElementById("staSsid");
    let staPass = document.getElementById("staPassword");
    let staDHCP = document.getElementById("staDhcp");
    let staIP = document.getElementById("staIp");
    let staMask = document.getElementById("staMask");
    let staGateway = document.getElementById("staGateway");
    let staDns = document.getElementById("staDns");

    staSSID.value = data.sta_ssid;
    staPass.value = data.sta_pass;
    staIP.value = data.sta_ip;
    staMask.value = data.sta_mask;
    staGateway.value = data.sta_gateway;
    staDns.value = data.sta_dns;
    if (data.sta_dhcp == "1") {
        staDHCP.checked = true;
    }
    else {
        staDHCP.checked = false;
    }

    updateDHCP();

}

function updateDHCP() {
    let dhcpEnabled = document.getElementById("staDhcp");
    let staticPanel = document.getElementById("static-ip-panel");
    if (dhcpEnabled.checked) {
        staticPanel.style.display = 'none';
    } else {
        staticPanel.style.display = 'flex';
    }
}

async function saveAP() {
    let apMode = document.getElementById("apMode").value;
    let apSSID = document.getElementById("apSsid").value;
    let apPass = document.getElementById("apPassword").value;
    let apChannel = document.getElementById("apChannel").value;
    let apHidden = document.getElementById("apHidden").checked ? "1" : "0";

    const payload = {
        ap_mode: apMode,
        ap_ssid: apSSID,
        ap_pass: apPass,
        ap_channel: apChannel,
        ap_hidden: apHidden
    };

    const res = await fetch("/saveAPConfig", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    });
    if (!res.ok) {
        console.error("ESP error:", await res.text());
    } else {
        console.log("OK");
    }
}

async function saveSTA() {
    let staSSID = document.getElementById("staSsid").value;
    let staPass = document.getElementById("staPassword").value;
    let staDHCP = document.getElementById("staDhcp").checked ? "1" : "0";
    let staIP = document.getElementById("staIp").value;
    let staMask = document.getElementById("staMask").value;
    let staGateway = document.getElementById("staGateway").value;
    let staDns = document.getElementById("staDns").value;

    const payload = {
        sta_ssid: staSSID,
        sta_pass: staPass,
        sta_dhcp: staDHCP,
        sta_ip: staIP,
        sta_mask: staMask,
        sta_gateway: staGateway,
        sta_dns: staDns
    };

    const res = await fetch("/saveSTAConfig", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
        return;
    }

    console.log("STA config saved");
}

function showPass(id) {
    document.getElementById(id).type = "text";
}

function hidePass(id) {
    document.getElementById(id).type = "password";
}

async function loadWifiStatus() {
    const res = await fetch("/getWifiStatus.json");
    const data = await res.json();

    document.getElementById("staStatusConnected").textContent =
        data.sta_connected ? "Connected" : "Disconnected";

    document.getElementById("staStatusSSID").textContent = data.sta_ssid || "-";
    document.getElementById("staStatusIP").textContent = data.sta_ip || "-";
    document.getElementById("staStatusGateway").textContent = data.sta_gateway || "-";

    document.getElementById("staStatusSignal").textContent =
        data.sta_connected ? data.sta_signal + "dBm" : "-";
}

async function disconnectSTA() {
    const res = await fetch("/disconnectSTA", {
        method: "POST"
    });

    if (!res.ok) {
        console.error("ESP error:", await res.text());
        return;
    }

    console.log("STA disconnected");

    document.getElementById("staStatusConnected").textContent = "Disconnected";
    document.getElementById("staStatusSSID").textContent = "-";
    document.getElementById("staStatusIP").textContent = "-";
    document.getElementById("staStatusGateway").textContent = "-";
    document.getElementById("staStatusSignal").textContent = "-";

    document.getElementById("staSsid").value = "";
    document.getElementById("staPassword").value = "";
}

async function checkOTAStatus() {
    const res = await fetch("/OTAstatus", {
        method: "POST"
    });

    if (!res.ok) {
        console.error("OTA status failed:", await res.text());
        return;
    }

    const data = await res.json();

    console.log("OTA status:", data);

    if (data.ota_update_status === 1) {
        console.log("OTA successful. Device will restart soon.");
    } else if (data.ota_update_status === -1) {
        console.error("OTA failed");
    } else {
        console.log("OTA pending");
    }
}

async function loadOTAStatus() {
    const res = await fetch("/ota/status");

    if (!res.ok) {
        console.error("OTA status failed:", await res.text());
        return;
    }

    const data = await res.json();

    document.getElementById("otaCurrent").textContent =
        data.current_version + " build " + data.current_build;

    if (data.latest_version && data.latest_build > 0) {
        document.getElementById("otaLatest").textContent =
            data.latest_version + " build " + data.latest_build;
    } else {
        document.getElementById("otaLatest").textContent = "-";
    }

    document.getElementById("otaStatus").textContent = data.message;
    document.getElementById("otaProgress").textContent = data.progress + "%";

    document.getElementById("otaStartBtn").disabled =
        !data.update_available || data.update_running;
}

async function checkUpdate() {
    document.getElementById("otaStatus").textContent = "Checking...";

    const res = await fetch("/ota/check", {
        method: "POST"
    });

    if (!res.ok) {
        console.error("OTA check failed:", await res.text());
    }

    await loadOTAStatus();
}

async function startUpdate() {
    document.getElementById("otaStatus").textContent = "Starting update...";

    const res = await fetch("/ota/start", {
        method: "POST"
    });

    if (!res.ok) {
        console.error("OTA start failed:", await res.text());
    }

    await loadOTAStatus();
}

function clampEvery(value) {
    const n = Number(value);

    if (Number.isNaN(n)) {
        return 0;
    }

    if (n < 0) {
        return 0;
    }

    if (n > 60) {
        return 60;
    }

    return n;
}

async function loadMaintenanceConfig() {
    const res = await fetch("/maintenanceConfig.json");

    if (!res.ok) {
        console.error("Maintenance config load failed:", await res.text());
        return;
    }

    const cfg = await res.json();

    document.getElementById("wifiRetryEnabled").checked = cfg.wifi_retry_enabled == 1;
    document.getElementById("wifiRetryEvery").value = String(cfg.wifi_retry_every);
    document.getElementById("wifiRetryUnit").value = String(cfg.wifi_retry_unit);

    document.getElementById("autoUpdateEnabled").checked = cfg.auto_update_enabled == 1;
    document.getElementById("autoUpdateEvery").value = String(cfg.auto_update_every);
    document.getElementById("autoUpdateUnit").value = String(cfg.auto_update_unit);
}

async function saveMaintenanceConfig() {
    const payload = {
        wifi_retry_enabled: document.getElementById("wifiRetryEnabled").checked ? 1 : 0,
        wifi_retry_every: clampEvery(document.getElementById("wifiRetryEvery").value),
        wifi_retry_unit: Number(document.getElementById("wifiRetryUnit").value),

        auto_update_enabled: document.getElementById("autoUpdateEnabled").checked ? 1 : 0,
        auto_update_every: clampEvery(document.getElementById("autoUpdateEvery").value),
        auto_update_unit: Number(document.getElementById("autoUpdateUnit").value)
    };

    const res = await fetch("/setMaintenanceConfig", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(payload)
    });

    if (!res.ok) {
        console.error("Maintenance config save failed:", await res.text());
        return;
    }

    console.log("Maintenance config saved");
    await loadMaintenanceConfig();
}

function checkTimeInputCorrectness() {
    const wifi_retry_time_input = document.getElementById("wifiRetryEvery");
    const wifi_retry_unit_input = document.getElementById("wifiRetryUnit");
    const auto_update_time_input = document.getElementById("autoUpdateEvery");
    const auto_update_unit_input = document.getElementById("autoUpdateUnit");

    if (wifi_retry_unit_input.value == 0) {
        wifi_retry_time_input.value = Math.max(wifi_retry_time_input.value, 10);
    }
    if (auto_update_unit_input.value == 0) {
        auto_update_time_input.value = Math.max(auto_update_time_input.value, 10);
    }
}

setInterval(loadOTAStatus, 1000);

loadOTAStatus();
loadConfig();
loadWifiStatus();
loadMaintenanceConfig();