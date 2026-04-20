use std::collections::VecDeque;

#[derive(Debug, Clone)]
pub struct RingBuffer {
    data:     VecDeque<f64>,
    capacity: usize,
    sum:      f64,
    count:    u64, // total ever pushed
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Stats {
    pub current: f64,
    pub min:     f64,
    pub max:     f64,
    pub mean:    f64,
    pub count:   u64,
}

impl RingBuffer {
    pub fn new(capacity: usize) -> Self {
        Self {
            data: VecDeque::with_capacity(capacity),
            capacity,
            sum: 0.0,
            count: 0,
        }
    }

    pub fn push(&mut self, value: f64) {
        if self.data.len() == self.capacity {
            // Fix vs C++: subtract evicted value so sum stays correct
            let evicted = self.data.pop_front().unwrap();
            self.sum -= evicted;
        }
        self.data.push_back(value);
        self.sum += value;
        self.count += 1;
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn stats(&self) -> Stats {
        if self.data.is_empty() {
            return Stats::default();
        }
        let current = *self.data.back().unwrap();
        let min = self.data.iter().cloned().fold(f64::INFINITY, f64::min);
        let max = self.data.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
        let mean = self.sum / self.data.len() as f64;
        Stats { current, min, max, mean, count: self.count }
    }

    /// Returns up to `zoom` samples ending `pan` samples from the right.
    pub fn snapshot_with_zoom(&self, zoom: usize, pan: usize) -> Vec<f64> {
        let len = self.data.len();
        let end = len.saturating_sub(pan);
        let start = end.saturating_sub(zoom);
        self.data.range(start..end).cloned().collect()
    }

    pub fn snapshot(&self) -> Vec<f64> {
        self.data.iter().cloned().collect()
    }

    pub fn resize(&mut self, new_cap: usize) {
        let snap: Vec<f64> = self.snapshot();
        self.capacity = new_cap;
        self.data.clear();
        self.sum = 0.0;
        for v in snap.iter().rev().take(new_cap).rev().cloned() {
            self.data.push_back(v);
            self.sum += v;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_eviction_sum() {
        let mut rb = RingBuffer::new(3);
        rb.push(1.0);
        rb.push(2.0);
        rb.push(3.0);
        rb.push(4.0); // evicts 1.0
        // sum should be 2+3+4=9
        assert!((rb.stats().mean - 3.0).abs() < 1e-9);
    }

    #[test]
    fn test_stats() {
        let mut rb = RingBuffer::new(10);
        for i in 1..=5 {
            rb.push(i as f64);
        }
        let s = rb.stats();
        assert_eq!(s.current, 5.0);
        assert_eq!(s.min, 1.0);
        assert_eq!(s.max, 5.0);
        assert!((s.mean - 3.0).abs() < 1e-9);
    }

    #[test]
    fn test_zoom_pan() {
        let mut rb = RingBuffer::new(10);
        for i in 0..10 {
            rb.push(i as f64);
        }
        let snap = rb.snapshot_with_zoom(3, 0);
        assert_eq!(snap, vec![7.0, 8.0, 9.0]);
        let snap2 = rb.snapshot_with_zoom(3, 2);
        assert_eq!(snap2, vec![5.0, 6.0, 7.0]);
    }
}
