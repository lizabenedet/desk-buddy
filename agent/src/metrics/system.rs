//! CPU and RAM usage: always available, no admin needed.
//!
//! CPU comes from `GetSystemTimes`, not `sysinfo`: sysinfo reads the PDH
//! performance counters, which debloated Windows builds (e.g. AtlasOS)
//! disable, and then reports 0% forever.

use windows_sys::Win32::Foundation::FILETIME;
use windows_sys::Win32::System::Threading::GetSystemTimes;

use super::Source;
use crate::protocol::Metrics;

pub struct System {
    sys: sysinfo::System,
    last_cpu: Option<CpuTimes>,
}

impl System {
    pub fn new() -> System {
        // CPU usage is a delta between two samples; prime the first one.
        System { sys: sysinfo::System::new(), last_cpu: CpuTimes::now() }
    }
}

impl Source for System {
    fn name(&self) -> &'static str {
        "sistema"
    }

    fn sample(&mut self, out: &mut Metrics) -> Result<(), String> {
        let now = CpuTimes::now();
        if let (Some(prev), Some(cur)) = (self.last_cpu, now) {
            out.cpu = cpu_usage(prev, cur);
        }
        self.last_cpu = now;

        self.sys.refresh_memory();
        let total = self.sys.total_memory();
        if total > 0 {
            out.ram = Some(self.sys.used_memory() as f32 * 100.0 / total as f32);
        }
        Ok(())
    }
}

/// Cumulative system times in 100 ns ticks. `kernel` includes idle time.
#[derive(Debug, Clone, Copy, PartialEq)]
struct CpuTimes {
    idle: u64,
    kernel: u64,
    user: u64,
}

impl CpuTimes {
    fn now() -> Option<CpuTimes> {
        let zero = FILETIME { dwLowDateTime: 0, dwHighDateTime: 0 };
        let (mut idle, mut kernel, mut user) = (zero, zero, zero);
        // SAFETY: plain out-pointers to stack FILETIMEs.
        let ok = unsafe { GetSystemTimes(&mut idle, &mut kernel, &mut user) };
        (ok != 0).then(|| CpuTimes { idle: ticks(idle), kernel: ticks(kernel), user: ticks(user) })
    }
}

fn ticks(t: FILETIME) -> u64 {
    (u64::from(t.dwHighDateTime) << 32) | u64::from(t.dwLowDateTime)
}

/// Busy share of the time elapsed between two samples, 0–100.
fn cpu_usage(prev: CpuTimes, cur: CpuTimes) -> Option<f32> {
    let idle = cur.idle.checked_sub(prev.idle)?;
    let total = cur.kernel.checked_sub(prev.kernel)? + cur.user.checked_sub(prev.user)?;
    if total == 0 {
        return None;
    }
    Some((total.saturating_sub(idle) as f64 * 100.0 / total as f64) as f32)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(idle: u64, kernel: u64, user: u64) -> CpuTimes {
        CpuTimes { idle, kernel, user }
    }

    #[test]
    fn usage_is_busy_share_of_elapsed_time() {
        // 1000 ticks elapsed: kernel 800 (750 of it idle) + user 200 -> 250 busy.
        assert_eq!(cpu_usage(t(0, 0, 0), t(750, 800, 200)), Some(25.0));
        assert_eq!(cpu_usage(t(100, 100, 100), t(100, 600, 600)), Some(100.0));
        assert_eq!(cpu_usage(t(0, 0, 0), t(500, 500, 0)), Some(0.0));
    }

    #[test]
    fn no_elapsed_time_or_counter_going_back_gives_none() {
        assert_eq!(cpu_usage(t(5, 5, 5), t(5, 5, 5)), None);
        assert_eq!(cpu_usage(t(10, 10, 10), t(5, 20, 20)), None);
    }

    #[test]
    fn reads_real_counters() {
        let a = CpuTimes::now().expect("GetSystemTimes");
        std::thread::sleep(std::time::Duration::from_millis(200));
        let b = CpuTimes::now().expect("GetSystemTimes");
        let usage = cpu_usage(a, b).expect("time elapsed");
        assert!((0.0..=100.0).contains(&usage), "{usage}");
    }
}
