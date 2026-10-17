//! PC readings. Each `Source` fills the fields it knows; adding a new kind
//! of reading means writing another `Source` and listing it in `Collector::new`.

mod lhm;
mod system;

use crate::protocol::Metrics;

pub trait Source: Send {
    fn name(&self) -> &'static str;
    /// Fills the fields this source provides. Blocking is fine (runs off the
    /// async runtime); keep it well under the sampling interval.
    fn sample(&mut self, out: &mut Metrics) -> Result<(), String>;
}

/// Outcome of the last sampling round, per source, for the tray status.
#[derive(Debug, Clone, Default)]
pub struct SourceStatus {
    pub name: &'static str,
    pub error: Option<String>,
}

pub struct Collector {
    sources: Vec<Box<dyn Source>>,
}

impl Collector {
    pub fn new(lhm_url: &str) -> Collector {
        Collector {
            sources: vec![Box::new(system::System::new()), Box::new(lhm::Lhm::new(lhm_url))],
        }
    }

    /// Later sources only fill fields earlier ones left empty.
    pub fn sample(&mut self) -> (Metrics, Vec<SourceStatus>) {
        let mut merged = Metrics::default();
        let mut status = Vec::with_capacity(self.sources.len());
        for source in &mut self.sources {
            let mut m = Metrics::default();
            let error = source.sample(&mut m).err();
            merged.cpu = merged.cpu.or(m.cpu);
            merged.ram = merged.ram.or(m.ram);
            merged.gpu = merged.gpu.or(m.gpu);
            merged.cpu_temp = merged.cpu_temp.or(m.cpu_temp);
            merged.gpu_temp = merged.gpu_temp.or(m.gpu_temp);
            status.push(SourceStatus { name: source.name(), error });
        }
        (merged, status)
    }
}
