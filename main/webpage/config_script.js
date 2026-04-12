function loadConfig() {

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


updateDHCP();