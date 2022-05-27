use std::{
    fs,
    io::{self, Write},
    path::{Path, PathBuf},
};

use serialport::{SerialPortType, UsbPortInfo};

pub fn detect_serial_ports() -> Option<(String, UsbPortInfo)> {
    let ports = serialport::available_ports().expect("list available ports");
    let mut ans = Vec::new();
    for p in ports {
        if let SerialPortType::UsbPort(info) = p.port_type {
            if info.vid == 0x1a86 && info.pid == 0x7523 {
                // CH340
                ans.push((p.port_name, info));
            } else if info.vid == 0x0403 && info.pid == 0x6010 {
                // FT2232
                ans.push((p.port_name, info));
            }
        }
    }
    if ans.len() == 0 {
        return None;
    } else if ans.len() == 1 {
        return Some(ans[0].clone());
    } else {
        let mut name_list = String::new();
        for (i, (e, _)) in ans.iter().enumerate() {
            name_list += e;
            if i != ans.len() - 1 {
                name_list += ", ";
            }
        } // fixme: rewrite using Join trait
        let mut input = String::new();
        println!("xtask: multiple serial ports detected.");
        for (port_name, info) in ans.iter() {
            dump_port(port_name, info);
        }
        let stdin = io::stdin();
        let mut stdout = io::stdout();
        let (port_name, info) = 'outer: loop {
            print!("xtask: please select one port [{}]: ", name_list);
            stdout.flush().unwrap();
            stdin.read_line(&mut input).expect("read line");
            for (port_name, info) in ans.iter() {
                if input.trim().eq_ignore_ascii_case(port_name) {
                    break 'outer (port_name, info);
                }
            }
            eprintln!(
                "Input '{}' does not match to any ports! Please input again.",
                input.trim()
            );
            input.clear();
        };
        return Some((port_name.clone(), info.clone()));
    }
}

pub fn dump_port(port_name: &str, info: &UsbPortInfo) {
    print!(
        "Port {}: vid: {:x}, pid: {:x}",
        port_name, info.vid, info.pid
    );
    if let Some(serial_number) = &info.serial_number {
        print!(", serial number: {}", serial_number)
    }
    if let Some(manufacturer) = &info.manufacturer {
        print!(", manufacturer: {}", manufacturer)
    }
    if let Some(product) = &info.product {
        print!(", product: {}", product)
    }
    println!()
}

pub fn save_to_file(port_name: &str) {
    fs::create_dir_all(project_root().join("target").join("xtask")).expect("create folder");
    let mut file = fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .open(
            project_root()
                .join("target")
                .join("xtask")
                .join("serial-port.txt"),
        )
        .expect("create and open file");
    file.write(port_name.as_bytes()).expect("write file");
}

pub fn read_serial_port_choose_file() -> io::Result<String> {
    fs::read_to_string(
        project_root()
            .join("target")
            .join("xtask")
            .join("serial-port.txt"),
    )
}

fn project_root() -> PathBuf {
    Path::new(&env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(1)
        .unwrap()
        .to_path_buf()
}
