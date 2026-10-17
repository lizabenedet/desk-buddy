//! GPU load and CPU/GPU temperatures from LibreHardwareMonitor's web server
//! (Options -> Remote Web Server). LHM needs admin rights to read
//! temperatures; this agent doesn't.

use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::time::Duration;

use serde::Deserialize;

use super::Source;
use crate::protocol::Metrics;

const TIMEOUT: Duration = Duration::from_millis(800);

pub struct Lhm {
    url: String,
}

impl Lhm {
    pub fn new(url: &str) -> Lhm {
        Lhm { url: url.to_string() }
    }
}

impl Source for Lhm {
    fn name(&self) -> &'static str {
        "LibreHardwareMonitor"
    }

    fn sample(&mut self, out: &mut Metrics) -> Result<(), String> {
        let body = http_get(&self.url)?;
        let root: Node = serde_json::from_str(&body).map_err(|e| format!("JSON inválido: {e}"))?;
        let readings = read_sensors(&root);
        out.gpu = readings.gpu;
        out.cpu_temp = readings.cpu_temp;
        out.gpu_temp = readings.gpu_temp;
        Ok(())
    }
}

/// A node of LHM's data.json tree; sensors are the leaves with a SensorId.
#[derive(Debug, Deserialize)]
#[serde(rename_all = "PascalCase")]
struct Node {
    #[serde(default)]
    text: String,
    #[serde(default)]
    value: String,
    #[serde(default)]
    sensor_id: Option<String>,
    #[serde(default, rename = "Type")]
    kind: Option<String>,
    #[serde(default)]
    children: Vec<Node>,
}

#[derive(Debug, Default, PartialEq)]
struct Readings {
    gpu: Option<f32>,
    cpu_temp: Option<f32>,
    gpu_temp: Option<f32>,
}

fn read_sensors(root: &Node) -> Readings {
    let mut sensors = Vec::new();
    collect(root, &mut sensors);

    let is_cpu = |id: &str| id.starts_with("/amdcpu/") || id.starts_with("/intelcpu/");
    let is_gpu = |id: &str| id.starts_with("/gpu-");
    let of = |kind: &str, hw: &dyn Fn(&str) -> bool| -> Vec<&&Node> {
        sensors
            .iter()
            .filter(|n| n.kind.as_deref() == Some(kind) && hw(n.sensor_id.as_deref().unwrap_or("")))
            .collect()
    };

    let cpu_temps = of("Temperature", &is_cpu);
    let gpu_temps = of("Temperature", &is_gpu);
    let gpu_loads = of("Load", &is_gpu);

    // The package/die sensor is the one people mean by "CPU temperature";
    // otherwise take the hottest core.
    let cpu_temp = cpu_temps
        .iter()
        .find(|n| ["Tctl", "Tdie", "Package"].iter().any(|k| n.text.contains(k)))
        .and_then(|n| parse_value(&n.value))
        .or_else(|| cpu_temps.iter().filter_map(|n| parse_value(&n.value)).reduce(f32::max));

    let gpu_core = |list: &[&&Node]| {
        list.iter()
            .find(|n| n.text == "GPU Core")
            .or_else(|| list.first())
            .and_then(|n| parse_value(&n.value))
    };

    Readings { gpu: gpu_core(&gpu_loads), cpu_temp, gpu_temp: gpu_core(&gpu_temps) }
}

fn collect<'a>(node: &'a Node, out: &mut Vec<&'a Node>) {
    if node.sensor_id.is_some() {
        out.push(node);
    }
    for child in &node.children {
        collect(child, out);
    }
}

/// "55,0 °C" / "12.5 %" -> 55.0 / 12.5. LHM formats with the system locale.
fn parse_value(text: &str) -> Option<f32> {
    let number: String = text
        .trim()
        .chars()
        .take_while(|c| c.is_ascii_digit() || matches!(c, '.' | ',' | '-'))
        .map(|c| if c == ',' { '.' } else { c })
        .collect();
    number.parse().ok()
}

