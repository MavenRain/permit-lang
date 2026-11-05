//! G1: a read-only scope with a step bound (g1-scope.lang).
//!
//! The agent can inspect docs and each resource below it, one time. judge
//! gives the decision on a ticket and the authority after it. G1 has no
//! approvals, thus the payload (payloadOf 0) plays no part and an execute in
//! the scope is denied with noApproval.
type Resource = Vec<usize>;
type Action = &'static str;

const PATHS: [&str; 2] = ["docs", "docs/report"];

struct Authority {
    scope: Vec<(Resource, Action)>,
    steps: usize,
    left: u64,
}

struct Ticket {
    target: Resource,
    action: Action,
    cost: u64,
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

/// The authority after a decision: one step less and cost units less.
fn take(s: &Authority, cost: u64) -> Authority {
    Authority { scope: s.scope.clone(), steps: s.steps.saturating_sub(1), left: s.left.saturating_sub(cost) }
}

/// The denials in the kit order: scopeMiss, boundSpent, budgetShort.
fn judge(s: &Authority, t: &Ticket) -> (&'static str, Authority) {
    match () {
        _ if !allows(&s.scope, &t.target, t.action) => ("deny scopeMiss", take(s, 0)),
        _ if s.steps == 0 => ("deny boundSpent", take(s, 0)),
        _ if t.cost > s.left => ("deny budgetShort", take(s, 0)),
        _ if t.action == "execute" => ("deny noApproval", take(s, 0)),
        _ => ("permit", take(s, t.cost)),
    }
}

fn main() {
    let docs = res("docs");
    let report = res("docs/report");
    let a0 = Authority { scope: vec![(docs, "inspect")], steps: 1, left: 5 };
    let read_report = Ticket { target: report.clone(), action: "inspect", cost: 1 };
    let edit_report = Ticket { target: report, action: "execute", cost: 1 };
    println!("{}", judge(&a0, &read_report).0);
    println!("{}", judge(&a0, &edit_report).0);
    println!("{}", judge(&judge(&a0, &read_report).1, &read_report).0);
}
