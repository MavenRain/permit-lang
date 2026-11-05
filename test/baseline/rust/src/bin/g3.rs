//! G3: an attenuated delegation to a sub-agent (g3-delegate.lang).
//!
//! delegate gives (child, rest) when the child scope is within the parent
//! scope, the child step bound is not longer and the budget covers the
//! cost. The child has the n units, and the rest keeps the parent scope,
//! the parent step bound and the remaining units.
type Resource = Vec<usize>;
type Action = &'static str;
type Scope = Vec<(Resource, Action)>;

const PATHS: [&str; 2] = ["docs", "docs/report"];

struct Cap {
    scope: Scope,
    steps: usize,
    budget: u64,
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

/// True when g is r or an ancestor of r.
fn within(r: &[usize], g: &[usize]) -> bool {
    r.starts_with(g)
}

fn allows(scope: &[(Resource, Action)], r: &[usize], a: Action) -> bool {
    scope.iter().any(|(g, b)| within(r, g) && *b == a)
}

/// True when t allows each grant of s.
fn scope_within(s: &[(Resource, Action)], t: &[(Resource, Action)]) -> bool {
    s.iter().all(|(g, b)| allows(t, g, b))
}

fn size_le(a: usize, b: usize) -> bool {
    a <= b
}

fn split(b: u64, n: u64) -> Option<(u64, u64)> {
    b.checked_sub(n).map(|r| (n, r))
}

fn delegate(c: &Cap, s: &Scope, k: usize, n: u64) -> Option<(Cap, Cap)> {
    (scope_within(s, &c.scope) && size_le(k, c.steps))
        .then(|| split(c.budget, n))
        .flatten()
        .map(|(b1, b2)| {
            (Cap { scope: s.clone(), steps: k, budget: b1 }, Cap { scope: c.scope.clone(), steps: c.steps, budget: b2 })
        })
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

/// The G3 scopes have one grant each: fmEta of the grant.
fn show_scope(scope: &[(Resource, Action)]) -> String {
    scope
        .iter()
        .map(|(g, b)| app("fmEta", &["Prod Resource Action".to_string(), app("pair", &[show_res(g), b.to_string()])]))
        .collect::<Vec<_>>()
        .join(" ")
}

fn show_cap(c: &Cap) -> String {
    app("cap", &[show_scope(&c.scope), show_size(c.steps), app("budget", &[c.budget.to_string()])])
}

fn main() {
    let docs = res("docs");
    let report = res("docs/report");
    let read_docs = vec![(docs, "inspect")];
    let read_report = vec![(report, "inspect")];
    let one = 1;
    let two = 2;
    let child = delegate(&Cap { scope: read_docs, steps: two, budget: 5 }, &read_report, one, 3);
    println!("{}", child.map_or("none".to_string(), |(c, r)| app("some", &[app("pair", &[show_cap(&c), show_cap(&r)])])));
    println!("{}", u8::from(size_le(one, two)));
    println!("{}", u8::from(size_le(two, one)));
}
