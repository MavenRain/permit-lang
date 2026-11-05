//! G5: the demo of test/host/deploy.lang (g5-deploy.lang).
//!
//! The start authority: 4 grants on prod and dev, 6 steps and 10 units.
//! Each of the 7 tools of test/fakedeploy.c has an action and the RFC 6901
//! pointer to the target in its arguments. An execute needs an approval.
type Resource = Vec<usize>;
type Action = &'static str;

const PATHS: [&str; 2] = ["prod", "dev"];

/// The start authority is data: only its steps are read.
#[allow(dead_code)]
struct Authority {
    scope: Vec<(Resource, Action)>,
    steps: usize,
    left: u64,
    used: Vec<[u8; 32]>,
}

/// The chain of segment numbers, in the order of the first use.
fn res(path: &str) -> Resource {
    let segments = PATHS.iter().flat_map(|p| p.split('/')).fold(Vec::new(), |v, s| {
        if v.contains(&s) {
            v
        } else {
            [v, vec![s]].concat()
        }
    });
    path.split('/').filter_map(|s| segments.iter().position(|t| *t == s)).collect()
}

/// A constructor application in the langc normal form.
fn app(head: &str, args: &[String]) -> String {
    args.iter().fold(head.to_string(), |s, a| if a.contains(' ') { format!("{s} ({a})") } else { format!("{s} {a}") })
}

fn show_res(r: &[usize]) -> String {
    r.split_first().map_or(String::new(), |(root, rest)| {
        rest.iter().fold(app("resRoot", &[root.to_string()]), |s, seg| app("resChild", &[s, seg.to_string()]))
    })
}

fn show_size(k: usize) -> String {
    (0..k).fold("sz0".to_string(), |s, _| app("szS", &[s]))
}

fn main() {
    let prod = res("prod");
    let dev = res("dev");
    let sc = vec![(prod.clone(), "inspect"), (prod.clone(), "propose"), (prod.clone(), "execute"), (dev.clone(), "execute")];
    let start = Authority { scope: sc, steps: 6, left: 10, used: Vec::new() };
    let start_steps = start.steps;
    let _tools: [(&str, Action, &str); 7] = [
        ("inspect", "inspect", "/target"),
        ("propose", "propose", "/target"),
        ("execute", "execute", "/target"),
        ("sync_left", "execute", "/left"),
        ("sync_right", "execute", "/right"),
        ("restart", "execute", "/target"),
        ("delete", "execute", "/target"),
    ];
    println!("{}", show_res(&prod));
    println!("{}", show_res(&dev));
    println!("{}", show_size(start_steps));
}