/// Plain HTTP/1.0 GET; LHM serves on localhost, so no TLS or chunking.
fn http_get(url: &str) -> Result<String, String> {
    let rest = url.strip_prefix("http://").ok_or("só http:// é suportado")?;
    let (host_port, path) = rest.split_once('/').map(|(h, p)| (h, format!("/{p}"))).unwrap_or((rest, "/".into()));
    let addr = (if host_port.contains(':') { host_port.to_string() } else { format!("{host_port}:80") })
        .to_socket_addrs()
        .map_err(|e| e.to_string())?
        .next()
        .ok_or("endereço não encontrado")?;

    let mut stream = TcpStream::connect_timeout(&addr, TIMEOUT).map_err(|_| "não está rodando".to_string())?;
    stream.set_read_timeout(Some(TIMEOUT)).ok();
    write!(stream, "GET {path} HTTP/1.0\r\nHost: {host_port}\r\n\r\n").map_err(|e| e.to_string())?;
    let mut response = Vec::new();
    stream.read_to_end(&mut response).map_err(|e| e.to_string())?;

    let response = String::from_utf8_lossy(&response);
    let (head, body) = response.split_once("\r\n\r\n").ok_or("resposta HTTP inválida")?;
    if !head.starts_with("HTTP/1.1 200") && !head.starts_with("HTTP/1.0 200") {
        return Err(head.lines().next().unwrap_or("").to_string());
    }
    Ok(body.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    // Trimmed data.json in LHM's shape (Ryzen + Radeon, pt-BR decimal commas).
    const SAMPLE: &str = r#"{"id":0,"Text":"Sensor","Value":"Value","ImageURL":"","Children":[
      {"id":1,"Text":"MEU-PC","Value":"","ImageURL":"images_icon/computer.png","Children":[
        {"id":2,"Text":"AMD Ryzen 7 5800X","Value":"","HardwareId":"/amdcpu/0","ImageURL":"images_icon/cpu.png","Children":[
          {"id":3,"Text":"Temperatures","Value":"","ImageURL":"images_icon/temperature.png","Children":[
            {"id":4,"Text":"Core #1","Value":"61,5 °C","SensorId":"/amdcpu/0/temperature/3","Type":"Temperature","Children":[]},
            {"id":5,"Text":"Core (Tctl/Tdie)","Value":"58,3 °C","SensorId":"/amdcpu/0/temperature/2","Type":"Temperature","Children":[]}
          ]},
          {"id":6,"Text":"Load","Value":"","Children":[
            {"id":7,"Text":"CPU Total","Value":"12,0 %","SensorId":"/amdcpu/0/load/0","Type":"Load","Children":[]}
          ]}
        ]},
        {"id":8,"Text":"AMD Radeon RX 6700 XT","Value":"","HardwareId":"/gpu-amd/0","ImageURL":"images_icon/ati.png","Children":[
          {"id":9,"Text":"Temperatures","Value":"","Children":[
            {"id":10,"Text":"GPU Hot Spot","Value":"71,0 °C","SensorId":"/gpu-amd/0/temperature/2","Type":"Temperature","Children":[]},
            {"id":11,"Text":"GPU Core","Value":"54,0 °C","SensorId":"/gpu-amd/0/temperature/0","Type":"Temperature","Children":[]}
          ]},
          {"id":12,"Text":"Load","Value":"","Children":[
            {"id":13,"Text":"GPU Memory","Value":"40,0 %","SensorId":"/gpu-amd/0/load/1","Type":"Load","Children":[]},
            {"id":14,"Text":"GPU Core","Value":"23,0 %","SensorId":"/gpu-amd/0/load/0","Type":"Load","Children":[]}
          ]}
        ]}
      ]}
    ]}"#;

    #[test]
    fn reads_the_right_sensors() {
        let root: Node = serde_json::from_str(SAMPLE).unwrap();
        assert_eq!(
            read_sensors(&root),
            Readings { gpu: Some(23.0), cpu_temp: Some(58.3), gpu_temp: Some(54.0) }
        );
    }

    #[test]
    fn falls_back_to_hottest_core_without_package_sensor() {
        let json = SAMPLE.replace("Core (Tctl/Tdie)", "Core #2");
        let root: Node = serde_json::from_str(&json).unwrap();
        assert_eq!(read_sensors(&root).cpu_temp, Some(61.5));
    }

    #[test]
    fn missing_hardware_gives_none() {
        let root: Node = serde_json::from_str(r#"{"Text":"Sensor","Children":[]}"#).unwrap();
        assert_eq!(read_sensors(&root), Readings::default());
    }

    #[test]
    fn parses_values_in_both_locales() {
        assert_eq!(parse_value("55,0 °C"), Some(55.0));
        assert_eq!(parse_value("12.5 %"), Some(12.5));
        assert_eq!(parse_value("-"), None);
        assert_eq!(parse_value(""), None);
    }

    #[test]
    fn not_running_is_a_clean_error() {
        // Nothing listens on port 1.
        assert!(http_get("http://127.0.0.1:1/data.json").is_err());
        assert!(http_get("https://x/").is_err());
    }
}
