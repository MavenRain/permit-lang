//! G4: propose and execute of one approved mutation (g4-approve.lang).
//!
//! The plan executes m1 only in the approved branch of the propose of m1,
//! and judge permits the exact approved ticket one time. canon m is the
//! bytes of the target and the payload; the digest of an approval is
//! SHA-256 of canon m.
use sha2::{Digest, Sha256};

type Resource = Vec<usize>;
type Action = &'static str;
type Hash = [u8; 32];

const PATHS: [&str; 1] = ["prod"];

#[derive(Clone)]
struct Mutation {
    target: Resource,
    action: Action,
    payload: u64,
}

struct Ticket {
    mutation: Mutation,
    cost: u64,
    digest: Option<Hash>,
}

struct Authority {
    scope: Vec<(Resource, Action)>,
    steps: usize,
    left: u64,
    used: Vec<Hash>,
}

/// The plan is data: no eval row reads it.
#[allow(dead_code)]
enum Plan {
    Done,
    Propose(Mutation, Box<Plan>, Box<Plan>),
    Execute(Mutation, Box<Plan>, Box<Plan>, Box<Plan>),
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

fn canon(m: &Mutation) -> Vec<u8> {
    let target = m.target.iter().map(usize::to_string).collect::<Vec<_>>().join("/");
    format!("{target} {}", m.payload).into_bytes()
}

fn digest(m: &Mutation) -> Hash {
    Sha256::digest(canon(m)).into()
}

/// The authority after a decision: one step less, cost units less.
fn take(s: &Authority, cost: u64, used: Vec<Hash>) -> Authority {
    Authority { scope: s.scope.clone(), steps: s.steps.saturating_sub(1), left: s.left.saturating_sub(cost), used }
}

/// The denials in the kit order: scopeMiss, boundSpent, budgetShort,
/// noApproval, digestDiffers, approvalUsed. A permit of an execute adds
/// the digest to the used digests.
fn judge(s: &Authority, t: &Ticket) -> (&'static str, Authority) {
    let m = &t.mutation;
    let keep = || take(s, 0, s.used.clone());
    match () {
        _ if !allows(&s.scope, &m.target, m.action) => ("deny scopeMiss", keep()),
        _ if s.steps == 0 => ("deny boundSpent", keep()),
        _ if t.cost > s.left => ("deny budgetShort", keep()),
        _ if m.action != "execute" => ("permit", take(s, t.cost, s.used.clone())),
        _ => t.digest.map_or_else(
            || ("deny noApproval", keep()),
            |d| match () {
                _ if d != digest(m) => ("deny digestDiffers", keep()),
                _ if s.used.contains(&d) => ("deny approvalUsed", keep()),
                _ => ("permit", take(s, t.cost, [s.used.clone(), vec![d]].concat())),
            },
        ),
    }
}

fn main() {
    let prod = res("prod");
    let m1 = Mutation { target: prod.clone(), action: "execute", payload: 7 };
    let m2 = Mutation { target: prod.clone(), action: "execute", payload: 8 };
    let a0 = Authority { scope: vec![(prod, "execute")], steps: 2, left: 5, used: Vec::new() };
    let t_ok = Ticket { mutation: m1.clone(), cost: 2, digest: Some(digest(&m1)) };
    let t_changed = Ticket { mutation: m2, cost: 2, digest: Some(digest(&m1)) };
    let stop = || Box::new(Plan::Done);
    let _approve_plan = Plan::Propose(m1.clone(), Box::new(Plan::Execute(m1, stop(), stop(), stop())), stop());
    println!("{}", judge(&a0, &t_ok).0);
    println!("{}", judge(&judge(&a0, &t_ok).1, &t_ok).0);
    println!("{}", judge(&a0, &t_changed).0);
}
