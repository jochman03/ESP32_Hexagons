async function loadConfig() {
    const res = await fetch("/getConfig.json");
    const data = await res.json();

    console.log(data);

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

loadConfig();
loadWifiStatus();