//! CPU and RAM usage through `sysinfo`: always available, no admin needed.

use super::Source;
use crate::protocol::Metrics;

pub struct System {
    sys: sysinfo::System,
}

impl System {
    pub fn new() -> System {
        let mut sys = sysinfo::System::new();
        // CPU usage is a delta between two refreshes; prime the first one.
        sys.refresh_cpu_usage();
        System { sys }
    }
}

impl Source for System {
    fn name(&self) -> &'static str {
        "sistema"
    }

    fn sample(&mut self, out: &mut Metrics) -> Result<(), String> {
        self.sys.refresh_cpu_usage();
        self.sys.refresh_memory();
        out.cpu = Some(self.sys.global_cpu_usage());
        let total = self.sys.total_memory();
        if total > 0 {
            out.ram = Some(self.sys.used_memory() as f32 * 100.0 / total as f32);
        }
        Ok(())
    }
}
