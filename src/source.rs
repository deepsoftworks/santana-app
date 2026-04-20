use std::fs::File;
use std::io::{BufRead, BufReader};
use std::os::unix::io::FromRawFd;
use std::thread;

use crossbeam_channel::Sender;
use regex::Regex;

use crate::parser::{filter_fields, parse_line, ParsedRow};

/// Spawn a background thread that reads lines from `fd`, parses them,
/// and sends `Some(row)` for each valid line, then `None` on EOF.
pub fn spawn_reader(
    fd: i32,
    tx: Sender<Option<ParsedRow>>,
    filter: Option<Regex>,
) -> thread::JoinHandle<()> {
    thread::spawn(move || {
        // SAFETY: we own this fd; the main thread no longer touches it.
        let file = unsafe { File::from_raw_fd(fd) };
        let reader = BufReader::new(file);

        for line in reader.lines() {
            match line {
                Ok(l) => {
                    if let Some(row) = parse_line(&l) {
                        let row = filter_fields(row, filter.as_ref());
                        if !row.is_empty() {
                            let _ = tx.send(Some(row));
                        }
                    }
                }
                Err(_) => break,
            }
        }

        let _ = tx.send(None); // EOF sentinel
    })
}
