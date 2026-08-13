function connect_wifi() {
    let ssid =
        document.getElementById("ssid").value;

    let password =
        document.getElementById("password").value;


    fetch("/wifi_config",
        {
            method: "POST",
            headers:
            {
                "Content-Type": "application/json"
            },
            body: JSON.stringify(
                {
                    ssid: ssid,
                    password: password
                })
        })
        .then(response => response.text())
        .then(data => {
            document.getElementById("msg").innerHTML = data;
        });
}



function scan_wifi() {

    fetch("/scan")
        .then(res => res.json())
        .then(data => {
            let list = document.getElementById("wifi_list");
            list.innerHTML = "";
            data.forEach(wifi => {
                let option = document.createElement("option");
                option.value = wifi.ssid;
                option.text =
                    wifi.ssid +
                    " (" +
                    wifi.rssi +
                    "dBm)";

                list.appendChild(option);

            });

        });

}



function select_wifi() {

    let list = document.getElementById("wifi_list");


    let ssid = list.options[list.selectedIndex].value;


    document.getElementById("ssid").value = ssid;

}

function factoryReset() {

    if (!confirm("确定恢复出厂设置吗？")) {
        return;
    }


    fetch("/factory_reset",
        {
            method: "GET"
        })
        .then(response => response.text())
        .then(data => {

            alert("恢复完成，请重新配置WiFi");


        })
        .catch(err => {
            console.log(err);

            alert("操作失败");

        });


}

